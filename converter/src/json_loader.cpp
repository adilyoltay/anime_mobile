#include "json_loader.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace
{
uint32_t parse_color_string(const std::string& color_string,
                            uint32_t fallback)
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

rive_converter::ShapeType parse_shape_type(const std::string& type)
{
    std::string lowered = type;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowered == "ellipse" || lowered == "circle")
    {
        return rive_converter::ShapeType::ellipse;
    }
    else if (lowered == "triangle")
    {
        return rive_converter::ShapeType::triangle;
    }
    else if (lowered == "polygon")
    {
        return rive_converter::ShapeType::polygon;
    }
    else if (lowered == "star")
    {
        return rive_converter::ShapeType::star;
    }
    return rive_converter::ShapeType::rectangle;
}
} // namespace

namespace rive_converter
{
Document parse_json(const std::string& json_content)
{
    Document doc;
    auto json = nlohmann::json::parse(json_content);

    if (json.contains("artboard"))
    {
        const auto& artboard = json["artboard"];
        doc.artboard.name = artboard.value("name", doc.artboard.name);
        doc.artboard.width = artboard.value("width", doc.artboard.width);
        doc.artboard.height = artboard.value("height", doc.artboard.height);
    }

    auto parse_shape_array = [&](const nlohmann::json& shapes) {
        for (const auto& shape : shapes)
        {
            ShapeData data;
            data.type = parse_shape_type(shape.value("type", "rectangle"));
            data.x = shape.value("x", data.x);
            data.y = shape.value("y", data.y);
            data.width = shape.value("width", data.width);
            data.height = shape.value("height", data.height);
            data.points = shape.value("points", data.points);
            data.cornerRadius = shape.value("cornerRadius", data.cornerRadius);
            data.innerRadius = shape.value("innerRadius", data.innerRadius);

            // Parse gradient
            if (shape.contains("gradient"))
            {
                const auto& grad = shape["gradient"];
                data.fill.hasGradient = true;
                data.fill.enabled = true;
                data.fill.gradient.type = grad.value("type", std::string("radial"));
                
                if (grad.contains("stops"))
                {
                    for (const auto& stop : grad["stops"])
                    {
                        GradientStop gs;
                        gs.position = stop.value("position", 0.0f);
                        std::string color = stop.value("color", std::string("#FFFFFF"));
                        gs.color = parse_color_string(color, 0xFFFFFFFF);
                        data.fill.gradient.stops.push_back(gs);
                    }
                }
            }
            else if (shape.contains("fill"))
            {
                const auto& fill = shape["fill"];
                std::string color = fill.value("color", std::string());
                if (!color.empty())
                {
                    data.fill.color = parse_color_string(color, data.fill.color);
                    data.fill.enabled = true;
                }
            }
            else if (shape.contains("color"))
            {
                std::string color = shape["color"].get<std::string>();
                data.fill.color = parse_color_string(color, data.fill.color);
                data.fill.enabled = true;
            }

            if (shape.contains("stroke"))
            {
                const auto& stroke = shape["stroke"];
                data.stroke.enabled = true;
                data.stroke.thickness = stroke.value("thickness", data.stroke.thickness);
                std::string color = stroke.value("color", std::string());
                if (!color.empty())
                {
                    data.stroke.color =
                        parse_color_string(color, data.stroke.color);
                }
            }

            doc.shapes.push_back(data);
        }
    };

    if (json.contains("shapes"))
    {
        parse_shape_array(json["shapes"]);
    }
    if (json.contains("rectangles"))
    {
        parse_shape_array(json["rectangles"]);
    }

    // Parse animations
    if (json.contains("animations"))
    {
        for (const auto& anim : json["animations"])
        {
            AnimationData animData;
            animData.name = anim.value("name", animData.name);
            animData.fps = anim.value("fps", animData.fps);
            animData.duration = anim.value("duration", animData.duration);
            animData.loop = anim.value("loop", animData.loop);
            
            if (anim.contains("yKeyframes"))
            {
                for (const auto& kf : anim["yKeyframes"])
                {
                    KeyFrameData keyframe;
                    keyframe.frame = kf.value("frame", keyframe.frame);
                    keyframe.value = kf.value("value", keyframe.value);
                    animData.yKeyframes.push_back(keyframe);
                }
            }
            
            if (anim.contains("scaleKeyframes"))
            {
                for (const auto& kf : anim["scaleKeyframes"])
                {
                    KeyFrameData keyframe;
                    keyframe.frame = kf.value("frame", keyframe.frame);
                    keyframe.value = kf.value("value", keyframe.value);
                    animData.scaleKeyframes.push_back(keyframe);
                }
            }
            
            if (anim.contains("opacityKeyframes"))
            {
                for (const auto& kf : anim["opacityKeyframes"])
                {
                    KeyFrameData keyframe;
                    keyframe.frame = kf.value("frame", keyframe.frame);
                    keyframe.value = kf.value("value", keyframe.value);
                    animData.opacityKeyframes.push_back(keyframe);
                }
            }
            
            doc.animations.push_back(animData);
        }
    }

    return doc;
}
} // namespace rive_converter
