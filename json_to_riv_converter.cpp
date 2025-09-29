#include "json_to_riv_converter.hpp"
#include "utils/no_op_factory.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

JsonToRivConverter::JsonToRivConverter() {
    // Initialize factory - using NoOpFactory for basic operations
    m_factory = std::make_unique<NoOpFactory>();
}

JsonToRivConverter::~JsonToRivConverter() = default;

uint32_t JsonToRivConverter::parseColorHex(const std::string& hexColor) {
    // Convert hex color string to RGBA (e.g., "#FF0000" -> 0xFFFF0000)
    std::string hex = hexColor;
    if (hex[0] == '#') {
        hex = hex.substr(1);
    }
    
    uint32_t color = 0xFF000000; // Default alpha = 255
    
    if (hex.length() == 6) {
        // RGB format
        std::stringstream ss;
        ss << std::hex << hex;
        uint32_t rgb;
        ss >> rgb;
        color = 0xFF000000 | rgb; // Add full alpha
    } else if (hex.length() == 8) {
        // RGBA format  
        std::stringstream ss;
        ss << std::hex << hex;
        ss >> color;
    }
    
    return color;
}

ArtboardData JsonToRivConverter::parseArtboard(const json& artboardJson) {
    ArtboardData data;
    data.name = artboardJson.value("name", "Artboard");
    data.width = artboardJson.value("width", 400.0f);
    data.height = artboardJson.value("height", 400.0f);
    data.backgroundColor = artboardJson.value("backgroundColor", "#FFFFFF");
    data.clip = artboardJson.value("clip", true);
    return data;
}

std::vector<ObjectData> JsonToRivConverter::parseObjects(const json& objectsJson) {
    std::vector<ObjectData> objects;
    
    for (const auto& obj : objectsJson) {
        ObjectData objData;
        objData.type = obj.value("type", "rectangle");
        objData.name = obj.value("name", "Object");
        objData.id = obj.value("id", 0);
        objData.x = obj.value("x", 0.0f);
        objData.y = obj.value("y", 0.0f);
        objData.width = obj.value("width", 100.0f);
        objData.height = obj.value("height", 100.0f);
        
        // Parse fill
        if (obj.contains("fill")) {
            const auto& fillJson = obj["fill"];
            objData.fill.type = fillJson.value("type", "solid");
            objData.fill.color = fillJson.value("color", "#FFFFFF");
            objData.hasFill = true;
        }
        
        // Parse stroke
        if (obj.contains("stroke")) {
            const auto& strokeJson = obj["stroke"];
            objData.stroke.color = strokeJson.value("color", "#000000");
            objData.stroke.thickness = strokeJson.value("thickness", 1.0f);
            objData.hasStroke = true;
        }
        
        objects.push_back(objData);
    }
    
    return objects;
}

std::vector<AnimationData> JsonToRivConverter::parseAnimations(const json& animationsJson) {
    std::vector<AnimationData> animations;
    
    for (const auto& anim : animationsJson) {
        AnimationData animData;
        animData.name = anim.value("name", "Animation");
        animData.duration = anim.value("duration", 1.0f);
        animData.fps = anim.value("fps", 60.0f);
        animData.loop = anim.value("loop", true);
        
        // Parse keyframes
        if (anim.contains("keyframes")) {
            for (const auto& track : anim["keyframes"]) {
                AnimationTrack animTrack;
                animTrack.objectId = track.value("objectId", 0);
                animTrack.property = track.value("property", "x");
                
                if (track.contains("keyframes")) {
                    for (const auto& kf : track["keyframes"]) {
                        KeyframeData kfData;
                        kfData.time = kf.value("time", 0.0f);
                        kfData.value = kf.value("value", 0.0f);
                        kfData.interpolationType = kf.value("interpolationType", "linear");
                        kfData.easeIn = kf.value("easeIn", false);
                        kfData.easeOut = kf.value("easeOut", false);
                        
                        animTrack.keyframes.push_back(kfData);
                    }
                }
                
                animData.tracks.push_back(animTrack);
            }
        }
        
        animations.push_back(animData);
    }
    
    return animations;
}

void JsonToRivConverter::createArtboard(const ArtboardData& data) {
    m_artboard = std::make_unique<Artboard>();
    m_artboard->name(data.name);
    m_artboard->width(data.width);
    m_artboard->height(data.height);
    // Position is handled by transform component, not directly on artboard
    
    if (data.clip) {
        m_artboard->clip(true);
    }
}

Core* JsonToRivConverter::createShape(const ObjectData& objData) {
    Core* shape = nullptr;
    
    if (objData.type == "rectangle") {
        auto rect = new Rectangle();
        rect->width(objData.width);
        rect->height(objData.height);
        rect->cornerRadiusTL(0);
        rect->cornerRadiusTR(0);
        rect->cornerRadiusBL(0);
        rect->cornerRadiusBR(0);
        shape = rect;
    } else if (objData.type == "ellipse") {
        auto ellipse = new Ellipse();
        ellipse->width(objData.width);
        ellipse->height(objData.height);
        shape = ellipse;
    }
    
    if (shape) {
        // Set transform properties
        auto transformComponent = shape->as<TransformComponent>();
        if (transformComponent) {
            // Use property setters instead of direct function calls
            // These need to be set via the property system
        }
        
        // Set name if available
        if (shape->is<Component>()) {
            shape->as<Component>()->name(objData.name);
        }
        
        // Store in object map for animation references
        m_objectMap[objData.id] = shape;
        
        // Add fill if specified
        if (objData.hasFill) {
            auto fill = new Fill();
            auto solidColor = new SolidColor();
            solidColor->colorValue(parseColorHex(objData.fill.color));
            // TODO: wire up paint assignment once proper API is available
            
            if (shape->is<Shape>()) {
                // Add fill to shape (this is simplified - actual implementation would be more complex)
            }
        }
        
        // Add stroke if specified  
        if (objData.hasStroke) {
            auto stroke = new Stroke();
            stroke->thickness(objData.stroke.thickness);
            auto solidColor = new SolidColor();
            solidColor->colorValue(parseColorHex(objData.stroke.color));
            // TODO: wire up paint assignment once proper API is available
            
            if (shape->is<Shape>()) {
                // Add stroke to shape (this is simplified)
            }
        }
    }
    
    return shape;
}

