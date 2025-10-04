#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>
#include "rive/core.hpp"
#include "hierarchical_schema.hpp"

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
    path,
    pointsPath  // Custom path with vertices
};

enum class VertexType
{
    straight,  // Line segment with optional corner radius
    cubic      // Bezier curve with control points
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

struct PathVertexData
{
    VertexType type = VertexType::straight;
    float x = 0.0f;
    float y = 0.0f;
    float radius = 0.0f;  // For StraightVertex
    // For CubicDetachedVertex
    float inRotation = 0.0f;
    float inDistance = 0.0f;
    float outRotation = 0.0f;
    float outDistance = 0.0f;
};

struct CustomPathData
{
    bool isClosed = true;
    std::vector<PathVertexData> vertices;
    // Paint data for paths (WITH GRADIENT SUPPORT!)
    bool fillEnabled = false;
    bool hasGradient = false;
    GradientData gradient;  // For gradients
    uint32_t fillColor = 0xFFFFFFFF;  // For solid colors
    bool strokeEnabled = false;
    uint32_t strokeColor = 0xFF000000;
    float strokeThickness = 1.0f;
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

struct DataBindData
{
    uint16_t typeKey = 447; // DataBindContext by default (supports sourcePathIds)
    std::optional<uint32_t> localId;
    std::optional<uint32_t> targetLocalId;
    std::optional<std::string> targetName;
    std::vector<std::string> targetPathNames;
    uint32_t propertyKey = 0;
    uint32_t flags = 0;
    int32_t converterId = -1;
    std::optional<uint32_t> converterLocalId;
    std::vector<uint16_t> sourcePathIds;
    bool toSource = false;
};

struct DataConverterData
{
    uint16_t typeKey = 0;
    std::optional<uint32_t> localId;
    std::string name;
    uint32_t interpolationType = 1;
    int32_t interpolatorId = -1;
    uint32_t flags = 0;
    float minInput = 1.0f;
    float maxInput = 1.0f;
    float minOutput = 1.0f;
    float maxOutput = 1.0f;
    uint32_t decimals = 0;
    std::string colorFormat;
    std::vector<DataBindData> contexts;
};

struct StateMachineListenerActionData
{
    uint16_t typeKey = 0;
    std::optional<uint32_t> localId;
    std::optional<uint32_t> inputLocalId;
    std::optional<std::string> inputName;
    std::optional<uint32_t> nestedInputLocalId;
    std::optional<std::string> nestedInputName;
    float value = 0.0f;
    std::vector<DataBindData> dataBinds;
};

struct StateMachineListenerData
{
    uint16_t typeKey = 114;
    std::optional<uint32_t> localId;
    std::string name;
    std::optional<uint32_t> targetLocalId;
    std::optional<std::string> targetName;
    uint32_t listenerTypeValue = 0;
    int32_t eventLocalId = -1;
    std::optional<std::string> eventName;
    std::vector<uint16_t> viewModelPathIds;
    std::vector<std::string> viewModelPathNames;
    std::vector<StateMachineListenerActionData> actions;
    std::vector<DataBindData> dataBinds;
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

enum class KeyFrameValueType
{
    unknown,
    doubleValue,
    colorValue,
    boolValue,
    uintValue,
    idValue,
    stringValue
};

struct KeyFrameData
{
    uint32_t frame = 0;
    float value = 0.0f; // Legacy convenience field for double keyframes
    KeyFrameValueType valueType = KeyFrameValueType::unknown;
    uint32_t colorValue = 0;
    uint32_t uintValue = 0;
    uint32_t idValue = 0;
    bool boolValue = false;
    std::string stringValue;
    uint32_t interpolationType = 1;
    std::optional<uint32_t> interpolatorId;
    nlohmann::json rawValue;
    nlohmann::json customData; // curve/easing payloads captured verbatim
};

struct KeyedPropertyData
{
    uint32_t propertyKey = 0;
    std::vector<KeyFrameData> keyframes;
};

struct KeyedObjectData
{
    uint32_t objectId = 0;
    std::optional<std::string> objectName;
    std::optional<uint32_t> componentIndex; // Diagnostic index from exact export
    std::vector<KeyedPropertyData> keyedProperties;
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
    std::optional<float> speed;
    std::optional<uint32_t> workStart;
    std::optional<uint32_t> workEnd;
    std::optional<bool> enableWorkArea;
    std::optional<bool> quantize;
    std::vector<KeyedObjectData> keyedObjects;
    bool hasHierarchicalData = false;
    std::vector<nlohmann::json> interpolators; // Raw interpolator payloads for deferred creation
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
    std::vector<StateMachineListenerData> listeners;
    std::vector<DataBindData> dataBinds;
};

struct ConstraintData
{
    std::string type = "ik"; // ik, transform, distance, rotation, scale
    uint32_t targetId = 0;
    float strength = 1.0f;
};

struct EventData
{
    std::string name = "Event";
    std::string type = "general"; // "general" or "audio"
    uint32_t assetId = 0; // For audio events
};

struct BoneData
{
    std::string name = "Bone";
    std::string type = "bone"; // "bone", "root"
    float x = 0.0f;
    float y = 0.0f;
    float length = 100.0f;
};

struct SkinData
{
    float xx = 1.0f;
    float yx = 0.0f;
    float xy = 0.0f;
    float yy = 1.0f;
    float tx = 0.0f;
    float ty = 0.0f;
};

// ArtboardData must come after all the types it references
struct ArtboardData
{
    std::string name = "Artboard";
    float width = 400.0f;
    float height = 300.0f;
    // Each artboard has its own content
    std::vector<ShapeData> shapes;
    std::vector<CustomPathData> customPaths;  // Custom vector paths
    std::vector<TextData> texts;
    std::vector<AnimationData> animations;
    std::vector<StateMachineData> stateMachines;
    std::vector<DataConverterData> dataConverters;
    std::vector<DataBindData> dataBinds;
    std::vector<ConstraintData> constraints;
    std::vector<EventData> events;  // NEW: Events and audio
    std::vector<BoneData> bones;    // NEW: Skeletal rigging
    std::vector<SkinData> skins;    // NEW: Mesh deformation
    
    // NEW: Hierarchical shapes (for exact copy)
    std::vector<rive_hierarchical::HierarchicalShapeData> hierarchicalShapes;
    bool useHierarchical = false; // Flag to use hierarchical builder
};

struct Document
{
    std::vector<ArtboardData> artboards; // Support multiple artboards
    
    // Legacy fields (DEPRECATED - kept for backwards compatibility)
    // These are moved to artboards[0] after parsing
    std::vector<ShapeData> shapes;
    std::vector<CustomPathData> customPaths;
    std::vector<TextData> texts;
    std::vector<AnimationData> animations;
    std::vector<StateMachineData> stateMachines;
    std::vector<DataConverterData> dataConverters;
    std::vector<DataBindData> dataBinds;
    std::vector<ConstraintData> constraints;
    std::vector<EventData> events;
    std::vector<BoneData> bones;
    std::vector<SkinData> skins;
};

KeyFrameData parse_keyframe_json(const nlohmann::json& keyframeJson);
KeyedPropertyData parse_keyed_property_json(const nlohmann::json& propertyJson);
KeyedObjectData parse_keyed_object_json(const nlohmann::json& keyedObjectJson);
AnimationData parse_animation_json(const nlohmann::json& animJson);

Document parse_json(const std::string& json_content);
} // namespace rive_converter
