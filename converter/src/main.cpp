#include "json_loader.hpp"
#include "hierarchical_schema.hpp"
#include "universal_builder.hpp"
#include "serializer.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

// Forward declaration
namespace rive_hierarchical {
    DocumentData parse_hierarchical_json(const std::string& json_content);
}

// Helper to parse color strings
static uint32_t parse_color_string(const std::string& colorStr, uint32_t fallback = 0xFFFFFFFF) {
    if (colorStr.empty() || colorStr[0] != '#') return fallback;
    
    std::string hex = colorStr.substr(1);
    try {
        uint32_t value = std::stoul(hex, nullptr, 16);
        if (hex.length() <= 6) {
            value |= 0xFF000000u; // Add alpha
        }
        return value;
    } catch (...) {
        return fallback;
    }
}

// Convert hierarchical format to converter format (EXACT COPY!)
rive_converter::Document convert_hierarchical_to_document(const rive_hierarchical::DocumentData& hierarchicalDoc)
{
    rive_converter::Document doc;
    
    for (const auto& hierarchicalArtboard : hierarchicalDoc.artboards)
    {
        rive_converter::ArtboardData artboard;
        artboard.name = hierarchicalArtboard.name;
        artboard.width = hierarchicalArtboard.width;
        artboard.height = hierarchicalArtboard.height;
        
        // DIRECTLY use hierarchical shapes (no conversion!)
        artboard.useHierarchical = true;
        artboard.hierarchicalShapes = hierarchicalArtboard.shapes;
        
        // Convert texts
        for (const auto& hierarchicalText : hierarchicalArtboard.texts)
        {
            rive_converter::TextData text;
            text.content = hierarchicalText.text;
            text.x = hierarchicalText.x;
            text.y = hierarchicalText.y;
            text.width = hierarchicalText.width;
            text.height = hierarchicalText.height;
            text.style.fontSize = hierarchicalText.fontSize;
            text.style.color = parse_color_string(hierarchicalText.color, 0xFF000000);
            text.style.fontFamily = hierarchicalText.fontFamily;
            text.align = hierarchicalText.align;
            text.sizing = hierarchicalText.sizing;
            artboard.texts.push_back(text);
        }
        
        // Convert animations
        for (const auto& hierarchicalAnim : hierarchicalArtboard.animations)
        {
            rive_converter::AnimationData anim;
            anim.name = hierarchicalAnim.name;
            anim.fps = hierarchicalAnim.fps;
            anim.duration = hierarchicalAnim.duration;
            anim.loop = hierarchicalAnim.loop;
            artboard.animations.push_back(anim);
        }
        
        // Convert state machines
        for (const auto& hierarchicalSM : hierarchicalArtboard.stateMachines)
        {
            rive_converter::StateMachineData sm;
            sm.name = hierarchicalSM.name;
            
            for (const auto& hierarchicalLayer : hierarchicalSM.layers)
            {
                rive_converter::LayerData layer;
                layer.name = hierarchicalLayer.name;
                
                for (const auto& hierarchicalState : hierarchicalLayer.states)
                {
                    rive_converter::StateData state;
                    state.name = hierarchicalState.name;
                    state.type = hierarchicalState.type;
                    state.animationName = hierarchicalState.animationName;
                    layer.states.push_back(state);
                }
                
                sm.layers.push_back(layer);
            }
            
            artboard.stateMachines.push_back(sm);
        }
        
        // Convert bones
        for (const auto& hierarchicalBone : hierarchicalArtboard.bones)
        {
            rive_converter::BoneData bone;
            bone.type = hierarchicalBone.type;
            bone.x = hierarchicalBone.x;
            bone.y = hierarchicalBone.y;
            bone.length = hierarchicalBone.length;
            artboard.bones.push_back(bone);
        }
        
        doc.artboards.push_back(artboard);
    }
    
    return doc;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: rive_convert [--exact] <input.json> <output.riv>" << std::endl;
        std::cerr << "Options:" << std::endl;
        std::cerr << "  --exact    Enable exact round-trip mode (requires __riv_exact__ in JSON)" << std::endl;
        return 1;
    }

    bool exactModeRequested = false;
    int argOffset = 1;
    
    // Check for --exact flag
    if (argc >= 4 && std::string(argv[1]) == "--exact")
    {
        exactModeRequested = true;
        argOffset = 2;
    }

    const std::string inputPath = argv[argOffset];
    const std::string outputPath = argv[argOffset + 1];

    std::ifstream inputFile(inputPath);
    if (!inputFile.is_open())
    {
        std::cerr << "Failed to open input file: " << inputPath << std::endl;
        return 1;
    }

    std::string jsonContent((std::istreambuf_iterator<char>(inputFile)),
                            std::istreambuf_iterator<char>());

    try
    {
        rive_converter::Document document;
        
        // Detect JSON format
        auto jsonParsed = nlohmann::json::parse(jsonContent);

        bool isExactUniversal = jsonParsed.contains("__riv_exact__") &&
                                 jsonParsed["__riv_exact__"].is_boolean() &&
                                 jsonParsed["__riv_exact__"].get<bool>();
        
        // Validate --exact flag usage
        if (exactModeRequested && !isExactUniversal)
        {
            std::cerr << "❌ Error: --exact flag requires JSON with __riv_exact__ = true" << std::endl;
            return 1;
        }
        
        // Warn if exact JSON used without --exact flag
        if (isExactUniversal && !exactModeRequested)
        {
            std::cerr << "⚠️  Warning: Exact mode JSON detected. Consider using --exact flag for clarity." << std::endl;
        }
        
        // Check for universal format (objects array with typeKey)
        bool isUniversal = isExactUniversal || (
            jsonParsed.contains("artboards") && 
            jsonParsed["artboards"].is_array() &&
            !jsonParsed["artboards"].empty() &&
            jsonParsed["artboards"][0].contains("objects") &&
            jsonParsed["artboards"][0]["objects"].is_array() &&
            !jsonParsed["artboards"][0]["objects"].empty() &&
            jsonParsed["artboards"][0]["objects"][0].contains("typeKey"));
        
        // Check for hierarchical format
        bool isHierarchical = jsonParsed.contains("hierarchicalShapes") || 
                             (jsonParsed.contains("artboards") && 
                              jsonParsed["artboards"].is_array() &&
                              !jsonParsed["artboards"].empty() &&
                              jsonParsed["artboards"][0].contains("hierarchicalShapes"));
        
        std::vector<uint8_t> bytes;
        
        if (isUniversal)
        {
            if (isExactUniversal)
            {
                std::cout << "🌟 Detected UNIVERSAL exact stream - performing raw serialization" << std::endl;
                bytes = rive_converter::serialize_exact_riv_json(jsonParsed);
            }
            else
            {
                std::cout << "🌟 Detected UNIVERSAL format - using universal builder!" << std::endl;
                rive_converter::PropertyTypeMap typeMap;
                auto coreDoc = rive_converter::build_from_universal_json(jsonParsed, typeMap);
                bytes = rive_converter::serialize_core_document(coreDoc, typeMap);
            }
        }
        else if (isHierarchical)
        {
            std::cout << "🎯 Detected HIERARCHICAL format - using exact copy pipeline!" << std::endl;
            auto hierarchicalDoc = rive_hierarchical::parse_hierarchical_json(jsonContent);
            document = convert_hierarchical_to_document(hierarchicalDoc);
            bytes = rive_converter::serialize_minimal_riv(document);
        }
        else
        {
            std::cout << "📝 Detected LEGACY format - using legacy pipeline" << std::endl;
            document = rive_converter::parse_json(jsonContent);
            bytes = rive_converter::serialize_minimal_riv(document);
        }

        std::ofstream outputFile(outputPath, std::ios::binary);
        outputFile.write(reinterpret_cast<const char*>(bytes.data()),
                         bytes.size());
        std::cout << "✅ Wrote RIV file: " << outputPath << " (" << bytes.size()
                  << " bytes)" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