uint16_t JsonToRivConverter::getPropertyKey(const std::string& propertyName) {
    // Map property names to Rive property keys
    // This is a simplified mapping - actual implementation would need complete mapping
    // Property keys need to be looked up from the actual component type
    if (propertyName == "x") return 14; // X property key (this would need to be looked up properly)
    if (propertyName == "y") return 15; // Y property key (this would need to be looked up properly)
    if (propertyName == "scaleX") return TransformComponent::scaleXPropertyKey;
    if (propertyName == "scaleY") return TransformComponent::scaleYPropertyKey;
    if (propertyName == "rotation") return TransformComponent::rotationPropertyKey;
    if (propertyName == "opacity") return Node::opacityPropertyKey;
    
    return 0; // Unknown property
}

void JsonToRivConverter::createAnimation(const AnimationData& animData) {
    auto animation = new LinearAnimation();
    animation->name(animData.name);
    animation->duration(static_cast<int>(animData.duration * animData.fps)); // Convert to frames
    animation->fps(static_cast<int>(animData.fps));
    // Set loop property using the correct API
    animation->loopValue(animData.loop ? 1 : 0); // 1 = loop, 0 = oneShot
    
    // Create keyframes for each track
    for (const auto& track : animData.tracks) {
        auto targetObject = m_objectMap.find(track.objectId);
        if (targetObject == m_objectMap.end()) {
            std::cout << "Warning: Object with ID " << track.objectId << " not found for animation\n";
            continue;
        }
        
        uint16_t propertyKey = getPropertyKey(track.property);
        if (propertyKey == 0) {
            std::cout << "Warning: Unknown property '" << track.property << "'\n";
            continue;
        }
        
        // Create KeyedObject for this object
        auto keyedObject = new KeyedObject();
        keyedObject->objectId(track.objectId);
        
        // Create KeyedProperty for this property
        auto keyedProperty = new KeyedProperty();
        keyedProperty->propertyKey(propertyKey);
        
        // Add keyframes
        for (const auto& kfData : track.keyframes) {
            auto keyframe = new KeyFrame();
            keyframe->frame(static_cast<int>(kfData.time * animData.fps));
            
            // Set interpolation type
            if (kfData.interpolationType == "linear") {
                // Linear interpolation - this would need proper keyframe type
            } else if (kfData.interpolationType == "cubic") {
                // Cubic interpolation - this would need proper keyframe type
            }
            
            // This is simplified - actual keyframe value setting would depend on property type
            // keyframe->value(kfData.value);
            
            // Add keyframe to property
            // keyedProperty->addKeyframe(keyframe);
        }
        
        // Add property to object
        // keyedObject->addKeyedProperty(keyedProperty);
        
        // Add keyed object to animation
        // animation->addKeyedObject(keyedObject);
    }
    
    // Add animation to artboard
    if (m_artboard) {
        // m_artboard->addAnimation(animation);
    }
}

bool JsonToRivConverter::convertJsonToRiv(const std::string& jsonContent, const std::string& outputPath) {
    try {
        // Parse JSON
        json jsonData = json::parse(jsonContent);
        
        // Create File
        m_file = std::make_unique<File>(m_factory.get(), nullptr);
        
        // Parse and create artboard
        if (jsonData.contains("artboard")) {
            auto artboardData = parseArtboard(jsonData["artboard"]);
            createArtboard(artboardData);
        }
        
        // Parse and create objects
        if (jsonData.contains("objects")) {
            auto objects = parseObjects(jsonData["objects"]);
            for (const auto& objData : objects) {
                auto shape = createShape(objData);
                if (shape && m_artboard) {
                    // Add shape to artboard
                    // This requires more complex parent-child relationship setup
                    // m_artboard->addObject(shape);
                }
            }
        }
        
        // Parse and create animations
        if (jsonData.contains("animations")) {
            auto animations = parseAnimations(jsonData["animations"]);
            for (const auto& animData : animations) {
                createAnimation(animData);
            }
        }
        
        // Export to file
        return saveToFile(outputPath);
        
    } catch (const json::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Conversion error: " << e.what() << std::endl;
        return false;
    }
}

std::vector<uint8_t> JsonToRivConverter::exportToBytes() {
    // This is a simplified export - actual implementation would need proper binary writing
    std::vector<uint8_t> bytes;
    
    if (!m_file || !m_artboard) {
        return bytes;
    }
    
    // Write header
    const char* signature = "RIVE";
    bytes.insert(bytes.end(), signature, signature + 4);
    
    // Write version (simplified)
    bytes.push_back(7); // Major version
    bytes.push_back(0); // Minor version
    
    // Write file ID
    bytes.push_back(0); bytes.push_back(0); bytes.push_back(0); bytes.push_back(0);
    
    // Write artboard data (this would be much more complex in reality)
    // For now, just write a placeholder
    bytes.push_back(0x01); // Artboard type marker
    
    return bytes;
}

bool JsonToRivConverter::saveToFile(const std::string& filepath) {
    auto bytes = exportToBytes();
    
    if (bytes.empty()) {
        return false;
    }
    
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    file.close();
    
    return true;
}
