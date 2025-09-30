#include "core_builder.hpp"
#include "hierarchical_schema.hpp"
#include "font_utils.hpp"
#include <iostream>
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
#include "rive/animation/keyframe_color.hpp"
#include "rive/animation/keyframe_id.hpp"
// Text support (full)
#include "rive/text/text.hpp"
#include "rive/text/text_style.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/text/text_style_axis.hpp"
#include "rive/assets/font_asset.hpp"
#include "rive/generated/text/text_base.hpp"
#include "rive/generated/text/text_style_base.hpp"
#include "rive/generated/text/text_style_paint_base.hpp"
#include "rive/generated/text/text_value_run_base.hpp"
#include "rive/generated/assets/font_asset_base.hpp"
// State machine support (full)  
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_bool.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/entry_state.hpp"
#include "rive/animation/exit_state.hpp"
#include "rive/animation/any_state.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/state_transition.hpp"
// Path vertices support
#include "rive/shapes/points_path.hpp"
#include "rive/shapes/straight_vertex.hpp"
#include "rive/shapes/cubic_detached_vertex.hpp"
#include "rive/shapes/cubic_mirrored_vertex.hpp"
// Event support
#include "rive/event.hpp"
#include "rive/audio_event.hpp"
// Bones and skinning support
#include "rive/bones/bone.hpp"
#include "rive/bones/root_bone.hpp"
#include "rive/bones/tendon.hpp"
#include "rive/bones/weight.hpp"
#include "rive/bones/skin.hpp"
#include "rive/generated/shapes/points_path_base.hpp"
#include "rive/generated/shapes/vertex_base.hpp"
#include "rive/generated/shapes/straight_vertex_base.hpp"
#include "rive/generated/shapes/cubic_detached_vertex_base.hpp"
#include "rive/generated/event_base.hpp"
#include "rive/generated/audio_event_base.hpp"
#include "rive/generated/bones/bone_base.hpp"
#include "rive/generated/bones/root_bone_base.hpp"
#include "rive/generated/bones/tendon_base.hpp"
#include "rive/generated/bones/weight_base.hpp"
#include "rive/generated/bones/skin_base.hpp"
#include "rive/generated/animation/animation_base.hpp"
#include "rive/generated/animation/linear_animation_base.hpp"
#include "rive/generated/animation/keyed_object_base.hpp"
#include "rive/generated/animation/keyed_property_base.hpp"
#include "rive/generated/animation/keyframe_double_base.hpp"
#include "rive/generated/animation/keyframe_color_base.hpp"
#include "rive/generated/animation/keyframe_id_base.hpp"
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

// Helper to parse color strings (for hierarchical format)
static uint32_t parse_color_string(const std::string& color_string, uint32_t fallback = 0xFFFFFFFF)
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

