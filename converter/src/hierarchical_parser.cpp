#include "hierarchical_schema.hpp"
#include "json_loader.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <iostream>

namespace
{
uint32_t parse_color_string(const std::string& color_string, uint32_t fallback = 0xFFFFFFFF)
{
    if (color_string.empty())
    {
        return fallback;
    }

    std::string normalized = color_string;
    if (normalized[0] == '#')
    {
        normalized = normalized.substr(1);
    }
    else if (normalized.rfind("0x", 0) == 0 || normalized.rfind("0X", 0) == 0)
    {
        normalized = normalized.substr(2);
    }

    try
    {
        uint32_t value = std::stoul(normalized, nullptr, 16);
        if (normalized.length() <= 6)
        {
            value |= 0xFF000000u;
        }
        return value;
    }
    catch (const std::exception&)
    {
        return fallback;
    }
}
} // namespace

namespace rive_hierarchical
{

VertexData parse_vertex(const nlohmann::json& vJson)
{
    VertexData v;
    std::string typeStr = vJson.value("type", "straight");
    v.type = typeStr;
    v.x = vJson.value("x", 0.0f);
    v.y = vJson.value("y", 0.0f);
    v.radius = vJson.value("radius", 0.0f);
    
    // Cubic properties
    if (typeStr == "cubic")
    {
        v.inRotation = vJson.value("inRotation", 0.0f);
        v.inDistance = vJson.value("inDistance", 0.0f);
        v.outRotation = vJson.value("outRotation", 0.0f);
        v.outDistance = vJson.value("outDistance", 0.0f);
    }
    else if (typeStr == "cubicMirrored")
    {
        v.rotation = vJson.value("rotation", 0.0f);
        v.distance = vJson.value("distance", 0.0f);
    }
    
    return v;
}

PathData parse_path(const nlohmann::json& pathJson)
{
    PathData path;
    path.isClosed = pathJson.value("isClosed", true);
    
    if (pathJson.contains("vertices") && pathJson["vertices"].is_array())
    {
        for (const auto& vJson : pathJson["vertices"])
        {
            path.vertices.push_back(parse_vertex(vJson));
        }
    }
    
    return path;
}

GradientStopData parse_gradient_stop(const nlohmann::json& stopJson)
{
    GradientStopData stop;
    stop.position = stopJson.value("position", 0.0f);
    stop.color = stopJson.value("color", "#FFFFFF");
    return stop;
}

GradientData parse_gradient(const nlohmann::json& gradJson)
{
    GradientData gradient;
    gradient.type = gradJson.value("type", "linear");
    
    if (gradJson.contains("stops") && gradJson["stops"].is_array())
    {
        for (const auto& stopJson : gradJson["stops"])
        {
            gradient.stops.push_back(parse_gradient_stop(stopJson));
        }
    }
    
    return gradient;
}

FillData parse_fill(const nlohmann::json& fillJson)
{
    FillData fill;
    
    if (fillJson.value("hasGradient", false))
    {
        fill.hasGradient = true;
        if (fillJson.contains("gradient"))
        {
            fill.gradient = parse_gradient(fillJson["gradient"]);
        }
    }
    else
    {
        fill.solidColor = fillJson.value("solidColor", "#FFFFFF");
    }
    
    // Parse feather if present
    if (fillJson.contains("feather"))
    {
        const auto& featherJson = fillJson["feather"];
        fill.hasFeather = true;
        fill.feather.strength = featherJson.value("strength", 0.0f);
        fill.feather.offsetX = featherJson.value("offsetX", 0.0f);
        fill.feather.offsetY = featherJson.value("offsetY", 0.0f);
        fill.feather.inner = featherJson.value("inner", false);
    }
    
    return fill;
}

StrokeData parse_stroke(const nlohmann::json& strokeJson)
{
    StrokeData stroke;
    stroke.thickness = strokeJson.value("thickness", 1.0f);
    stroke.color = strokeJson.value("color", "#000000");
    return stroke;
}

HierarchicalShapeData parse_hierarchical_shape(const nlohmann::json& shapeJson)
{
    HierarchicalShapeData shape;
    
    // Basic properties
    shape.type = shapeJson.value("type", "custom");
    shape.x = shapeJson.value("x", 0.0f);
    shape.y = shapeJson.value("y", 0.0f);
    
    // Parametric shape properties
    if (shape.type == "rectangle" || shape.type == "ellipse")
    {
        shape.width = shapeJson.value("width", 100.0f);
        shape.height = shapeJson.value("height", 100.0f);
    }
    
    // Parse paths array (CRITICAL - can be 0-N paths per shape!)
    if (shapeJson.contains("paths") && shapeJson["paths"].is_array())
    {
        for (const auto& pathJson : shapeJson["paths"])
        {
            shape.paths.push_back(parse_path(pathJson));
        }
    }
    
    // Parse Fill
    if (shapeJson.value("hasFill", false))
    {
        shape.hasFill = true;
        if (shapeJson.contains("fill"))
        {
            shape.fill = parse_fill(shapeJson["fill"]);
        }
    }
    
    // Parse Stroke
    if (shapeJson.value("hasStroke", false))
    {
        shape.hasStroke = true;
        if (shapeJson.contains("stroke"))
        {
            shape.stroke = parse_stroke(shapeJson["stroke"]);
        }
    }
    
    return shape;
}

TextData parse_text(const nlohmann::json& textJson)
{
    TextData text;
    text.name = textJson.value("name", "Text");
    text.text = textJson.value("text", "");
    text.x = textJson.value("x", 0.0f);
    text.y = textJson.value("y", 0.0f);
    text.width = textJson.value("width", 200.0f);
    text.height = textJson.value("height", 100.0f);
    text.fontSize = textJson.value("fontSize", 24.0f);
    text.color = textJson.value("color", "#000000");
    text.fontFamily = textJson.value("fontFamily", "Arial");
    text.align = textJson.value("align", 0u);
    text.sizing = textJson.value("sizing", 0u);
    return text;
}

AnimationData parse_animation(const nlohmann::json& animJson)
{
    AnimationData anim;
    anim.name = animJson.value("name", "Animation");
    anim.fps = animJson.value("fps", 60u);
    anim.duration = animJson.value("duration", 60u);
    anim.loop = animJson.value("loop", 1u);
    return anim;
}

StateData parse_state(const nlohmann::json& stateJson)
{
    StateData state;
    state.name = stateJson.value("name", "State");
    state.type = stateJson.value("type", "animation");
    state.animationName = stateJson.value("animationName", "");
    return state;
}

LayerData parse_layer(const nlohmann::json& layerJson)
{
    LayerData layer;
    layer.name = layerJson.value("name", "Layer");
    
    if (layerJson.contains("states") && layerJson["states"].is_array())
    {
        for (const auto& stateJson : layerJson["states"])
        {
            layer.states.push_back(parse_state(stateJson));
        }
    }
    
    return layer;
}

StateMachineData parse_state_machine(const nlohmann::json& smJson)
{
    StateMachineData sm;
    sm.name = smJson.value("name", "StateMachine");
    
    if (smJson.contains("layers") && smJson["layers"].is_array())
    {
        for (const auto& layerJson : smJson["layers"])
        {
            sm.layers.push_back(parse_layer(layerJson));
        }
    }
    
    return sm;
}

ArtboardData parse_artboard(const nlohmann::json& artboardJson)
{
    ArtboardData artboard;
    artboard.name = artboardJson.value("name", "Artboard");
    artboard.width = artboardJson.value("width", 400.0f);
    artboard.height = artboardJson.value("height", 300.0f);
    
    // Parse hierarchical shapes
    if (artboardJson.contains("hierarchicalShapes") && artboardJson["hierarchicalShapes"].is_array())
    {
        std::cout << "Parsing " << artboardJson["hierarchicalShapes"].size() << " hierarchical shapes..." << std::endl;
        
        for (const auto& shapeJson : artboardJson["hierarchicalShapes"])
        {
            artboard.shapes.push_back(parse_hierarchical_shape(shapeJson));
        }
        
        std::cout << "Parsed " << artboard.shapes.size() << " shapes" << std::endl;
    }
    
    // Parse texts
    if (artboardJson.contains("texts") && artboardJson["texts"].is_array())
    {
        std::cout << "Parsing " << artboardJson["texts"].size() << " texts..." << std::endl;
        
        for (const auto& textJson : artboardJson["texts"])
        {
            artboard.texts.push_back(parse_text(textJson));
        }
        
        std::cout << "Parsed " << artboard.texts.size() << " texts" << std::endl;
    }
    
    // Parse animations
    if (artboardJson.contains("animations") && artboardJson["animations"].is_array())
    {
        for (const auto& animJson : artboardJson["animations"])
        {
            artboard.animations.push_back(parse_animation(animJson));
        }
    }
    
    // Parse state machines
    if (artboardJson.contains("stateMachines") && artboardJson["stateMachines"].is_array())
    {
        for (const auto& smJson : artboardJson["stateMachines"])
        {
            artboard.stateMachines.push_back(parse_state_machine(smJson));
        }
    }
    
    return artboard;
}

DocumentData parse_hierarchical_json(const std::string& json_content)
{
    DocumentData doc;
    auto json = nlohmann::json::parse(json_content);
    
    std::cout << "=== HIERARCHICAL PARSER ===" << std::endl;
    
    // Check for hierarchical format markers
    bool hasHierarchicalShapes = json.contains("hierarchicalShapes");
    bool hasArtboards = json.contains("artboards");
    
    if (hasHierarchicalShapes)
    {
        std::cout << "Detected hierarchical format (single artboard)" << std::endl;
        
        // Single artboard with hierarchical shapes
        ArtboardData artboard;
        artboard.name = json.value("name", "Artboard");
        artboard.width = json.value("width", 400.0f);
        artboard.height = json.value("height", 300.0f);
        
        // Parse hierarchical shapes
        if (json["hierarchicalShapes"].is_array())
        {
            std::cout << "Parsing " << json["hierarchicalShapes"].size() << " hierarchical shapes..." << std::endl;
            
            for (const auto& shapeJson : json["hierarchicalShapes"])
            {
                artboard.shapes.push_back(parse_hierarchical_shape(shapeJson));
            }
            
            std::cout << "Parsed " << artboard.shapes.size() << " shapes" << std::endl;
        }
        
        // Parse animations
        if (json.contains("animations") && json["animations"].is_array())
        {
            for (const auto& animJson : json["animations"])
            {
                artboard.animations.push_back(parse_animation(animJson));
            }
            std::cout << "Parsed " << artboard.animations.size() << " animations" << std::endl;
        }
        
        // Parse state machines
        if (json.contains("stateMachines") && json["stateMachines"].is_array())
        {
            for (const auto& smJson : json["stateMachines"])
            {
                artboard.stateMachines.push_back(parse_state_machine(smJson));
            }
            std::cout << "Parsed " << artboard.stateMachines.size() << " state machines" << std::endl;
        }
        
        // Parse bones
        if (json.contains("bones") && json["bones"].is_array())
        {
            for (const auto& boneJson : json["bones"])
            {
                BoneData bone;
                bone.type = boneJson.value("type", "bone");
                bone.x = boneJson.value("x", 0.0f);
                bone.y = boneJson.value("y", 0.0f);
                bone.length = boneJson.value("length", 0.0f);
                artboard.bones.push_back(bone);
            }
            std::cout << "Parsed " << artboard.bones.size() << " bones" << std::endl;
        }
        
        doc.artboards.push_back(artboard);
    }
    else if (hasArtboards)
    {
        std::cout << "Detected hierarchical format (multiple artboards)" << std::endl;
        
        for (const auto& artboardJson : json["artboards"])
        {
            doc.artboards.push_back(parse_artboard(artboardJson));
        }
    }
    else
    {
        std::cout << "WARNING: No hierarchical format detected!" << std::endl;
    }
    
    std::cout << "=== PARSE COMPLETE ===" << std::endl;
    std::cout << "Artboards: " << doc.artboards.size() << std::endl;
    if (!doc.artboards.empty())
    {
        std::cout << "Shapes in first artboard: " << doc.artboards[0].shapes.size() << std::endl;
    }
    
    return doc;
}

} // namespace rive_hierarchical
