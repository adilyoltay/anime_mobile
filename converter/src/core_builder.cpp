#include "core_builder.hpp"
#include "rive/artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/ellipse.hpp"
#include "rive/shapes/triangle.hpp"
#include "rive/shapes/polygon.hpp"
#include "rive/shapes/star.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/animation/keyframe_double.hpp"
#include "rive/generated/animation/animation_base.hpp"
#include "rive/generated/animation/linear_animation_base.hpp"
#include "rive/generated/animation/keyed_object_base.hpp"
#include "rive/generated/animation/keyed_property_base.hpp"
#include "rive/generated/animation/keyframe_double_base.hpp"
#include "rive/generated/component_base.hpp"
#include "rive/generated/layout_component_base.hpp"
#include "rive/generated/node_base.hpp"
#include "rive/generated/shapes/rectangle_base.hpp"
#include "rive/generated/shapes/triangle_base.hpp"
#include "rive/generated/shapes/polygon_base.hpp"
#include "rive/generated/shapes/star_base.hpp"
#include "rive/generated/shapes/paint/fill_base.hpp"
#include "rive/generated/shapes/paint/solid_color_base.hpp"
#include "rive/generated/shapes/paint/stroke_base.hpp"
#include "rive/generated/transform_component_base.hpp"
#include "rive/generated/world_transform_component_base.hpp"
#include "rive/generated/drawable_base.hpp"
#include "rive/generated/shapes/parametric_path_base.hpp"
#include "rive/generated/shapes/paint/shape_paint_base.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_color_type.hpp"

