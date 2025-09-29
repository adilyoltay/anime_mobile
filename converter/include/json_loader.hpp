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
    ellipse
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
    ShapePaint fill;
    ShapeStroke stroke;
};

struct ArtboardData
{
    std::string name = "Artboard";
    float width = 400.0f;
    float height = 300.0f;
};

struct Document
{
    ArtboardData artboard;
    std::vector<ShapeData> shapes;
};

Document parse_json(const std::string& json_content);
} // namespace rive_converter
