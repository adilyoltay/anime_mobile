#include "core_builder.hpp"
#include "rive/artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/ellipse.hpp"
#include "rive/shapes/triangle.hpp"
#include "rive/shapes/polygon.hpp"
#include "rive/shapes/star.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/shapes/image.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/paint/radial_gradient.hpp"
#include "rive/shapes/paint/linear_gradient.hpp"
#include "rive/shapes/paint/gradient_stop.hpp"
#include "rive/shapes/paint/dash.hpp"
#include "rive/shapes/paint/dash_path.hpp"
#include "rive/shapes/paint/trim_path.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/animation/keyframe_double.hpp"
// Text support (full)
#include "rive/text/text.hpp"
#include "rive/text/text_style.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/generated/text/text_base.hpp"
#include "rive/generated/text/text_style_base.hpp"
#include "rive/generated/text/text_value_run_base.hpp"
// State machine support (skeleton)  
#include "rive/animation/state_machine.hpp"
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
#include "rive/generated/shapes/clipping_shape_base.hpp"
#include "rive/generated/shapes/image_base.hpp"
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
                case 38: // GradientStop::colorValue
                    type = rive::CoreColorType::id;
                    break;
                case rive::ComponentBase::namePropertyKey:
                case 271: // TextValueRun::text
                    type = rive::CoreStringType::id;
                    break;
                case rive::ShapePaintBase::isVisiblePropertyKey:
                case rive::LayoutComponentBase::clipPropertyKey:
                case rive::RectangleBase::linkCornerRadiusPropertyKey:
                case rive::PolygonBase::pointsPropertyKey:
                case 51: // KeyedObject::objectId
                case 53: // KeyedProperty::propertyKey
                case 67: // KeyFrame::frame
                case 92: // ClippingShape::sourceId
                case 93: // ClippingShape::fillRule
                case 94: // ClippingShape::isVisible
                case 206: // Image::assetId
                case 117: // TrimPath::modeValue
                case 693: // Dash::lengthIsPercentage
                case 691: // DashPath::offsetIsPercentage
                case 752: // Feather::inner
                case 748: // Feather::spaceValue
                case 281: // Text::alignValue
                case 284: // Text::sizingValue
                case 287: // Text::overflowValue
                case 683: // Text::wrapValue
                case 685: // Text::verticalAlignValue
                case 703: // Text::fitFromBaseline
                case 128: // Path::pathFlags
                case 770: // Path::isHole
                case 279: // TextStyle::fontAssetId
                    type = rive::CoreUintType::id;
                    break;
                case rive::PolygonBase::cornerRadiusPropertyKey:
                case rive::StarBase::innerRadiusPropertyKey:
                case 39: // GradientStop::position
                case 70: // KeyFrameDouble::value
                case 380: // Image::originX
                case 381: // Image::originY
                case 114: // TrimPath::start
                case 115: // TrimPath::end
                case 116: // TrimPath::offset
                case 692: // Dash::length
                case 690: // DashPath::offset
                case 749: // Feather::strength
                case 750: // Feather::offsetX
                case 751: // Feather::offsetY
                case 285: // Text::width
                case 286: // Text::height
                case 366: // Text::originX
                case 367: // Text::originY
                case 371: // Text::paragraphSpacing
                case 274: // TextStyle::fontSize
                case 370: // TextStyle::lineHeight
                case 390: // TextStyle::letterSpacing
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

        // TrimPath - TODO: Requires special path/paint relationship
        // Disabled pending further investigation of Rive's trim system
        // if (shapeData.fill.trimPath.enabled || shapeData.stroke.trimPath.enabled) { ... }

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
            case ShapeType::image:
            {
                auto& image = builder.addCore(new rive::Image());
                builder.setParent(image, shape.id);
                builder.set(image, static_cast<uint16_t>(206), shapeData.assetId); // assetId
                builder.set(image, static_cast<uint16_t>(380), shapeData.originX); // originX
                builder.set(image, static_cast<uint16_t>(381), shapeData.originY); // originY
                break;
            }
            case ShapeType::clipping:
            {
                auto& clip = builder.addCore(new rive::ClippingShape());
                builder.setParent(clip, artboard.id);
                builder.set(clip, static_cast<uint16_t>(92), shapeData.sourceId); // sourceId
                builder.set(clip, static_cast<uint16_t>(93), shapeData.fillRule); // fillRule
                builder.set(clip, static_cast<uint16_t>(94), shapeData.clipVisible); // isVisible
                break;
            }
            case ShapeType::path:
            {
                auto& path = builder.addCore(new rive::Path());
                builder.setParent(path, shape.id);
                builder.set(path, static_cast<uint16_t>(128), static_cast<uint32_t>(0)); // pathFlags
                builder.set(path, static_cast<uint16_t>(770), false); // isHole
                // Note: Full path functionality requires PathVertex objects
                // This is a skeleton for basic path support
                break;
            }
        }

        if (shapeData.fill.enabled)
        {
            auto& fill = builder.addCore(new rive::Fill());
            builder.setParent(fill, shape.id);
            builder.set(fill, rive::ShapePaintBase::isVisiblePropertyKey, true);

            if (shapeData.fill.hasGradient)
            {
                // Create gradient (radial or linear)
                rive::Core* gradientCore = nullptr;
                if (shapeData.fill.gradient.type == "radial")
                {
                    gradientCore = new rive::RadialGradient();
                }
                else
                {
                    gradientCore = new rive::LinearGradient();
                }
                
                auto& gradient = builder.addCore(gradientCore);
                builder.setParent(gradient, fill.id);
                
                // Add gradient stops
                for (const auto& stopData : shapeData.fill.gradient.stops)
                {
                    auto& stop = builder.addCore(new rive::GradientStop());
                    builder.setParent(stop, gradient.id);
                    builder.set(stop, static_cast<uint16_t>(38), stopData.color); // colorValue
                    builder.set(stop, static_cast<uint16_t>(39), stopData.position); // position
                }
            }
            else
            {
                auto& solid = builder.addCore(new rive::SolidColor());
                builder.setParent(solid, fill.id);
                builder.set(solid, rive::SolidColorBase::colorValuePropertyKey,
                            shapeData.fill.color);
            }
            
            // Add Feather to fill if enabled
            if (shapeData.fill.feather.enabled)
            {
                auto& feather = builder.addCore(new rive::Feather());
                builder.setParent(feather, fill.id);
                builder.set(feather, static_cast<uint16_t>(748), static_cast<uint32_t>(0)); // spaceValue
                builder.set(feather, static_cast<uint16_t>(749), shapeData.fill.feather.strength);
                builder.set(feather, static_cast<uint16_t>(750), shapeData.fill.feather.offsetX);
                builder.set(feather, static_cast<uint16_t>(751), shapeData.fill.feather.offsetY);
                builder.set(feather, static_cast<uint16_t>(752), shapeData.fill.feather.inner);
            }
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
            
            // Add Dash if enabled
            if (shapeData.stroke.dash.enabled)
            {
                auto& dashPath = builder.addCore(new rive::DashPath());
                builder.setParent(dashPath, stroke.id);
                builder.set(dashPath, static_cast<uint16_t>(690), 0.0f); // offset
                builder.set(dashPath, static_cast<uint16_t>(691), false); // offsetIsPercentage
                
                // Add dash segments
                auto& dash1 = builder.addCore(new rive::Dash());
                builder.setParent(dash1, dashPath.id);
                builder.set(dash1, static_cast<uint16_t>(692), shapeData.stroke.dash.length);
                builder.set(dash1, static_cast<uint16_t>(693), shapeData.stroke.dash.lengthIsPercentage);
                
                auto& dash2 = builder.addCore(new rive::Dash());
                builder.setParent(dash2, dashPath.id);
                builder.set(dash2, static_cast<uint16_t>(692), shapeData.stroke.dash.gap);
                builder.set(dash2, static_cast<uint16_t>(693), false);
            }
            
        }
        
        // Feather can also be applied to strokes
        if (shapeData.stroke.enabled && shapeData.fill.feather.enabled)
        {
            // Note: Feather is typically on Fill, but can affect stroke rendering
            // Implementation depends on use case
        }
    }

    // Build texts (full implementation with all properties)
    for (const auto& textData : document.texts)
    {
        auto& text = builder.addCore(new rive::Text());
        builder.setParent(text, artboard.id);
        builder.set(text, rive::NodeBase::xPropertyKey, textData.x);
        builder.set(text, rive::NodeBase::yPropertyKey, textData.y);
        builder.set(text, static_cast<uint16_t>(285), textData.width); // width
        builder.set(text, static_cast<uint16_t>(286), textData.height); // height
        builder.set(text, static_cast<uint16_t>(281), textData.align); // alignValue
        builder.set(text, static_cast<uint16_t>(284), textData.sizing); // sizingValue
        builder.set(text, static_cast<uint16_t>(287), textData.overflow); // overflowValue
        builder.set(text, static_cast<uint16_t>(683), textData.wrap); // wrapValue
        builder.set(text, static_cast<uint16_t>(685), textData.verticalAlign); // verticalAlignValue
        builder.set(text, static_cast<uint16_t>(371), textData.paragraphSpacing); // paragraphSpacing
        builder.set(text, static_cast<uint16_t>(703), textData.fitFromBaseline); // fitFromBaseline
        
        // Add TextStyle
        auto& textStyle = builder.addCore(new rive::TextStyle());
        builder.setParent(textStyle, text.id);
        builder.set(textStyle, static_cast<uint16_t>(274), textData.style.fontSize); // fontSize
        builder.set(textStyle, static_cast<uint16_t>(370), textData.style.lineHeight); // lineHeight
        builder.set(textStyle, static_cast<uint16_t>(390), textData.style.letterSpacing); // letterSpacing
        builder.set(textStyle, static_cast<uint16_t>(279), static_cast<uint32_t>(-1)); // fontAssetId (system font)
        
        // Add TextRun with content
        auto& textRun = builder.addCore(new rive::TextValueRun());
        builder.setParent(textRun, textStyle.id);
        builder.set(textRun, static_cast<uint16_t>(271), textData.content); // text property
    }
    
    // Build state machines (skeleton implementation)
    for (const auto& smData : document.stateMachines)
    {
        auto& stateMachine = builder.addCore(new rive::StateMachine());
        builder.set(stateMachine, rive::AnimationBase::namePropertyKey, smData.name);
        
        // Note: Full state machine requires State, Transition, and Input objects
        // This is a minimal skeleton for future expansion
    }
    
    // Constraints are typically added to specific bones/objects
    // Skeleton implementation deferred - requires bone system first

    // Build animations from JSON
    for (size_t animIdx = 0; animIdx < document.animations.size(); ++animIdx)
    {
        const auto& animData = document.animations[animIdx];
        auto& anim = builder.addCore(new rive::LinearAnimation());
        builder.set(anim, rive::AnimationBase::namePropertyKey, animData.name);
        builder.set(anim, rive::LinearAnimationBase::fpsPropertyKey, animData.fps);
        builder.set(anim, rive::LinearAnimationBase::durationPropertyKey, animData.duration);
        builder.set(anim, rive::LinearAnimationBase::loopValuePropertyKey, animData.loop);

        if (!shapeIds.empty())
        {
            // Calculate artboard-local index for the first shape
            // Artboard = 0, animations come next, then shapes
            uint32_t shapeLocalIndex = 1 + static_cast<uint32_t>(document.animations.size());
            
            // Y-position animation
            if (!animData.yKeyframes.empty())
            {
                auto& keyedObj = builder.addCore(new rive::KeyedObject());
                builder.set(keyedObj, static_cast<uint16_t>(51), shapeLocalIndex);

                auto& keyedProp = builder.addCore(new rive::KeyedProperty());
                builder.set(keyedProp, static_cast<uint16_t>(53), static_cast<uint32_t>(14)); // y property

                for (const auto& kf : animData.yKeyframes)
                {
                    auto& keyframe = builder.addCore(new rive::KeyFrameDouble());
                    builder.set(keyframe, static_cast<uint16_t>(67), static_cast<uint32_t>(kf.frame));
                    builder.set(keyframe, static_cast<uint16_t>(70), kf.value);
                    builder.set(keyframe, static_cast<uint16_t>(68), static_cast<uint32_t>(1)); // cubic
                }
            }
            
            // Scale animation (uniform: scaleX and scaleY together)
            if (!animData.scaleKeyframes.empty())
            {
                // ScaleX animation
                auto& keyedObjX = builder.addCore(new rive::KeyedObject());
                builder.set(keyedObjX, static_cast<uint16_t>(51), shapeLocalIndex);

                auto& keyedPropX = builder.addCore(new rive::KeyedProperty());
                builder.set(keyedPropX, static_cast<uint16_t>(53), static_cast<uint32_t>(16)); // scaleX

                for (const auto& kf : animData.scaleKeyframes)
                {
                    auto& keyframe = builder.addCore(new rive::KeyFrameDouble());
                    builder.set(keyframe, static_cast<uint16_t>(67), static_cast<uint32_t>(kf.frame));
                    builder.set(keyframe, static_cast<uint16_t>(70), kf.value);
                    builder.set(keyframe, static_cast<uint16_t>(68), static_cast<uint32_t>(1)); // cubic
                }
                
                // ScaleY animation (same values)
                auto& keyedObjY = builder.addCore(new rive::KeyedObject());
                builder.set(keyedObjY, static_cast<uint16_t>(51), shapeLocalIndex);

                auto& keyedPropY = builder.addCore(new rive::KeyedProperty());
                builder.set(keyedPropY, static_cast<uint16_t>(53), static_cast<uint32_t>(17)); // scaleY

                for (const auto& kf : animData.scaleKeyframes)
                {
                    auto& keyframe = builder.addCore(new rive::KeyFrameDouble());
                    builder.set(keyframe, static_cast<uint16_t>(67), static_cast<uint32_t>(kf.frame));
                    builder.set(keyframe, static_cast<uint16_t>(70), kf.value);
                    builder.set(keyframe, static_cast<uint16_t>(68), static_cast<uint32_t>(1)); // cubic
                }
            }
            
            // Opacity animation
            if (!animData.opacityKeyframes.empty())
            {
                auto& keyedObj = builder.addCore(new rive::KeyedObject());
                builder.set(keyedObj, static_cast<uint16_t>(51), shapeLocalIndex);

                auto& keyedProp = builder.addCore(new rive::KeyedProperty());
                builder.set(keyedProp, static_cast<uint16_t>(53), static_cast<uint32_t>(18)); // opacity

                for (const auto& kf : animData.opacityKeyframes)
                {
                    auto& keyframe = builder.addCore(new rive::KeyFrameDouble());
                    builder.set(keyframe, static_cast<uint16_t>(67), static_cast<uint32_t>(kf.frame));
                    builder.set(keyframe, static_cast<uint16_t>(70), kf.value);
                    builder.set(keyframe, static_cast<uint16_t>(68), static_cast<uint32_t>(1)); // cubic
                }
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
