#ifndef REAL_JSON_TO_RIV_CONVERTER_HPP
#define REAL_JSON_TO_RIV_CONVERTER_HPP

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

// JSON parsing library
#include <nlohmann/json.hpp>

// Rive runtime includes
#include "rive/file.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/ellipse.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/keyframe_double.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/core/binary_writer.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include "rive/node.hpp"
#include "rive/transform_component.hpp"
#include "utils/no_op_factory.hpp"

using json = nlohmann::json;
using namespace rive;

struct KeyframeData {
    float time;
    float value;
    std::string interpolationType;
    bool easeIn = false;
    bool easeOut = false;
};

struct AnimationTrack {
    uint32_t objectId;
    std::string property;
    std::vector<KeyframeData> keyframes;
};

struct AnimationData {
    std::string name;
    float duration;
    float fps;
    bool loop;
    std::vector<AnimationTrack> tracks;
};

struct ColorData {
    std::string type;
    std::string color;
};

struct FillData {
    std::string type;
    std::string color;
};

struct StrokeData {
    std::string color;
    float thickness;
};

struct ObjectData {
    std::string type;
    std::string name;
    uint32_t id;
    float x, y, width, height;
    FillData fill;
    StrokeData stroke;
    bool hasFill = true;
    bool hasStroke = false;
};

struct ArtboardData {
    std::string name;
    float width, height;
    std::string backgroundColor;
    bool clip;
};

class RealJsonToRivConverter {
private:
    std::unique_ptr<NoOpFactory> m_factory;
    std::unique_ptr<File> m_file;
    std::unique_ptr<Artboard> m_artboard;
    std::unordered_map<uint32_t, Core*> m_objectMap;
    
    // Helper methods
    uint32_t parseColorHex(const std::string& hexColor);
    void createArtboard(const ArtboardData& data);
    Core* createShape(const ObjectData& objData);
    void createAnimation(const AnimationData& animData);
    uint16_t getPropertyKey(const std::string& propertyName);
    
public:
    RealJsonToRivConverter();
    ~RealJsonToRivConverter();
    
    // Main conversion method
    bool convertJsonToRiv(const std::string& jsonContent, const std::string& outputPath);
    
    // Individual parsing methods
    ArtboardData parseArtboard(const json& artboardJson);
    std::vector<ObjectData> parseObjects(const json& objectsJson);
    std::vector<AnimationData> parseAnimations(const json& animationsJson);
    
    // Export methods using proper Rive binary format
    std::vector<uint8_t> exportToRivBytes();
    bool saveToFile(const std::string& filepath);
};

#endif // REAL_JSON_TO_RIV_CONVERTER_HPP
