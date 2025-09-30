#pragma once
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "rive/core.hpp"

namespace rive_converter
{
enum class ShapeType
{
    rectangle,
    ellipse,
    triangle,
    polygon,
    star,
    image,
    clipping,
    path
};

struct GradientStop
{
    float position = 0.0f;
    uint32_t color = 0xFFFFFFFF;
};

struct GradientData
{
    std::string type = "radial"; // "radial" or "linear"
    std::vector<GradientStop> stops;
};

struct DashData
{
    bool enabled = false;
    float length = 10.0f;
    float gap = 10.0f;
    bool lengthIsPercentage = false;
};

struct TrimPathData
{
    bool enabled = false;
    float start = 0.0f;
    float end = 1.0f;
    float offset = 0.0f;
    uint32_t mode = 0; // 0=sequential, 1=synced
};

struct FeatherData
{
    bool enabled = false;
    float strength = 12.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    bool inner = false;
};

struct ShapePaint
{
    bool enabled = false;
    uint32_t color = 0xFFFFFFFF;
    bool hasGradient = false;
    GradientData gradient;
    TrimPathData trimPath;
    FeatherData feather;
};

struct ShapeStroke
{
    bool enabled = false;
    uint32_t color = 0xFF000000;
    float thickness = 1.0f;
    DashData dash;
    TrimPathData trimPath;
};

struct ShapeData
{
    ShapeType type = ShapeType::rectangle;
    float x = 0.0f;
    float y = 0.0f;
    float width = 100.0f;
    float height = 100.0f;
    // Polygon/Star specific
    uint32_t points = 5;
    float cornerRadius = 0.0f;
    float innerRadius = 0.5f; // Star only
    // Image specific
    uint32_t assetId = 0;
    float originX = 0.5f;
    float originY = 0.5f;
    // ClippingShape specific
    uint32_t sourceId = 0; // ID of shape to use as clip source
    uint32_t fillRule = 0; // 0=nonZero, 1=evenOdd
    bool clipVisible = true;
    ShapePaint fill;
    ShapeStroke stroke;
};

struct ArtboardData
{
    std::string name = "Artboard";
    float width = 400.0f;
    float height = 300.0f;
};

struct KeyFrameData
{
    uint32_t frame = 0;
    float value = 0.0f;
};

struct TextStyleData
{
    std::string fontFamily = "Inter";
    float fontSize = 24.0f;
    uint32_t fontWeight = 400; // 100-900 (400=normal, 700=bold)
    float fontWidth = 100.0f;  // Font width/stretch (50-200, 100=normal)
    float fontSlant = 0.0f;    // Font slant/italic (-15 to 15 degrees)
    float lineHeight = -1.0f;  // -1 = auto
    float letterSpacing = 0.0f;
    uint32_t color = 0xFF000000; // Text fill color
    bool hasStroke = false;
    uint32_t strokeColor = 0xFF000000; // Text stroke/outline color
    float strokeThickness = 1.0f;
};

struct TextData
{
    std::string content = "";
    float x = 0.0f;
    float y = 0.0f;
    float width = 200.0f;
    float height = 100.0f;
    uint32_t align = 0; // 0=left, 1=center, 2=right
    uint32_t sizing = 0; // 0=auto, 1=fixed
    uint32_t overflow = 0; // 0=visible, 1=hidden, 2=clipped
    uint32_t wrap = 0; // 0=off, 1=on
    uint32_t verticalAlign = 0; // 0=top, 1=middle, 2=bottom
    float paragraphSpacing = 0.0f;
    bool fitFromBaseline = true;
    TextStyleData style;
};

struct AnimationData
{
    std::string name = "Animation";
    uint32_t fps = 60;
    uint32_t duration = 60;
    uint32_t loop = 1;
    std::vector<KeyFrameData> yKeyframes;
    std::vector<KeyFrameData> scaleKeyframes;
    std::vector<KeyFrameData> opacityKeyframes;
};

struct StateMachineInputData
{
    std::string name = "";
    std::string type = "bool"; // bool, number, trigger
    float defaultValue = 0.0f;
};

struct StateData
{
    std::string name = "State";
    std::string type = "entry"; // entry, animation, exit, any
    std::string animationName = ""; // For animation states
};

struct TransitionConditionData
{
    std::string input = ""; // Input name
    std::string op = "=="; // ==, !=, <, <=, >, >=
    float value = 0.0f;
};

struct TransitionData
{
    std::string from = ""; // Source state name
    std::string to = ""; // Target state name
    uint32_t duration = 300; // Transition duration in ms
    std::vector<TransitionConditionData> conditions;
};

struct LayerData
{
    std::string name = "Layer";
    std::vector<StateData> states;
    std::vector<TransitionData> transitions;
};

struct StateMachineData
{
    std::string name = "StateMachine";
    std::vector<StateMachineInputData> inputs;
    std::vector<LayerData> layers;
};

struct ConstraintData
{
    std::string type = "ik"; // ik, transform, distance, rotation, scale
    uint32_t targetId = 0;
    float strength = 1.0f;
};

struct Document
{
    ArtboardData artboard;
    std::vector<ShapeData> shapes;
    std::vector<TextData> texts;
    std::vector<AnimationData> animations;
    std::vector<StateMachineData> stateMachines;
    std::vector<ConstraintData> constraints;
};

Document parse_json(const std::string& json_content);
} // namespace rive_converter