// Build hierarchical shapes (TRUE %100 exact copy!)
static void build_hierarchical_shapes(CoreBuilder& builder,
                                      const std::vector<rive_hierarchical::HierarchicalShapeData>& shapes,
                                      uint32_t artboardId)
{
    std::cout << "Building " << shapes.size() << " hierarchical shapes..." << std::endl;
    
    for (const auto& shapeData : shapes)
    {
        // CRITICAL: For parametric shapes (rectangle/ellipse), we need TWO objects:
        // 1. Shape (ShapePaintContainer) - parent for fills/strokes
        // 2. Parametric path (Rectangle/Ellipse) - child of Shape
        
        CoreObject* shapeContainer = nullptr;
        
        if (shapeData.type == "node") {
            // Node is NOT a shape - just create Node directly
            auto& node = builder.addCore(new rive::Node());
            builder.setParent(node, artboardId);
            
            if (shapeData.x != 0.0f) {
                builder.set(node, rive::NodeBase::xPropertyKey, shapeData.x);
            }
            if (shapeData.y != 0.0f) {
                builder.set(node, rive::NodeBase::yPropertyKey, shapeData.y);
            }
            
            // Node doesn't have paints, skip paint section
            continue;
        }
        else if (shapeData.type == "ellipse" || shapeData.type == "rectangle") {
            // Create Shape wrapper first (for paint container)
            auto& shape = builder.addCore(new rive::Shape());
            builder.setParent(shape, artboardId);
            
            if (shapeData.x != 0.0f) {
                builder.set(shape, rive::NodeBase::xPropertyKey, shapeData.x);
            }
            if (shapeData.y != 0.0f) {
                builder.set(shape, rive::NodeBase::yPropertyKey, shapeData.y);
            }
            
            shapeContainer = &shape;
            
            // Create parametric path as CHILD of Shape
            rive::Core* parametricPath = (shapeData.type == "ellipse")
                ? static_cast<rive::Core*>(new rive::Ellipse())
                : static_cast<rive::Core*>(new rive::Rectangle());
            
            auto& path = builder.addCore(parametricPath);
            builder.setParent(path, shape.id); // Parent to Shape!
            
            builder.set(path, rive::ParametricPathBase::widthPropertyKey, shapeData.width);
            builder.set(path, rive::ParametricPathBase::heightPropertyKey, shapeData.height);
            
            if (shapeData.type == "rectangle") {
                builder.set(path, rive::RectangleBase::linkCornerRadiusPropertyKey, false);
            }
        }
        else {
            // Custom shape with paths/vertices
            auto& shape = builder.addCore(new rive::Shape());
            builder.setParent(shape, artboardId);
            
            if (shapeData.x != 0.0f) {
                builder.set(shape, rive::NodeBase::xPropertyKey, shapeData.x);
            }
            if (shapeData.y != 0.0f) {
                builder.set(shape, rive::NodeBase::yPropertyKey, shapeData.y);
            }
            
            shapeContainer = &shape;
            // Create ALL paths for custom shapes (CRITICAL!)
            for (const auto& pathData : shapeData.paths)
        {
            auto& pointsPath = builder.addCore(new rive::PointsPath());
            builder.setParent(pointsPath, shape.id);
            builder.set(pointsPath, static_cast<uint16_t>(120), pathData.isClosed);
            // Skip pathFlags=0 and isHole=false (defaults)
            
            // Add vertices
            for (const auto& vertexData : pathData.vertices)
            {
                if (vertexData.type == "straight")
                {
                    auto& v = builder.addCore(new rive::StraightVertex());
                    builder.setParent(v, pointsPath.id);
                    builder.set(v, static_cast<uint16_t>(24), vertexData.x);
                    builder.set(v, static_cast<uint16_t>(25), vertexData.y);
                    builder.set(v, static_cast<uint16_t>(26), vertexData.radius);
                }
                else if (vertexData.type == "cubic")
                {
                    auto& v = builder.addCore(new rive::CubicDetachedVertex());
                    builder.setParent(v, pointsPath.id);
                    builder.set(v, static_cast<uint16_t>(24), vertexData.x);
                    builder.set(v, static_cast<uint16_t>(25), vertexData.y);
                    builder.set(v, static_cast<uint16_t>(84), vertexData.inRotation);
                    builder.set(v, static_cast<uint16_t>(85), vertexData.inDistance);
                    builder.set(v, static_cast<uint16_t>(86), vertexData.outRotation);
                    builder.set(v, static_cast<uint16_t>(87), vertexData.outDistance);
                }
                else if (vertexData.type == "cubicMirrored")
                {
                    auto& v = builder.addCore(new rive::CubicMirroredVertex());
                    builder.setParent(v, pointsPath.id);
                    builder.set(v, static_cast<uint16_t>(24), vertexData.x);
                    builder.set(v, static_cast<uint16_t>(25), vertexData.y);
                    builder.set(v, static_cast<uint16_t>(79), vertexData.rotation);
                    builder.set(v, static_cast<uint16_t>(80), vertexData.distance);
                }
            }
        }
        } // End of else block (custom paths)
        
        // Add Fill (if present) - parent to shapeContainer
        if (shapeData.hasFill && shapeContainer)
        {
            auto& fill = builder.addCore(new rive::Fill());
            builder.setParent(fill, shapeContainer->id);
            builder.set(fill, rive::ShapePaintBase::isVisiblePropertyKey, true);
            
            if (shapeData.fill.hasGradient)
            {
                // Create gradient
                rive::Core* gradCore = (shapeData.fill.gradient.type == "radial")
                    ? static_cast<rive::Core*>(new rive::RadialGradient())
                    : static_cast<rive::Core*>(new rive::LinearGradient());
                
                auto& gradient = builder.addCore(gradCore);
                builder.setParent(gradient, fill.id);
                
                // Add GradientStops
                for (const auto& stopData : shapeData.fill.gradient.stops)
                {
                    auto& stop = builder.addCore(new rive::GradientStop());
                    builder.setParent(stop, gradient.id);
                    builder.set(stop, static_cast<uint16_t>(38), parse_color_string(stopData.color));
                    builder.set(stop, static_cast<uint16_t>(39), stopData.position);
                }
            }
            else
            {
                // Solid color
                auto& solid = builder.addCore(new rive::SolidColor());
                builder.setParent(solid, fill.id);
                builder.set(solid, rive::SolidColorBase::colorValuePropertyKey, 
                           parse_color_string(shapeData.fill.solidColor));
            }
            
            // Add Feather if enabled
            if (shapeData.fill.hasFeather)
            {
                auto& feather = builder.addCore(new rive::Feather());
                builder.setParent(feather, fill.id);
                builder.set(feather, static_cast<uint16_t>(749), shapeData.fill.feather.strength);
                builder.set(feather, static_cast<uint16_t>(750), shapeData.fill.feather.offsetX);
                builder.set(feather, static_cast<uint16_t>(751), shapeData.fill.feather.offsetY);
                builder.set(feather, static_cast<uint16_t>(752), shapeData.fill.feather.inner);
            }
        }
        
        // Add Stroke (if present) - parent to shapeContainer
        if (shapeData.hasStroke && shapeContainer)
        {
            auto& stroke = builder.addCore(new rive::Stroke());
            builder.setParent(stroke, shapeContainer->id);
            builder.set(stroke, rive::StrokeBase::thicknessPropertyKey, shapeData.stroke.thickness);
            
            auto& solid = builder.addCore(new rive::SolidColor());
            builder.setParent(solid, stroke.id);
            builder.set(solid, rive::SolidColorBase::colorValuePropertyKey,
                       parse_color_string(shapeData.stroke.color));
        }
    }
    
    std::cout << "Built " << shapes.size() << " shapes successfully!" << std::endl;
}

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