namespace rive_converter
{
CoreBuilder::CoreBuilder() = default;

CoreObject& CoreBuilder::addCore(rive::Core* core)
{
    CoreObject object;
    object.core.reset(core);
    object.id = m_nextId++;
    m_objects.push_back(std::move(object));
    return m_objects.back();
}

void CoreBuilder::setParent(CoreObject& object, uint32_t parentId)
{
    object.parentId = parentId;
}

void CoreBuilder::set(CoreObject& object, uint16_t key, float value)
{
    object.properties.push_back({key, value});
}

void CoreBuilder::set(CoreObject& object, uint16_t key, uint32_t value)
{
    object.properties.push_back({key, value});
}

void CoreBuilder::set(CoreObject& object, uint16_t key, const std::string& value)
{
    object.properties.push_back({key, value});
}

void CoreBuilder::set(CoreObject& object, uint16_t key, bool value)
{
    object.properties.push_back({key, value});
}

CoreDocument CoreBuilder::build(PropertyTypeMap& typeMap)
{
    CoreDocument doc;
    doc.artboard = nullptr;

    for (auto& object : m_objects)
    {
        if (object.core && object.core->is<rive::Artboard>())
        {
            doc.artboard = object.core.get()->as<rive::Artboard>();
        }

        for (const auto& prop : object.properties)
        {
            uint8_t type = rive::CoreUintType::id;
            switch (prop.key)
            {
                case rive::LayoutComponentBase::widthPropertyKey:
                case rive::LayoutComponentBase::heightPropertyKey:
                case rive::NodeBase::xPropertyKey:
                case rive::NodeBase::yPropertyKey:
                case rive::ParametricPathBase::widthPropertyKey:
                case rive::ParametricPathBase::heightPropertyKey:
                case rive::StrokeBase::thicknessPropertyKey:
                case rive::TransformComponentBase::rotationPropertyKey:
                case rive::TransformComponentBase::scaleXPropertyKey:
                case rive::TransformComponentBase::scaleYPropertyKey:
                case rive::WorldTransformComponentBase::opacityPropertyKey:
                    type = rive::CoreDoubleType::id;
                    break;
                case rive::SolidColorBase::colorValuePropertyKey:
                    type = rive::CoreColorType::id;
                    break;
                case rive::ComponentBase::namePropertyKey:
                    type = rive::CoreStringType::id;
                    break;
                case rive::ShapePaintBase::isVisiblePropertyKey:
                case rive::LayoutComponentBase::clipPropertyKey:
                case rive::RectangleBase::linkCornerRadiusPropertyKey:
                case rive::PolygonBase::pointsPropertyKey:
                case 51: // KeyedObject::objectId
                case 53: // KeyedProperty::propertyKey
                case 67: // KeyFrame::frame
                    type = rive::CoreUintType::id;
                    break;
                case rive::PolygonBase::cornerRadiusPropertyKey:
                case rive::StarBase::innerRadiusPropertyKey:
                case 70: // KeyFrameDouble::value
                    type = rive::CoreDoubleType::id;
                    break;
                default:
                    if (std::holds_alternative<std::string>(prop.value))
                    {
                        type = rive::CoreStringType::id;
                    }
                    else if (std::holds_alternative<float>(prop.value))
                    {
                        type = rive::CoreDoubleType::id;
                    }
                    else if (prop.key == rive::ShapePaintBase::blendModeValuePropertyKey ||
                             prop.key == rive::ComponentBase::parentIdPropertyKey)
                    {
                        type = rive::CoreUintType::id;
                    }
                    break;
            }
            typeMap[prop.key] = type;
        }

        doc.objects.push_back({std::move(object.core), object.id, object.parentId,
                               std::move(object.properties)});
    }

    m_objects.clear();
    return doc;
}

CoreDocument build_core_document(const Document& document,
                                PropertyTypeMap& typeMap)
{
    CoreBuilder builder;

    auto& backboard = builder.addCore(new rive::Backboard());
    builder.set(backboard, static_cast<uint16_t>(44u), static_cast<uint32_t>(0));

    auto& artboard = builder.addCore(new rive::Artboard());
    builder.set(artboard, rive::ComponentBase::namePropertyKey,
                document.artboard.name);
    builder.set(artboard, rive::LayoutComponentBase::widthPropertyKey,
                document.artboard.width);
    builder.set(artboard, rive::LayoutComponentBase::heightPropertyKey,
                document.artboard.height);
    builder.set(artboard, rive::LayoutComponentBase::clipPropertyKey, true);

    // Store shape IDs for animation keyframe references
    std::vector<uint32_t> shapeIds;

    // Shapes disabled for Artboard-only variant
    for (const auto& shapeData : document.shapes)
    {
        auto& shape = builder.addCore(new rive::Shape());
        shapeIds.push_back(shape.id);
        builder.setParent(shape, artboard.id);
        builder.set(shape, rive::NodeBase::xPropertyKey, shapeData.x);
        builder.set(shape, rive::NodeBase::yPropertyKey, shapeData.y);
        // Transform defaults
        builder.set(shape, rive::TransformComponentBase::rotationPropertyKey, 0.0f);
        builder.set(shape, rive::TransformComponentBase::scaleXPropertyKey, 1.0f);
        builder.set(shape, rive::TransformComponentBase::scaleYPropertyKey, 1.0f);
        builder.set(shape, rive::WorldTransformComponentBase::opacityPropertyKey, 1.0f);

        switch (shapeData.type)
        {
            case ShapeType::rectangle:
            {
                auto& rectangle = builder.addCore(new rive::Rectangle());
                builder.setParent(rectangle, shape.id);
                builder.set(rectangle, rive::NodeBase::xPropertyKey, 0.0f);
                builder.set(rectangle, rive::NodeBase::yPropertyKey, 0.0f);
                builder.set(rectangle, rive::ParametricPathBase::widthPropertyKey,
                            shapeData.width);
                builder.set(rectangle, rive::ParametricPathBase::heightPropertyKey,
                            shapeData.height);
                builder.set(rectangle, rive::RectangleBase::linkCornerRadiusPropertyKey,
                            false);
                break;
            }
            case ShapeType::ellipse:
            {
                auto& ellipse = builder.addCore(new rive::Ellipse());
                builder.setParent(ellipse, shape.id);
                builder.set(ellipse, rive::NodeBase::xPropertyKey, 0.0f);
                builder.set(ellipse, rive::NodeBase::yPropertyKey, 0.0f);
                builder.set(ellipse, rive::ParametricPathBase::widthPropertyKey,
                            shapeData.width);
                builder.set(ellipse, rive::ParametricPathBase::heightPropertyKey,
                            shapeData.height);
                break;
            }
            case ShapeType::triangle:
            {
                auto& triangle = builder.addCore(new rive::Triangle());
                builder.setParent(triangle, shape.id);
                builder.set(triangle, rive::NodeBase::xPropertyKey, 0.0f);
                builder.set(triangle, rive::NodeBase::yPropertyKey, 0.0f);
                builder.set(triangle, rive::ParametricPathBase::widthPropertyKey,
                            shapeData.width);
                builder.set(triangle, rive::ParametricPathBase::heightPropertyKey,
                            shapeData.height);
                break;
            }
            case ShapeType::polygon:
            {
                auto& polygon = builder.addCore(new rive::Polygon());
                builder.setParent(polygon, shape.id);
                builder.set(polygon, rive::NodeBase::xPropertyKey, 0.0f);
                builder.set(polygon, rive::NodeBase::yPropertyKey, 0.0f);
                builder.set(polygon, rive::ParametricPathBase::widthPropertyKey,
                            shapeData.width);
                builder.set(polygon, rive::ParametricPathBase::heightPropertyKey,
                            shapeData.height);
                builder.set(polygon, rive::PolygonBase::pointsPropertyKey,
                            shapeData.points);
                builder.set(polygon, rive::PolygonBase::cornerRadiusPropertyKey,
                            shapeData.cornerRadius);
                break;
            }
            case ShapeType::star:
            {
                auto& star = builder.addCore(new rive::Star());
                builder.setParent(star, shape.id);
                builder.set(star, rive::NodeBase::xPropertyKey, 0.0f);
                builder.set(star, rive::NodeBase::yPropertyKey, 0.0f);
                builder.set(star, rive::ParametricPathBase::widthPropertyKey,
                            shapeData.width);
                builder.set(star, rive::ParametricPathBase::heightPropertyKey,
                            shapeData.height);
                builder.set(star, rive::PolygonBase::pointsPropertyKey,
                            shapeData.points);
                builder.set(star, rive::PolygonBase::cornerRadiusPropertyKey,
                            shapeData.cornerRadius);
                builder.set(star, rive::StarBase::innerRadiusPropertyKey,
                            shapeData.innerRadius);
                break;
            }
        }

        if (shapeData.fill.enabled)
        {
            auto& fill = builder.addCore(new rive::Fill());
            builder.setParent(fill, shape.id);
            builder.set(fill, rive::ShapePaintBase::isVisiblePropertyKey, true);

            auto& solid = builder.addCore(new rive::SolidColor());
            builder.setParent(solid, fill.id);
            builder.set(solid, rive::SolidColorBase::colorValuePropertyKey,
                        shapeData.fill.color);
        }

        if (shapeData.stroke.enabled)
        {
            auto& stroke = builder.addCore(new rive::Stroke());
            builder.setParent(stroke, shape.id);
            builder.set(stroke, rive::StrokeBase::thicknessPropertyKey,
                        shapeData.stroke.thickness);

            auto& solid = builder.addCore(new rive::SolidColor());
            builder.setParent(solid, stroke.id);
            builder.set(solid, rive::SolidColorBase::colorValuePropertyKey,
                        shapeData.stroke.color);
        }
    }

    // Build animations from JSON
    for (size_t animIdx = 0; animIdx < document.animations.size(); ++animIdx)
    {
        const auto& animData = document.animations[animIdx];
        auto& anim = builder.addCore(new rive::LinearAnimation());
        builder.set(anim, rive::AnimationBase::namePropertyKey, animData.name);
        builder.set(anim, rive::LinearAnimationBase::fpsPropertyKey, animData.fps);
        builder.set(anim, rive::LinearAnimationBase::durationPropertyKey, animData.duration);
        builder.set(anim, rive::LinearAnimationBase::loopValuePropertyKey, animData.loop);

        // For now, only support y-position animation on first shape
        if (!animData.yKeyframes.empty() && !shapeIds.empty())
        {
            // Calculate artboard-local index for the first shape
            // Artboard = 0, animations come next, then shapes
            uint32_t shapeLocalIndex = 1 + static_cast<uint32_t>(document.animations.size());
            
            auto& keyedObj = builder.addCore(new rive::KeyedObject());
            builder.set(keyedObj, static_cast<uint16_t>(51), shapeLocalIndex); // objectId (artboard-local)

            auto& keyedProp = builder.addCore(new rive::KeyedProperty());
            builder.set(keyedProp, static_cast<uint16_t>(53), static_cast<uint32_t>(14)); // propertyKey = y (14)

            for (const auto& kf : animData.yKeyframes)
            {
                auto& keyframe = builder.addCore(new rive::KeyFrameDouble());
                builder.set(keyframe, static_cast<uint16_t>(67), static_cast<uint32_t>(kf.frame)); // frame
                builder.set(keyframe, static_cast<uint16_t>(70), kf.value); // value
                builder.set(keyframe, static_cast<uint16_t>(68), static_cast<uint32_t>(1)); // interpolationType = 1 (cubic)
            }
        }
    }

    // Provide default animation if none specified
    if (document.animations.empty())
    {
        auto& animation = builder.addCore(new rive::LinearAnimation());
        builder.set(animation, rive::AnimationBase::namePropertyKey,
                    std::string("Default"));
        builder.set(animation, rive::LinearAnimationBase::fpsPropertyKey,
                    static_cast<uint32_t>(60));
        builder.set(animation, rive::LinearAnimationBase::durationPropertyKey,
                    static_cast<uint32_t>(60));
        builder.set(animation, rive::LinearAnimationBase::loopValuePropertyKey,
                    static_cast<uint32_t>(1));
    }

    return builder.build(typeMap);
}
} // namespace rive_converter
