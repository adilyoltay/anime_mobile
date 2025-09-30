#pragma once
// HIERARCHICAL JSON SCHEMA - Matches RIV's true structure
// This enables TRUE exact copy of any RIV file

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace rive_hierarchical
{

// Gradient Stop (child of Gradient)
struct GradientStopData {
    float position = 0.0f;
    std::string color = "#FFFFFF";
};

// Gradient (child of Fill)
struct GradientData {
    std::string type = "linear"; // or "radial"
    std::vector<GradientStopData> stops;
};

// Feather (child of Fill)
struct FeatherData {
    float strength = 0.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    bool inner = false;
};

// Fill (child of Shape)
struct FillData {
    bool hasGradient = false;
    GradientData gradient;
    std::string solidColor = "#FFFFFF";
    bool hasFeather = false;
    FeatherData feather;
};

// Stroke (child of Shape)
struct StrokeData {
    float thickness = 1.0f;
    std::string color = "#000000";
};

// Path Vertex
struct VertexData {
    std::string type = "straight"; // "straight", "cubic", "cubicMirrored"
    float x = 0.0f;
    float y = 0.0f;
    float radius = 0.0f;
    // CubicDetached properties
    float inRotation = 0.0f;
    float inDistance = 0.0f;
    float outRotation = 0.0f;
    float outDistance = 0.0f;
    // CubicMirrored properties
    float rotation = 0.0f;
    float distance = 0.0f;
};

// PointsPath (child of Shape) - can be multiple per Shape!
struct PathData {
    bool isClosed = true;
    std::vector<VertexData> vertices;
};

// Node container (layout/grouping)
struct NodeData {
    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float opacity = 1.0f;
};

// HIERARCHICAL SHAPE - Contains everything!
struct HierarchicalShapeData {
    std::string type = "custom"; // "custom", "rectangle", "ellipse", "node"
    float x = 0.0f;
    float y = 0.0f;
    
    // For parametric shapes (rectangle, ellipse)
    float width = 100.0f;
    float height = 100.0f;
    
    // For custom shapes: MULTIPLE paths!
    std::vector<PathData> paths;
    
    // Paint (can have both fill and stroke)
    bool hasFill = false;
    FillData fill;
    bool hasStroke = false;
    StrokeData stroke;
};

// Rest remains same
struct AnimationData {
    std::string name;
    uint32_t fps = 60;
    uint32_t duration = 60;
    uint32_t loop = 1;
};

struct StateData {
    std::string name;
    std::string type; // "entry", "exit", "any", "animation"
    std::string animationName;
};

struct LayerData {
    std::string name;
    std::vector<StateData> states;
};

struct StateMachineData {
    std::string name;
    std::vector<LayerData> layers;
};

struct BoneData {
    std::string type = "bone"; // "bone", "root"
    float x = 0.0f;
    float y = 0.0f;
    float length = 0.0f;
};

struct TextData {
    std::string name;
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    float width = 200.0f;
    float height = 100.0f;
    float fontSize = 24.0f;
    std::string color = "#000000";
    std::string fontFamily = "Arial";
    uint32_t align = 0; // 0=left, 1=center, 2=right
    uint32_t sizing = 0; // 0=auto, 1=fixed
};

struct ArtboardData {
    std::string name;
    float width;
    float height;
    
    // HIERARCHICAL shapes (each can have multiple paths!)
    std::vector<HierarchicalShapeData> shapes;
    std::vector<TextData> texts;
    
    std::vector<AnimationData> animations;
    std::vector<StateMachineData> stateMachines;
    std::vector<BoneData> bones;
};

struct DocumentData {
    std::vector<ArtboardData> artboards;
};

} // namespace rive_hierarchical
