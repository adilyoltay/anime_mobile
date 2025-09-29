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
    star
};

struct ShapePaint
{
    bool enabled = false;
    uint32_t color = 0xFFFFFFFF;
};

struct ShapeStroke
{
    bool enabled = false;
    uint32_t color = 0xFF000000;
    float thickness = 1.0f;
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

struct Document
{
    ArtboardData artboard;
    std::vector<ShapeData> shapes;
    std::vector<AnimationData> animations;
};

Document parse_json(const std::string& json_content);
} // namespace rive_converter