void CoreBuilder::set(CoreObject& object, uint16_t key, const std::vector<uint8_t>& value)
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
                case 88: // KeyFrameColor::value
                    type = rive::CoreColorType::id;
                    break;
                case rive::ComponentBase::namePropertyKey:
                case 203: // Asset::name
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
                case 377: // Text::originValue
                case 128: // Path::pathFlags
                case 770: // Path::isHole
                case 279: // TextStyle::fontAssetId
                case 272: // TextValueRun::styleId
                case 289: // TextStyleAxis::tag (font variation tag)
                case 149: // AnimationState::animationId
                case 151: // StateTransition::stateToId
                case 152: // StateTransition::flags
                case 158: // StateTransition::duration
                case 120: // PointsCommonPath::isClosed
                case rive::StrokeBase::capPropertyKey: // 48
                case rive::StrokeBase::joinPropertyKey: // 49
                case rive::DrawableBase::blendModeValuePropertyKey: // 23
                case rive::DrawableBase::drawableFlagsPropertyKey: // 129
                    type = rive::CoreUintType::id;
                    break;
                case rive::StrokeBase::transformAffectsStrokePropertyKey: // 50
                    type = rive::CoreBoolType::id;
                    break;
                case 703: // Text::fitFromBaseline
                case 141: // StateMachineBool::value
                    type = rive::CoreBoolType::id;
                    break;
                case 142: // StateMachineNumber::value
                    type = rive::CoreDoubleType::id;
                    break;
                case 268: // TextValueRun::text
                case 138: // StateMachineComponent::name
                case 55: // Animation::name (also StateMachine)
                    type = rive::CoreStringType::id;
                    break;
                case 212: // FileAssetContents::bytes
                    type = rive::CoreBytesType::id;
                    break;
                case rive::PolygonBase::cornerRadiusPropertyKey:
                case rive::StarBase::innerRadiusPropertyKey:
                case 39: // GradientStop::position
                case 70: // KeyFrameDouble::value
                case 24: // Vertex::x
                case 25: // Vertex::y
                case 26: // StraightVertex::radius
                case 79: // CubicMirroredVertex::rotation
                case 80: // CubicMirroredVertex::distance
                case 84: // CubicDetachedVertex::inRotation
                case 85: // CubicDetachedVertex::inDistance
                case 86: // CubicDetachedVertex::outRotation
                case 87: // CubicDetachedVertex::outDistance
                case 89: // Bone::length
                case 90: // RootBone::x
                case 91: // RootBone::y
                case 104: // Skin::xx (matrix components)
                case 105: // Skin::yx
                case 106: // Skin::xy
                case 107: // Skin::yy
                case 108: // Skin::tx
                case 109: // Skin::ty
                case 122: // KeyFrameId::value
                case 408: // AudioEvent::assetId
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
                case 288: // TextStyleAxis::axisValue (font weight/width/slant)
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

    if (document.artboards.empty())
    {
        return builder.build(typeMap); // No artboards, return empty
    }
    
    // Load and embed font if ANY artboard has text objects
    std::vector<uint8_t> fontBinary;
    bool hasAnyText = false;
    for (const auto& artboardData : document.artboards)
    {
        if (!artboardData.texts.empty())
        {
            hasAnyText = true;
            break;
        }
    }
    
    if (hasAnyText)
    {
        // Load system font (Arial.ttf ~755KB)
        std::string fontPath = get_system_font_path("Arial");
        fontBinary = load_font_file(fontPath);
        
        // Add font asset (assetId=0)
        auto& fontAsset = builder.addCore(new rive::FontAsset());
        builder.set(fontAsset, static_cast<uint16_t>(203), std::string("Arial")); // Asset::name
        builder.set(fontAsset, static_cast<uint16_t>(204), static_cast<uint32_t>(0)); // FileAsset::assetId
    }

    // Loop through all artboards
    for (const auto& artboardData : document.artboards)
    {
        auto& artboard = builder.addCore(new rive::Artboard());
    builder.set(artboard, rive::ComponentBase::namePropertyKey,
                artboardData.name);
    builder.set(artboard, rive::LayoutComponentBase::widthPropertyKey,
                artboardData.width);
    builder.set(artboard, rive::LayoutComponentBase::heightPropertyKey,
                artboardData.height);
    builder.set(artboard, rive::LayoutComponentBase::clipPropertyKey, true);

    // Store shape IDs for animation keyframe references
    std::vector<uint32_t> shapeIds;

    // Shapes disabled for Artboard-only variant
    for (const auto& shapeData : artboardData.shapes)
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
            case ShapeType::pointsPath:
            {
                // PointsPath is handled in customPaths section below
                // This case is here to avoid switch warning
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

    // Build hierarchical shapes (EXACT COPY MODE!)
    if (artboardData.useHierarchical && !artboardData.hierarchicalShapes.empty())
    {
        std::cout << "Using HIERARCHICAL builder for exact copy!" << std::endl;
        build_hierarchical_shapes(builder, artboardData.hierarchicalShapes, artboard.id);
    }
    // Build custom paths with vertices (LEGACY MODE)
    else if (!artboardData.customPaths.empty())
    {
        for (const auto& pathData : artboardData.customPaths)
    {
        // Create parent Shape for the path
        auto& shape = builder.addCore(new rive::Shape());
        builder.setParent(shape, artboard.id);
        builder.set(shape, rive::NodeBase::xPropertyKey, 0.0f);
        builder.set(shape, rive::NodeBase::yPropertyKey, 0.0f);
        builder.set(shape, rive::TransformComponentBase::rotationPropertyKey, 0.0f);
        builder.set(shape, rive::TransformComponentBase::scaleXPropertyKey, 1.0f);
        builder.set(shape, rive::TransformComponentBase::scaleYPropertyKey, 1.0f);
        builder.set(shape, rive::WorldTransformComponentBase::opacityPropertyKey, 1.0f);
        
        // Create PointsPath (container for vertices)
        auto& pointsPath = builder.addCore(new rive::PointsPath());
        builder.setParent(pointsPath, shape.id);
        builder.set(pointsPath, static_cast<uint16_t>(120), pathData.isClosed); // isClosed
        builder.set(pointsPath, static_cast<uint16_t>(128), static_cast<uint32_t>(0)); // pathFlags
        builder.set(pointsPath, static_cast<uint16_t>(770), false); // isHole
        
        // Add vertices to the path
        for (const auto& vertexData : pathData.vertices)
        {
            if (vertexData.type == VertexType::straight)
            {
                auto& vertex = builder.addCore(new rive::StraightVertex());
                builder.setParent(vertex, pointsPath.id);
                builder.set(vertex, static_cast<uint16_t>(24), vertexData.x); // x
                builder.set(vertex, static_cast<uint16_t>(25), vertexData.y); // y
                builder.set(vertex, static_cast<uint16_t>(26), vertexData.radius); // radius
            }
            else if (vertexData.type == VertexType::cubic)
            {
                auto& vertex = builder.addCore(new rive::CubicDetachedVertex());
                builder.setParent(vertex, pointsPath.id);
                builder.set(vertex, static_cast<uint16_t>(24), vertexData.x); // x
                builder.set(vertex, static_cast<uint16_t>(25), vertexData.y); // y
                builder.set(vertex, static_cast<uint16_t>(84), vertexData.inRotation); // inRotation
                builder.set(vertex, static_cast<uint16_t>(85), vertexData.inDistance); // inDistance
                builder.set(vertex, static_cast<uint16_t>(86), vertexData.outRotation); // outRotation
                builder.set(vertex, static_cast<uint16_t>(87), vertexData.outDistance); // outDistance
            }
        }
        
        // Add fill if enabled (WITH GRADIENT SUPPORT!)
        if (pathData.fillEnabled)
        {
            auto& fill = builder.addCore(new rive::Fill());
            builder.setParent(fill, shape.id);
            builder.set(fill, rive::ShapePaintBase::isVisiblePropertyKey, true);

            if (pathData.hasGradient)
            {
                // Create gradient (linear or radial) with stops!
                rive::Core* gradientCore = nullptr;
                if (pathData.gradient.type == "radial") {
                    gradientCore = new rive::RadialGradient();
                } else {
                    gradientCore = new rive::LinearGradient();
                }
                
                auto& gradient = builder.addCore(gradientCore);
                builder.setParent(gradient, fill.id);
                
                // Add ALL gradient stops
                for (const auto& stopData : pathData.gradient.stops) {
                    auto& stop = builder.addCore(new rive::GradientStop());
                    builder.setParent(stop, gradient.id);
                    builder.set(stop, static_cast<uint16_t>(38), stopData.color); // colorValue
                    builder.set(stop, static_cast<uint16_t>(39), stopData.position); // position
                }
            }
            else
            {
                // Solid color
                auto& solid = builder.addCore(new rive::SolidColor());
                builder.setParent(solid, fill.id);
                builder.set(solid, rive::SolidColorBase::colorValuePropertyKey, pathData.fillColor);
            }
        }
        
        // Add stroke if enabled
        if (pathData.strokeEnabled)
        {
            auto& stroke = builder.addCore(new rive::Stroke());
            builder.setParent(stroke, shape.id);
            builder.set(stroke, rive::StrokeBase::thicknessPropertyKey, pathData.strokeThickness);

            auto& solid = builder.addCore(new rive::SolidColor());
            builder.setParent(solid, stroke.id);
            builder.set(solid, rive::SolidColorBase::colorValuePropertyKey, pathData.strokeColor);
        }
    }
    } // End of else if customPaths

    // Build events (NEW - Casino Slots support)
    for (const auto& eventData : artboardData.events)
    {
        if (eventData.type == "audio")
        {
            auto& audioEvent = builder.addCore(new rive::AudioEvent());
            builder.setParent(audioEvent, artboard.id);
            builder.set(audioEvent, static_cast<uint16_t>(4), eventData.name); // Component::name
            builder.set(audioEvent, static_cast<uint16_t>(408), eventData.assetId); // AudioEvent::assetId
        }
        else
        {
            auto& event = builder.addCore(new rive::Event());
            builder.setParent(event, artboard.id);
            builder.set(event, static_cast<uint16_t>(4), eventData.name); // Component::name
        }
    }
    
    // Build bones (NEW - Casino Slots support)
    for (const auto& boneData : artboardData.bones)
    {
        if (boneData.type == "root")
        {
            auto& rootBone = builder.addCore(new rive::RootBone());
            builder.setParent(rootBone, artboard.id);
            builder.set(rootBone, static_cast<uint16_t>(4), boneData.name); // Component::name
            builder.set(rootBone, static_cast<uint16_t>(90), boneData.x); // RootBone::x
            builder.set(rootBone, static_cast<uint16_t>(91), boneData.y); // RootBone::y
            builder.set(rootBone, static_cast<uint16_t>(89), boneData.length); // Bone::length
        }
        else
        {
            auto& bone = builder.addCore(new rive::Bone());
            builder.setParent(bone, artboard.id);
            builder.set(bone, static_cast<uint16_t>(4), boneData.name);
            builder.set(bone, static_cast<uint16_t>(89), boneData.length);
        }
    }
    
    // Build skins (NEW - Casino Slots support)
    for (const auto& skinData : artboardData.skins)
    {
        auto& skin = builder.addCore(new rive::Skin());
        builder.setParent(skin, artboard.id);
        builder.set(skin, static_cast<uint16_t>(104), skinData.xx); // Skin::xx
        builder.set(skin, static_cast<uint16_t>(105), skinData.yx); // Skin::yx
        builder.set(skin, static_cast<uint16_t>(106), skinData.xy); // Skin::xy
        builder.set(skin, static_cast<uint16_t>(107), skinData.yy); // Skin::yy
        builder.set(skin, static_cast<uint16_t>(108), skinData.tx); // Skin::tx
        builder.set(skin, static_cast<uint16_t>(109), skinData.ty); // Skin::ty
    }

    // Build texts - FULL IMPLEMENTATION with all properties    
    for (const auto& textData : artboardData.texts)
    {
        auto& text = builder.addCore(new rive::Text());
        builder.setParent(text, artboard.id);
        
        // Position (Node properties)
        builder.set(text, rive::NodeBase::xPropertyKey, textData.x);
        builder.set(text, rive::NodeBase::yPropertyKey, textData.y);
        
        // Transform (default to identity)
        builder.set(text, rive::TransformComponentBase::rotationPropertyKey, 0.0f);
        builder.set(text, rive::TransformComponentBase::scaleXPropertyKey, 1.0f);
        builder.set(text, rive::TransformComponentBase::scaleYPropertyKey, 1.0f);
        
        // WorldTransform (opacity)
        builder.set(text, rive::WorldTransformComponentBase::opacityPropertyKey, 1.0f);
        
        // Drawable properties
        builder.set(text, rive::DrawableBase::blendModeValuePropertyKey, static_cast<uint32_t>(3)); // SrcOver
        builder.set(text, rive::DrawableBase::drawableFlagsPropertyKey, static_cast<uint32_t>(4)); // visible
        
        // Text layout properties (TextBase)
        builder.set(text, static_cast<uint16_t>(281), textData.align); // alignValue
        builder.set(text, static_cast<uint16_t>(284), textData.sizing); // sizingValue  
        builder.set(text, static_cast<uint16_t>(285), textData.width); // width
        builder.set(text, static_cast<uint16_t>(286), textData.height); // height
        builder.set(text, static_cast<uint16_t>(287), textData.overflow); // overflowValue
        builder.set(text, static_cast<uint16_t>(366), 0.0f); // originX
        builder.set(text, static_cast<uint16_t>(367), 0.0f); // originY
        builder.set(text, static_cast<uint16_t>(371), textData.paragraphSpacing); // paragraphSpacing
        builder.set(text, static_cast<uint16_t>(377), static_cast<uint32_t>(0)); // originValue
        builder.set(text, static_cast<uint16_t>(683), textData.wrap); // wrapValue
        builder.set(text, static_cast<uint16_t>(685), textData.verticalAlign); // verticalAlignValue
        builder.set(text, static_cast<uint16_t>(703), textData.fitFromBaseline); // fitFromBaseline
        
        // Add TextStylePaint - FULL with all typography properties
        auto& textStylePaint = builder.addCore(new rive::TextStylePaint());
        builder.setParent(textStylePaint, text.id);
        builder.set(textStylePaint, static_cast<uint16_t>(274), textData.style.fontSize); // fontSize
        builder.set(textStylePaint, static_cast<uint16_t>(370), textData.style.lineHeight); // lineHeight (-1 = auto)
        builder.set(textStylePaint, static_cast<uint16_t>(390), textData.style.letterSpacing); // letterSpacing
        builder.set(textStylePaint, static_cast<uint16_t>(279), static_cast<uint32_t>(0)); // fontAssetId
        
        // Add Fill for text color (ShapePaintContainer pattern)
        auto& textFill = builder.addCore(new rive::Fill());
        builder.setParent(textFill, textStylePaint.id);
        builder.set(textFill, rive::ShapePaintBase::isVisiblePropertyKey, true);
        
        auto& textFillColor = builder.addCore(new rive::SolidColor());
        builder.setParent(textFillColor, textFill.id);
        builder.set(textFillColor, rive::SolidColorBase::colorValuePropertyKey, textData.style.color);
        
        // Add Stroke for text outline (if requested in JSON)
        if (textData.style.hasStroke)
        {
            auto& textStroke = builder.addCore(new rive::Stroke());
            builder.setParent(textStroke, textStylePaint.id);
            builder.set(textStroke, rive::StrokeBase::thicknessPropertyKey, textData.style.strokeThickness);
            builder.set(textStroke, rive::StrokeBase::capPropertyKey, static_cast<uint32_t>(0)); // butt
            builder.set(textStroke, rive::StrokeBase::joinPropertyKey, static_cast<uint32_t>(0)); // miter
            builder.set(textStroke, rive::ShapePaintBase::isVisiblePropertyKey, true);
            
            auto& textStrokeColor = builder.addCore(new rive::SolidColor());
            builder.setParent(textStrokeColor, textStroke.id);
            builder.set(textStrokeColor, rive::SolidColorBase::colorValuePropertyKey, textData.style.strokeColor);
        }
        
        // Add TextStyleAxis for variable fonts (font weight, width, slant)
        if (textData.style.fontWeight != 400) // Non-normal weight
        {
            auto& weightAxis = builder.addCore(new rive::TextStyleAxis());
            builder.setParent(weightAxis, textStylePaint.id);
            builder.set(weightAxis, static_cast<uint16_t>(289), static_cast<uint32_t>(0x77676874)); // 'wght' tag
            builder.set(weightAxis, static_cast<uint16_t>(288), static_cast<float>(textData.style.fontWeight));
        }
        
        if (textData.style.fontWidth != 100.0f) // Non-normal width
        {
            auto& widthAxis = builder.addCore(new rive::TextStyleAxis());
            builder.setParent(widthAxis, textStylePaint.id);
            builder.set(widthAxis, static_cast<uint16_t>(289), static_cast<uint32_t>(0x77647468)); // 'wdth' tag
            builder.set(widthAxis, static_cast<uint16_t>(288), textData.style.fontWidth);
        }
        
        if (textData.style.fontSlant != 0.0f) // Has slant/italic
        {
            auto& slantAxis = builder.addCore(new rive::TextStyleAxis());
            builder.setParent(slantAxis, textStylePaint.id);
            builder.set(slantAxis, static_cast<uint16_t>(289), static_cast<uint32_t>(0x736c6e74)); // 'slnt' tag
            builder.set(slantAxis, static_cast<uint16_t>(288), textData.style.fontSlant);
        }
        
        // Add TextValueRun (direct child of Text)
        auto& textRun = builder.addCore(new rive::TextValueRun());
        builder.setParent(textRun, text.id);
        builder.set(textRun, static_cast<uint16_t>(268), textData.content); // text property
        builder.set(textRun, static_cast<uint16_t>(272), textStylePaint.id); // styleId (will be remapped)
    }
    
    // Build state machines - StateMachine is Core (not Component), no parentId
    for (const auto& smData : artboardData.stateMachines)
    {
        auto& stateMachine = builder.addCore(new rive::StateMachine());
        // NOTE: StateMachine extends Animation extends Core (NOT Component)
        // So NO setParent() call - hierarchy is implicit by file order
        builder.set(stateMachine, rive::AnimationBase::namePropertyKey, smData.name); // name key = 55
        
        // Add inputs (Bool, Number, Trigger) - Core objects, no parentId
        for (const auto& inputData : smData.inputs)
        {
            if (inputData.type == "bool")
            {
                auto& input = builder.addCore(new rive::StateMachineBool());
                // NO setParent() - StateMachineInput is Core, not Component
                builder.set(input, static_cast<uint16_t>(138), inputData.name); // StateMachineComponent::name = 138
                builder.set(input, static_cast<uint16_t>(141), inputData.defaultValue != 0.0f); // value property
            }
            else if (inputData.type == "number")
            {
                auto& input = builder.addCore(new rive::StateMachineNumber());
                // NO setParent()
                builder.set(input, static_cast<uint16_t>(138), inputData.name);
                builder.set(input, static_cast<uint16_t>(142), inputData.defaultValue);
            }
            else if (inputData.type == "trigger")
            {
                auto& input = builder.addCore(new rive::StateMachineTrigger());
                // NO setParent()
                builder.set(input, static_cast<uint16_t>(138), inputData.name);
            }
        }
        
        // Build animation name → 0-based index map
        // AnimationState::animationId expects direct 0-based index into artboard's animation list
        // State machines are stored separately and must NOT be counted
        std::map<std::string, uint32_t> animationIndexMap;
        for (size_t i = 0; i < artboardData.animations.size(); ++i)
        {
            animationIndexMap[artboardData.animations[i].name] = static_cast<uint32_t>(i);
        }
        
        // Add layers
        for (const auto& layerData : smData.layers)
        {
            auto& layer = builder.addCore(new rive::StateMachineLayer());
            // NO setParent() - Core object, implicit hierarchy by file order
            builder.set(layer, static_cast<uint16_t>(138), layerData.name); // StateMachineComponent::name
            
            // Map state names to layer-local indices for transitions
            std::map<std::string, uint32_t> stateIndexMap;
            uint32_t stateIndex = 0;
            
            // CRITICAL: Every layer MUST have Entry, Exit, Any states (runtime requirement)
            // Add system states first (order matters for indexing)
            
            // 1. EntryState (typeKey 63)
            auto& entryState = builder.addCore(new rive::EntryState());
            // NO setParent() - Core object
            stateIndexMap["Entry"] = stateIndex++;
            
            // 2. ExitState (typeKey 64)
            auto& exitState = builder.addCore(new rive::ExitState());
            // NO setParent() - Core object
            stateIndexMap["Exit"] = stateIndex++;
            
            // 3. AnyState (typeKey 62)
            auto& anyState = builder.addCore(new rive::AnyState());
            // NO setParent() - Core object
            stateIndexMap["Any"] = stateIndex++;
            
            // 4. Add user-defined states from JSON
            for (const auto& stateData : layerData.states)
            {
                // Skip system states if user explicitly listed them in JSON
                if (stateData.type == "entry" || stateData.type == "exit" || stateData.type == "any")
                {
                    // Already added above, just record the name mapping
                    if (stateData.type == "entry") stateIndexMap[stateData.name] = 0;
                    else if (stateData.type == "exit") stateIndexMap[stateData.name] = 1;
                    else if (stateData.type == "any") stateIndexMap[stateData.name] = 2;
                    continue;
                }
                
                if (stateData.type == "animation")
                {
                    auto& animState = builder.addCore(new rive::AnimationState());
                    // NO setParent() - Core object
                    
                    // Find animation artboard-local index
                    auto it = animationIndexMap.find(stateData.animationName);
                    if (it != animationIndexMap.end())
                    {
                        builder.set(animState, static_cast<uint16_t>(149), it->second); // animationId
                    }
                    
                    stateIndexMap[stateData.name] = stateIndex++;
                }
            }
            
            // Add transitions (written after all states in the layer)
            for (const auto& transData : layerData.transitions)
            {
                auto& transition = builder.addCore(new rive::StateTransition());
                // NO setParent() - Core object
                
                // Find target state layer-local index
                auto toIt = stateIndexMap.find(transData.to);
                if (toIt != stateIndexMap.end())
                {
                    builder.set(transition, static_cast<uint16_t>(151), toIt->second); // stateToId
                }
                
                builder.set(transition, static_cast<uint16_t>(158), transData.duration); // duration (ms)
                builder.set(transition, static_cast<uint16_t>(152), static_cast<uint32_t>(0)); // flags
                
                // TODO: Add transition conditions (TransitionCondition objects)
                // Deferred for now - requires input index mapping
            }
        }
    }
    
    // Constraints are typically added to specific bones/objects
    // Skeleton implementation deferred - requires bone system first

    // Build animations from JSON
    for (size_t animIdx = 0; animIdx < artboardData.animations.size(); ++animIdx)
    {
        const auto& animData = artboardData.animations[animIdx];
        auto& anim = builder.addCore(new rive::LinearAnimation());
        builder.set(anim, rive::AnimationBase::namePropertyKey, animData.name);
        builder.set(anim, rive::LinearAnimationBase::fpsPropertyKey, animData.fps);
        builder.set(anim, rive::LinearAnimationBase::durationPropertyKey, animData.duration);
        builder.set(anim, rive::LinearAnimationBase::loopValuePropertyKey, animData.loop);

        if (!shapeIds.empty())
        {
            // Calculate artboard-local index for the first shape
            // Artboard = 0, animations come next, then shapes
            uint32_t shapeLocalIndex = 1 + static_cast<uint32_t>(artboardData.animations.size());
            
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
    
    } // End of artboard loop

    // Provide default animation if none specified (legacy - check first artboard)
    if (!document.artboards.empty() && document.artboards[0].animations.empty())
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

    auto doc = builder.build(typeMap);
    doc.fontData = std::move(fontBinary);
    return doc;
}
} // namespace rive_converter
