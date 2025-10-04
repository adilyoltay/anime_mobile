#include "core_builder.hpp"
#include "json_loader.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cctype>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include "rive/artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/node.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/ellipse.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/points_path.hpp"
#include "rive/shapes/straight_vertex.hpp"
#include "rive/shapes/cubic_detached_vertex.hpp"
#include "rive/shapes/cubic_mirrored_vertex.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/paint/linear_gradient.hpp"
#include "rive/shapes/paint/radial_gradient.hpp"
#include "rive/shapes/paint/gradient_stop.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/shapes/paint/dash.hpp"
#include "rive/shapes/paint/dash_path.hpp"
#include "rive/shapes/paint/trim_path.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/animation/keyframe_double.hpp"
#include "rive/animation/keyframe_color.hpp"
#include "rive/animation/keyframe_id.hpp"
#include "rive/animation/keyframe_bool.hpp"
#include "rive/animation/keyframe_string.hpp"
#include "rive/animation/keyframe_uint.hpp"
#include "rive/animation/keyframe_callback.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include "rive/data_bind/converters/data_converter_range_mapper.hpp"
#include "rive/data_bind/converters/data_converter_to_string.hpp"
#include "rive/data_bind/converters/data_converter_group_item.hpp"
#include "rive/generated/data_bind/data_bind_base.hpp"
#include "rive/generated/data_bind/data_bind_context_base.hpp"
#include "rive/generated/data_bind/converters/data_converter_base.hpp"
#include "rive/generated/data_bind/converters/data_converter_range_mapper_base.hpp"
#include "rive/generated/data_bind/converters/data_converter_to_string_base.hpp"
#include "rive/generated/data_bind/converters/data_converter_group_item_base.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/layer_state.hpp"
#include "rive/animation/any_state.hpp"
#include "rive/animation/entry_state.hpp"
#include "rive/animation/exit_state.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/animation/state_machine_bool.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/generated/animation/animation_base.hpp"
#include "rive/generated/animation/state_machine_component_base.hpp"
#include "rive/generated/animation/state_machine_layer_base.hpp"
#include "rive/generated/animation/state_machine_number_base.hpp"
#include "rive/generated/animation/state_machine_bool_base.hpp"
#include "rive/generated/animation/state_machine_trigger_base.hpp"
#include "rive/generated/animation/entry_state_base.hpp"
#include "rive/generated/animation/any_state_base.hpp"
#include "rive/generated/animation/animation_state_base.hpp"
#include "rive/animation/cubic_ease_interpolator.hpp"
#include "rive/animation/cubic_value_interpolator.hpp"
#include "rive/bones/bone.hpp"
#include "rive/bones/root_bone.hpp"
#include "rive/constraints/translation_constraint.hpp"
#include "rive/constraints/follow_path_constraint.hpp"
#include "rive/shapes/paint/trim_path.hpp"
#include "rive/drawable.hpp"
#include "rive/draw_target.hpp"
#include "rive/draw_rules.hpp"
// Layout system requires WITH_RIVE_LAYOUT (yoga dependency)
// #include "rive/layout/layout_component_style.hpp"
#include "rive/generated/backboard_base.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/assets/font_asset.hpp"
#include "rive/assets/file_asset_contents.hpp"
#include "rive/core/field_types/core_bytes_type.hpp"

namespace rive_converter
{

// PR2: Helper functions for type classification
static bool isParametricPathType(uint16_t typeKey) {
    return typeKey == 4 ||  // Ellipse
           typeKey == 7 ||  // Rectangle
           typeKey == 16;   // PointsPath
}

static bool isPaintOrDecorator(uint16_t typeKey) {
    return typeKey == 20 ||  // Fill
           typeKey == 24 ||  // Stroke
           typeKey == 47 ||  // TrimPath
           typeKey == 506 || // DashPath (DashPathBase::typeKey)
           typeKey == 507 || // Dash (DashBase::typeKey)
           typeKey == 533 || // Feather
           typeKey == 22 ||  // LinearGradient
           typeKey == 17 ||  // RadialGradient
           typeKey == 19 ||  // GradientStop
           typeKey == 18;    // SolidColor
}

// PR-ORPHAN-FIX: Top-level paints only (Fill/Stroke that attach to Shapes)
// Excludes gradient components (LinearGradient, RadialGradient, GradientStop, SolidColor)
// which are valid children of Fill/Stroke
static bool isTopLevelPaint(uint16_t typeKey) {
    return typeKey == 20 ||  // Fill
           typeKey == 24;    // Stroke
}

static bool isVertexType(uint16_t typeKey) {
    return typeKey == 5 ||   // StraightVertex
           typeKey == 6 ||   // CubicDetachedVertex
           typeKey == 35;    // CubicMirroredVertex
}

static bool isAnimGraphType(uint16_t typeKey) {
    return typeKey == 25 ||  // KeyedObject
           typeKey == 26 ||  // KeyedProperty
           typeKey == 28 ||  // CubicEaseInterpolator
           typeKey == 30 ||  // KeyFrameDouble
           typeKey == 31 ||  // LinearAnimation - CRITICAL: Animation system manages these
           typeKey == 37 ||  // KeyFrameColor
           typeKey == 50 ||  // KeyFrameId
           typeKey == 84 ||  // KeyFrameBool
           typeKey == 138 || // CubicValueInterpolator
           typeKey == 142 || // KeyFrameString
           typeKey == 450;   // KeyFrameUint
}

static bool isKeyedSerializationType(uint16_t typeKey) {
    return typeKey == 25 ||  // KeyedObject
           typeKey == 26 ||  // KeyedProperty
           typeKey == 28 ||  // CubicEaseInterpolator
           typeKey == 30 ||  // KeyFrameDouble
           typeKey == 37 ||  // KeyFrameColor
           typeKey == 50 ||  // KeyFrameId
           typeKey == 84 ||  // KeyFrameBool
           typeKey == 138 || // CubicValueInterpolator
           typeKey == 139 || // CubicInterpolatorBase
           typeKey == 142 || // KeyFrameString
           typeKey == 171 || // KeyFrameCallback
           typeKey == 174 || // ElasticInterpolatorBase
           typeKey == 175 || // KeyFrameInterpolator (abstract placeholder)
           typeKey == 450;   // KeyFrameUint
}

static bool isShapePaintContainer(uint16_t typeKey) {
    return typeKey == 3 ||   // Shape
           typeKey == 4 ||   // Ellipse
           typeKey == 7 ||   // Rectangle
           typeKey == 16;    // PointsPath
}

// Helper to parse color strings
static uint32_t parse_color(const std::string& colorStr) {
    if (colorStr.empty() || colorStr[0] != '#') return 0xFFFFFFFF;
    
    std::string hex = colorStr.substr(1);
    uint32_t value = std::stoul(hex, nullptr, 16);
    if (hex.length() <= 6) {
        value |= 0xFF000000u; // Add alpha
    }
    return value;
}

static std::vector<uint8_t> decode_base64(const std::string& input)
{
    auto decode_char = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };

    std::vector<uint8_t> out;
    int val = 0;
    int bits = -8;
    for (unsigned char ch : input)
    {
        if (std::isspace(ch))
        {
            continue;
        }
        if (ch == '=')
        {
            break;
        }
        int decoded = decode_char(static_cast<char>(ch));
        if (decoded < 0)
        {
            continue;
        }
        val = (val << 6) | decoded;
        bits += 6;
        if (bits >= 0)
        {
            out.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

// Create Core object based on typeKey
static rive::Core* createObjectByTypeKey(uint16_t typeKey) {
    switch (typeKey) {
        case 1: return new rive::Artboard();
        case 2: return new rive::Node();
        case 3: return new rive::Shape();
        case 4: return new rive::Ellipse();
        case 5: return new rive::StraightVertex();
        case 6: return new rive::CubicDetachedVertex();
        case 7: return new rive::Rectangle();
        case 16: return new rive::PointsPath();
        case 17: return new rive::RadialGradient();
        case 18: return new rive::SolidColor();
        case 19: return new rive::GradientStop();
        case 20: return new rive::Fill();
        case 22: return new rive::LinearGradient();
        case 24: return new rive::Stroke();
        case 106: return new rive::FileAssetContents();
        // Animation keyframe types
        case 25: return new rive::KeyedObject();
        case 26: return new rive::KeyedProperty();
        case 28: return new rive::CubicEaseInterpolator(); // Bezier interpolator
        case 30: return new rive::KeyFrameDouble();
        case 31: return new rive::LinearAnimation();
        case 35: return new rive::CubicMirroredVertex();
        case 37: return new rive::KeyFrameColor();
        case 50: return new rive::KeyFrameId();
        case 84: return new rive::KeyFrameBool();
        case 142: return new rive::KeyFrameString();
        case 171: return new rive::KeyFrameCallback();
        // case 175: KeyFrameInterpolator is abstract; do not instantiate directly
        case 450: return new rive::KeyFrameUint();
        // State Machine types
        case 53: return new rive::StateMachine();
        case 56: return new rive::StateMachineNumber();
        case 57: return new rive::StateMachineLayer();
        case 58: return new rive::StateMachineTrigger();
        case 59: return new rive::StateMachineBool();
        case 61: return new rive::AnimationState();
        case 62: return new rive::AnyState();
        case 63: return new rive::EntryState();
        case 64: return new rive::ExitState();
        case 65: return new rive::StateTransition();
        case 40: return new rive::Bone();
        case 41: return new rive::RootBone();
        case 42: return new rive::ClippingShape(); // NOT Bone!
        case 47: return new rive::TrimPath();
        case 87: return new rive::TranslationConstraint();
        case 138: return new rive::CubicValueInterpolator(); // Bezier interpolator
        case 165: return new rive::FollowPathConstraint();
        case 134: return new rive::Text();
        case 135: return new rive::TextValueRun();
        case 137: return new rive::TextStylePaint();
        case 141: return new rive::FontAsset();
        case rive::DataBindBase::typeKey: return new rive::DataBind();
        case rive::DataBindContextBase::typeKey: return new rive::DataBindContext();
        case rive::DataConverterToStringBase::typeKey: return new rive::DataConverterToString();
        case rive::DataConverterRangeMapperBase::typeKey: return new rive::DataConverterRangeMapper();
        case rive::DataConverterGroupItemBase::typeKey: return new rive::DataConverterGroupItem();
        // case 420: return new rive::LayoutComponentStyle(); // Requires WITH_RIVE_LAYOUT - skipped for now
        case 533: return new rive::Feather();
        case 507: return new rive::Dash();      // Dash (DashBase::typeKey)
        case 506: return new rive::DashPath();  // DashPath (DashPathBase::typeKey)
        // DrawTarget/DrawRules (PR-DRAWTARGET)
        case 48: return new rive::DrawTarget();
        case 49: return new rive::DrawRules();
        // Add more as needed
        default:
            std::cerr << "Unknown typeKey: " << typeKey << std::endl;
            return nullptr;
    }
}

static constexpr uint16_t kTypeKeyLinearAnimation = 31;
static constexpr uint16_t kTypeKeyKeyedObject = 25;
static constexpr uint16_t kTypeKeyKeyedProperty = 26;

static uint16_t selectKeyFrameType(const rive_converter::KeyFrameData& data, int propertyFieldType)
{
    using rive_converter::KeyFrameValueType;

    switch (data.valueType)
    {
        case KeyFrameValueType::colorValue:
            return rive::KeyFrameColor::typeKey;
        case KeyFrameValueType::boolValue:
            return rive::KeyFrameBool::typeKey;
        case KeyFrameValueType::stringValue:
            return rive::KeyFrameString::typeKey;
        case KeyFrameValueType::uintValue:
            return rive::KeyFrameUint::typeKey;
        case KeyFrameValueType::idValue:
            return rive::KeyFrameId::typeKey;
        case KeyFrameValueType::doubleValue:
            return rive::KeyFrameDouble::typeKey;
        case KeyFrameValueType::unknown:
        default:
            break;
    }

    switch (propertyFieldType)
    {
        case rive::CoreColorType::id:
            return rive::KeyFrameColor::typeKey;
        case rive::CoreBoolType::id:
            return rive::KeyFrameBool::typeKey;
        case rive::CoreStringType::id:
            return rive::KeyFrameString::typeKey;
        case rive::CoreUintType::id:
            return rive::KeyFrameUint::typeKey;
        default:
            return rive::KeyFrameDouble::typeKey;
    }
}

static void applyKeyFrameValue(CoreBuilder& builder,
                               CoreObject& keyframeObj,
                               uint16_t typeKey,
                               const rive_converter::KeyFrameData& data,
                               int propertyFieldType)
{
    constexpr uint32_t kSentinelNone = std::numeric_limits<uint32_t>::max();

    auto resolveUnsigned = [&](const rive_converter::KeyFrameData& keyframeData) -> uint32_t {
        if (keyframeData.valueType == rive_converter::KeyFrameValueType::uintValue)
        {
            return keyframeData.uintValue;
        }

        if (!keyframeData.rawValue.is_null())
        {
            if (keyframeData.rawValue.is_number_integer())
            {
                int64_t rawInt = keyframeData.rawValue.get<int64_t>();
                if (rawInt < 0)
                {
                    return kSentinelNone;
                }
                return static_cast<uint32_t>(rawInt);
            }
            if (keyframeData.rawValue.is_number_unsigned())
            {
                return keyframeData.rawValue.get<uint32_t>();
            }
        }

        if (keyframeData.valueType == rive_converter::KeyFrameValueType::doubleValue)
        {
            if (keyframeData.value < 0.0f)
            {
                return kSentinelNone;
            }
            double rounded = std::llround(static_cast<double>(keyframeData.value));
            if (rounded < 0.0)
            {
                return 0u;
            }
            if (rounded > static_cast<double>(std::numeric_limits<uint32_t>::max()))
            {
                return std::numeric_limits<uint32_t>::max();
            }
            return static_cast<uint32_t>(rounded);
        }

        return 0u;
    };

    switch (typeKey)
    {
        case rive::KeyFrameColor::typeKey:
            builder.set(keyframeObj, rive::KeyFrameColorBase::valuePropertyKey, data.colorValue);
            break;
        case rive::KeyFrameBool::typeKey:
        {
            bool boolValue = data.valueType == rive_converter::KeyFrameValueType::boolValue
                                 ? data.boolValue
                                 : (data.value != 0.0f);
            builder.set(keyframeObj, rive::KeyFrameBoolBase::valuePropertyKey, boolValue);
            break;
        }
        case rive::KeyFrameString::typeKey:
            builder.set(keyframeObj, rive::KeyFrameStringBase::valuePropertyKey, data.stringValue);
            break;
        case rive::KeyFrameUint::typeKey:
        {
            uint32_t uintValue = resolveUnsigned(data);
            builder.set(keyframeObj, rive::KeyFrameUintBase::valuePropertyKey, uintValue);
            break;
        }
        case rive::KeyFrameId::typeKey:
        {
            uint32_t idValue = data.idValue;
            if (idValue == 0u)
            {
                idValue = resolveUnsigned(data);
            }
            builder.set(keyframeObj, rive::KeyFrameIdBase::valuePropertyKey, idValue);
            break;
        }
        case rive::KeyFrameDouble::typeKey:
        default:
            builder.set(keyframeObj, rive::KeyFrameDoubleBase::valuePropertyKey, data.value);
            break;
    }
}

// Set property on object based on key name and value
// Note: x/y are handled in main loop (different keys for Node vs Vertex)
static void setProperty(CoreBuilder& builder, CoreObject& obj, const std::string& key, const nlohmann::json& value, 
                        const std::map<uint32_t, uint32_t>& idMapping, int& objectIdRemapSuccess, int& objectIdRemapFail) {
    // Transform properties (x/y handled in main loop!)
    if (key == "rotation") builder.set(obj, 15, value.get<float>());
    else if (key == "scaleX") builder.set(obj, 16, value.get<float>());
    else if (key == "scaleY") builder.set(obj, 17, value.get<float>());
    else if (key == "opacity") builder.set(obj, 18, value.get<float>());
    
    // Name (Component) - PR2: Use correct property key based on typeKey
    else if (key == "name") {
        uint16_t typeKey = obj.typeKey;
        if (typeKey == 31) { // LinearAnimation
            builder.set(obj, 55, value.get<std::string>()); // AnimationBase::namePropertyKey = 55
        } else if (typeKey == 53 || typeKey == 57 || typeKey == 61 || typeKey == 62 || 
                   typeKey == 63 || typeKey == 64 || typeKey == 65) { // SM + Layer + States + Transition
            builder.set(obj, 138, value.get<std::string>()); // StateMachineComponentBase::namePropertyKey = 138
        } else if (typeKey == rive::DataConverterBase::typeKey ||
                   typeKey == rive::DataConverterRangeMapperBase::typeKey ||
                   typeKey == rive::DataConverterToStringBase::typeKey)
        {
            builder.set(obj, rive::DataConverterBase::namePropertyKey, value.get<std::string>());
        } else { // Component (Artboard, Shape, etc.)
            builder.set(obj, 4, value.get<std::string>()); // ComponentBase::namePropertyKey = 4
        }
    }
    
    // Parametric shapes (width/height handled in main loop for artboard vs shape distinction)
    else if (key == "linkCornerRadius") builder.set(obj, rive::RectangleBase::linkCornerRadiusPropertyKey, value.get<bool>());
    
    // Path
    else if (key == "isClosed") builder.set(obj, 32, value.get<bool>());  // PointsCommonPathBase::isClosedPropertyKey
    else if (key == "pathFlags") builder.set(obj, 128, value.get<uint32_t>());
    
    // Vertices
    else if (key == "radius") builder.set(obj, 26, value.get<float>());
    else if (key == "inRotation") builder.set(obj, 84, value.get<float>());
    else if (key == "inDistance") builder.set(obj, 85, value.get<float>());
    else if (key == "outRotation") builder.set(obj, 86, value.get<float>());
    else if (key == "outDistance") builder.set(obj, 87, value.get<float>());
    // CubicMirrored
    else if (key == "distance") {
        uint16_t typeKey = obj.typeKey;
        if (typeKey == 35) { // CubicMirroredVertex
            builder.set(obj, 80, value.get<float>());
        } else if (typeKey == 165) { // FollowPathConstraint
            builder.set(obj, 363, static_cast<float>(value.get<double>()));
        } else {
            builder.set(obj, 80, value.get<float>());
        }
    }
    else if (key == "orient") {
        if (obj.typeKey == 165) { // FollowPathConstraint
            if (value.is_boolean()) {
                builder.set(obj, 364, value.get<bool>());
            } else if (value.is_number()) {
                builder.set(obj, 364, value.get<double>() != 0.0);
            }
        }
    }
    else if (key == "assetId") {
        builder.set(obj, 204, value.get<uint32_t>());
    }
    else if (key == "bytes") {
        std::vector<uint8_t> decoded = decode_base64(value.get<std::string>());
        builder.set(obj, 212, decoded);
    }
    else if (key == "offset") {
        uint16_t typeKey = obj.typeKey;
        if (typeKey == 47) { // TrimPath
            if (value.is_number()) {
                builder.set(obj, 116, static_cast<float>(value.get<double>()));
            }
        } else if (typeKey == 506) { // DashPath (DashPathBase::offsetPropertyKey)
            if (value.is_number()) {
                builder.set(obj, 690, static_cast<float>(value.get<double>()));
            }
        } else if (typeKey == 165) { // FollowPathConstraint
            if (value.is_boolean()) {
                builder.set(obj, 365, value.get<bool>());
            } else if (value.is_number()) {
                builder.set(obj, 365, value.get<double>() != 0.0);
            }
        }
    }
    else if (key == "offsetIsPercentage") {
        uint16_t typeKey = obj.typeKey;
        if (typeKey == 506) { // DashPath (DashPathBase::offsetIsPercentagePropertyKey)
            builder.set(obj, 691, value.get<bool>());
        }
    }
    
    // Constraints - TargetedConstraint (targetId needs DEFERRED remapping in PASS3)
    // NOTE: targetId NOT set here - handled in PASS3 after all objects created
    else if (key == "targetId") {
        // Skip - will be handled in PASS3
    }
    
    // Animation - InterpolatingKeyFrame (interpolatorId needs DEFERRED remapping in PASS3)
    // CRITICAL FIX: interpolatorId from JSON needs remapping to runtime component ID
    else if (key == "interpolatorId") {
        // Skip - will be handled in PASS3
    }
    
    // Constraints - TransformSpaceConstraint
    else if (key == "sourceSpaceValue") {
        if (value.is_number()) {
            builder.set(obj, 179, value.get<uint32_t>());
        }
    }
    else if (key == "destSpaceValue") {
        if (value.is_number()) {
            builder.set(obj, 180, value.get<uint32_t>());
        }
    }
    
    // Paint
    else if (key == "color") builder.set(obj, 37, parse_color(value.get<std::string>()));
    else if (key == "isVisible") builder.set(obj, 41, value.get<bool>());
    else if (key == "thickness") builder.set(obj, 140, value.get<float>());
    else if (key == "cap") builder.set(obj, 48, value.get<uint32_t>());
    else if (key == "join") builder.set(obj, 49, value.get<uint32_t>());

    // Text component properties
    else if (key == "align") builder.set(obj, 281, value.get<uint32_t>());
    else if (key == "sizing") builder.set(obj, 284, value.get<uint32_t>());
    else if (key == "overflow") builder.set(obj, 287, value.get<uint32_t>());
    else if (key == "paragraphSpacing") builder.set(obj, 371, value.get<float>());
    else if (key == "origin") builder.set(obj, 377, value.get<uint32_t>());
    else if (key == "wrap") builder.set(obj, 683, value.get<uint32_t>());
    else if (key == "verticalAlign") builder.set(obj, 685, value.get<uint32_t>());
    else if (key == "fitFromBaseline") builder.set(obj, 703, value.get<bool>());
    else if (key == "originX") builder.set(obj, 366, value.get<float>());
    else if (key == "originY") builder.set(obj, 367, value.get<float>());
    else if (key == "text") builder.set(obj, 268, value.get<std::string>());

    // Text style properties
    else if (key == "fontSize") builder.set(obj, 274, value.get<float>());
    else if (key == "lineHeight") builder.set(obj, 370, value.get<float>());
    else if (key == "letterSpacing") builder.set(obj, 390, value.get<float>());

    // Drawable
    else if (key == "blendMode" || key == "blendModeValue") builder.set(obj, 23, value.get<uint32_t>());
    else if (key == "drawableFlags") builder.set(obj, 129, value.get<uint32_t>());
    
    // Gradient
    else if (key == "position") builder.set(obj, 39, value.get<float>());
    
    // Feather
    else if (key == "strength") builder.set(obj, 749, value.get<float>());
    else if (key == "offsetX") builder.set(obj, 750, value.get<float>());
    else if (key == "offsetY") builder.set(obj, 751, value.get<float>());
    else if (key == "inner") builder.set(obj, 752, value.get<bool>());
    
    // Bone / Dash
    else if (key == "length") {
        uint16_t typeKey = obj.typeKey;
        if (typeKey == 507) { // Dash (DashBase::lengthPropertyKey)
            builder.set(obj, 692, value.get<float>());
        } else { // Bone (default)
            builder.set(obj, 89, value.get<float>());
        }
    }
    else if (key == "lengthIsPercentage") {
        uint16_t typeKey = obj.typeKey;
        if (typeKey == 507) { // Dash (DashBase::lengthIsPercentagePropertyKey)
            builder.set(obj, 693, value.get<bool>());
        }
    }
    
    // Cubic Interpolator (animation)
    else if (key == "x1") builder.set(obj, 63, value.get<float>());
    else if (key == "y1") builder.set(obj, 64, value.get<float>());
    else if (key == "x2") builder.set(obj, 65, value.get<float>());
    else if (key == "y2") builder.set(obj, 66, value.get<float>());
    
    // Animation keyframe properties
    else if (key == "objectId") {
        // Remap localId to builderId for KeyedObject
        // DO NOT FALLBACK - dangling references cause importer hang!
        uint32_t localId = value.get<uint32_t>();
        auto it = idMapping.find(localId);
        if (it != idMapping.end()) {
            builder.set(obj, 51, it->second); // Use remapped builderId
            objectIdRemapSuccess++; // PR3: Track successful remap
        } else {
            objectIdRemapFail++; // PR3: Track failed remap
            std::cerr << "⚠️  PR3: objectId remap FAILED for localId=" << localId 
                      << " (dangling reference!)" << std::endl;
        }
    }
    else if (key == "propertyKey") {
        uint16_t typeKey = obj.typeKey;
        if (typeKey == 26) // KeyedProperty
        {
            builder.set(obj, 53, value.get<uint32_t>());
        }
        else if (typeKey == rive::DataBindBase::typeKey)
        {
            builder.set(obj, rive::DataBindBase::propertyKeyPropertyKey, value.get<uint32_t>());
        }
    }
    else if (key == "flags")
    {
        uint16_t typeKey = obj.typeKey;
        if (typeKey == rive::DataBindBase::typeKey)
        {
            builder.set(obj, rive::DataBindBase::flagsPropertyKey, value.get<uint32_t>());
        }
        else if (typeKey == rive::DataConverterRangeMapperBase::typeKey)
        {
            builder.set(obj, rive::DataConverterRangeMapperBase::flagsPropertyKey, value.get<uint32_t>());
        }
        else if (typeKey == rive::DataConverterToStringBase::typeKey)
        {
            builder.set(obj, rive::DataConverterToStringBase::flagsPropertyKey, value.get<uint32_t>());
        }
    }
    else if (key == "converterId")
    {
        uint16_t typeKey = obj.typeKey;
        if (typeKey == rive::DataBindBase::typeKey)
        {
            builder.set(obj, rive::DataBindBase::converterIdPropertyKey,
                        value.get<uint32_t>());
        }
        else if (typeKey == rive::DataConverterGroupItemBase::typeKey)
        {
            builder.set(obj, rive::DataConverterGroupItemBase::converterIdPropertyKey,
                        value.get<uint32_t>());
        }
    }
    else if (key == "sourcePathIds")
    {
        if (value.is_array())
        {
            std::vector<uint8_t> bytes;
            bytes.reserve(value.size() * 2);
            for (const auto& entry : value)
            {
                uint16_t u16 = static_cast<uint16_t>(entry.get<int>() & 0xFFFF);
                bytes.push_back(static_cast<uint8_t>(u16 & 0xFF));
                bytes.push_back(static_cast<uint8_t>((u16 >> 8) & 0xFF));
            }
            builder.set(obj, rive::DataBindContextBase::sourcePathIdsPropertyKey, bytes);
        }
    }
    else if (key == "interpolationType")
    {
        if (obj.typeKey == rive::DataConverterRangeMapperBase::typeKey)
        {
            builder.set(obj, rive::DataConverterRangeMapperBase::interpolationTypePropertyKey,
                        value.get<uint32_t>());
        }
    }
    else if (key == "interpolatorId")
    {
        if (obj.typeKey == rive::DataConverterRangeMapperBase::typeKey)
        {
            int32_t signedId = value.get<int32_t>();
            builder.set(obj, rive::DataConverterRangeMapperBase::interpolatorIdPropertyKey,
                        static_cast<uint32_t>(signedId));
        }
    }
    else if (key == "minInput")
    {
        if (obj.typeKey == rive::DataConverterRangeMapperBase::typeKey)
        {
            builder.set(obj, rive::DataConverterRangeMapperBase::minInputPropertyKey,
                        static_cast<float>(value.get<double>()));
        }
    }
    else if (key == "maxInput")
    {
        if (obj.typeKey == rive::DataConverterRangeMapperBase::typeKey)
        {
            builder.set(obj, rive::DataConverterRangeMapperBase::maxInputPropertyKey,
                        static_cast<float>(value.get<double>()));
        }
    }
    else if (key == "minOutput")
    {
        if (obj.typeKey == rive::DataConverterRangeMapperBase::typeKey)
        {
            builder.set(obj, rive::DataConverterRangeMapperBase::minOutputPropertyKey,
                        static_cast<float>(value.get<double>()));
        }
    }
    else if (key == "maxOutput")
    {
        if (obj.typeKey == rive::DataConverterRangeMapperBase::typeKey)
        {
            builder.set(obj, rive::DataConverterRangeMapperBase::maxOutputPropertyKey,
                        static_cast<float>(value.get<double>()));
        }
    }
    else if (key == "decimals")
    {
        if (obj.typeKey == rive::DataConverterToStringBase::typeKey)
        {
            builder.set(obj, rive::DataConverterToStringBase::decimalsPropertyKey,
                        value.get<uint32_t>());
        }
    }
    else if (key == "colorFormat")
    {
        if (obj.typeKey == rive::DataConverterToStringBase::typeKey)
        {
            builder.set(obj, rive::DataConverterToStringBase::colorFormatPropertyKey,
                        value.get<std::string>());
        }
    }
    else if (key == "frame") builder.set(obj, 67, value.get<uint32_t>()); // KeyFrame
    else if (key == "value") {
        // KeyFrame value - property key depends on KeyFrame subclass type
        uint16_t typeKey = obj.typeKey;
        
        if (typeKey == 30) { // KeyFrameDouble
            builder.set(obj, 70, value.get<float>());
        } else if (typeKey == 37) { // KeyFrameColor
            builder.set(obj, 88, value.get<uint32_t>());
        } else if (typeKey == 50) { // KeyFrameId
            builder.set(obj, 122, value.get<uint32_t>());
        } else if (typeKey == 84) { // KeyFrameBool
            builder.set(obj, 181, value.get<bool>());
        } else if (typeKey == 142) { // KeyFrameString
            builder.set(obj, 280, value.get<std::string>());
        } else if (typeKey == 450) { // KeyFrameUint
            builder.set(obj, 631, value.get<uint32_t>());
        } else {
            // Fallback: try to infer from value type
            if (value.is_number_float()) {
                builder.set(obj, 70, value.get<float>());
            } else if (value.is_boolean()) {
                builder.set(obj, 181, value.get<bool>());
            } else if (value.is_string()) {
                builder.set(obj, 280, value.get<std::string>());
            } else {
                builder.set(obj, 631, value.get<uint32_t>());
            }
        }
    }
    // Animation properties
    else if (key == "fps") builder.set(obj, 56, value.get<uint32_t>());
    else if (key == "duration") {
        // Could be LinearAnimation (57) or StateTransition (158)
        uint16_t typeKey = obj.typeKey;
        if (typeKey == 31) { // LinearAnimation
            builder.set(obj, 57, value.get<uint32_t>());
        } else if (typeKey == 65) { // StateTransition
            builder.set(obj, 158, value.get<uint32_t>());
        }
    }
    else if (key == "loop") builder.set(obj, 59, value.get<uint32_t>());
    
    // State Machine properties (ID remapping needed!)
    else if (key == "animationId") {
        // AnimationState.animationId is 0-based index into animations (NOT builderId)
        // No remapping needed - already correct from extractor
        builder.set(obj, 149, value.get<uint32_t>());
    }
    else if (key == "stateToId") {
        // StateTransition.stateToId is layer-local index (NOT builderId)
        // No remapping needed - already correct from extractor
        builder.set(obj, 151, value.get<uint32_t>());
    }
    else if (key == "flags") builder.set(obj, 152, value.get<uint32_t>()); // StateTransition
}

// Initialize PropertyTypeMap for universal builder
static void initUniversalTypeMap(PropertyTypeMap& typeMap) {
    // Transform properties
    typeMap[13] = rive::CoreDoubleType::id; // x
    typeMap[14] = rive::CoreDoubleType::id; // y
    typeMap[15] = rive::CoreDoubleType::id; // rotation
    typeMap[16] = rive::CoreDoubleType::id; // scaleX
    typeMap[17] = rive::CoreDoubleType::id; // scaleY
    typeMap[18] = rive::CoreDoubleType::id; // opacity
    
    // Parametric
    typeMap[20] = rive::CoreDoubleType::id; // width
    typeMap[21] = rive::CoreDoubleType::id; // height
    typeMap[164] = rive::CoreBoolType::id; // RectangleBase::linkCornerRadius (correct key)
    
    // TrimPath (PR2d: defaults for empty properties)
    typeMap[114] = rive::CoreDoubleType::id; // TrimPath::start
    typeMap[115] = rive::CoreDoubleType::id; // TrimPath::end
    typeMap[116] = rive::CoreDoubleType::id; // TrimPath::offset
    typeMap[117] = rive::CoreUintType::id;   // TrimPath::modeValue

    // Constraints - TargetedConstraint base (typeKey 80)
    typeMap[173] = rive::CoreUintType::id;   // targetId
    
    // Constraints - TransformSpaceConstraint base (typeKey 90)
    typeMap[179] = rive::CoreUintType::id;   // sourceSpaceValue
    typeMap[180] = rive::CoreUintType::id;   // destSpaceValue
    
    // FollowPathConstraint (typeKey 165)
    typeMap[363] = rive::CoreDoubleType::id; // distance
    typeMap[364] = rive::CoreBoolType::id;   // orient
    typeMap[365] = rive::CoreBoolType::id;   // offset
    
    // DrawTarget (typeKey 48) - PR-DRAWTARGET
    typeMap[119] = rive::CoreUintType::id;   // drawableIdPropertyKey
    typeMap[120] = rive::CoreUintType::id;   // placementValuePropertyKey
    
    // DrawRules (typeKey 49) - PR-DRAWTARGET
    typeMap[121] = rive::CoreUintType::id;   // drawTargetIdPropertyKey
    
    // Path
    typeMap[32] = rive::CoreBoolType::id;  // isClosed (PointsCommonPathBase::isClosedPropertyKey)
    typeMap[128] = rive::CoreUintType::id; // pathFlags

    // Drawable defaults
    typeMap[23] = rive::CoreUintType::id;  // blendModeValue
    typeMap[129] = rive::CoreUintType::id; // drawableFlags
    
    // Vertices
    typeMap[24] = rive::CoreDoubleType::id; // vertex x (shared)
    typeMap[25] = rive::CoreDoubleType::id; // vertex y
    typeMap[26] = rive::CoreDoubleType::id; // radius
    typeMap[79] = rive::CoreDoubleType::id; // cubicMirrored rotation
    typeMap[80] = rive::CoreDoubleType::id; // cubicMirrored distance
    typeMap[84] = rive::CoreDoubleType::id; // inRotation
    typeMap[85] = rive::CoreDoubleType::id; // inDistance
    typeMap[86] = rive::CoreDoubleType::id; // outRotation
    typeMap[87] = rive::CoreDoubleType::id; // outDistance
    
    // Paint
    typeMap[37] = rive::CoreColorType::id; // color
    typeMap[38] = rive::CoreColorType::id; // gradientStop color
    typeMap[39] = rive::CoreDoubleType::id; // position
    typeMap[41] = rive::CoreBoolType::id; // isVisible
    typeMap[48] = rive::CoreUintType::id; // cap
    typeMap[49] = rive::CoreUintType::id; // join
    typeMap[140] = rive::CoreDoubleType::id; // thickness
    
    // Feather
    typeMap[749] = rive::CoreDoubleType::id; // strength
    typeMap[750] = rive::CoreDoubleType::id; // offsetX
    typeMap[751] = rive::CoreDoubleType::id; // offsetY
    typeMap[752] = rive::CoreBoolType::id; // inner
    
    // DashPath (typeKey 506)
    typeMap[690] = rive::CoreDoubleType::id; // offset (DashPathBase::offsetPropertyKey)
    typeMap[691] = rive::CoreBoolType::id;   // offsetIsPercentage (DashPathBase::offsetIsPercentagePropertyKey)
    
    // Dash (typeKey 507)
    typeMap[692] = rive::CoreDoubleType::id; // length (DashBase::lengthPropertyKey)
    typeMap[693] = rive::CoreBoolType::id;   // lengthIsPercentage (DashBase::lengthIsPercentagePropertyKey)
    
    // Bone
    typeMap[89] = rive::CoreDoubleType::id; // length
    typeMap[90] = rive::CoreDoubleType::id; // rootBone x
    typeMap[91] = rive::CoreDoubleType::id; // rootBone y
    
    // Cubic Interpolator
    typeMap[63] = rive::CoreDoubleType::id; // x1
    typeMap[64] = rive::CoreDoubleType::id; // y1
    typeMap[65] = rive::CoreDoubleType::id; // x2
    typeMap[66] = rive::CoreDoubleType::id; // y2
    
    // Animation keyframe properties
    typeMap[51] = rive::CoreUintType::id; // KeyedObject.objectId
    typeMap[53] = rive::CoreUintType::id; // KeyedProperty.propertyKey
    typeMap[56] = rive::CoreUintType::id; // LinearAnimation.fps
    typeMap[57] = rive::CoreUintType::id; // LinearAnimation.duration
    typeMap[59] = rive::CoreUintType::id; // LinearAnimation.loop
    typeMap[67] = rive::CoreUintType::id; // KeyFrame.frame
    typeMap[68] = rive::CoreUintType::id; // InterpolatingKeyFrame.interpolationType
    typeMap[70] = rive::CoreDoubleType::id; // KeyFrameDouble.value
    typeMap[88] = rive::CoreColorType::id; // KeyFrameColor.value (MUST be Color, not Uint!)
    typeMap[122] = rive::CoreUintType::id; // KeyFrameId.value
    typeMap[181] = rive::CoreBoolType::id;   // KeyFrameBool.value
    typeMap[280] = rive::CoreStringType::id; // KeyFrameString.value
    typeMap[631] = rive::CoreUintType::id;   // KeyFrameUint.value
    typeMap[69]  = rive::CoreUintType::id;   // InterpolatingKeyFrame.interpolatorId

    // Text / Font properties
    typeMap[203] = rive::CoreStringType::id; // Component name (for FontAsset names)
    typeMap[204] = rive::CoreUintType::id;   // FontAsset.assetId
    typeMap[212] = rive::CoreBytesType::id;  // FileAssetContents.bytes
    typeMap[268] = rive::CoreStringType::id; // TextValueRun.text
    typeMap[272] = rive::CoreUintType::id;   // TextValueRun.styleId
    typeMap[274] = rive::CoreDoubleType::id; // TextStyle.fontSize
    typeMap[279] = rive::CoreUintType::id;   // TextStyle.fontAssetId
    typeMap[281] = rive::CoreUintType::id;   // Text.alignValue
    typeMap[284] = rive::CoreUintType::id;   // Text.sizingValue
    typeMap[285] = rive::CoreDoubleType::id; // Text.width
    typeMap[286] = rive::CoreDoubleType::id; // Text.height
    typeMap[287] = rive::CoreUintType::id;   // Text.overflowValue
    typeMap[366] = rive::CoreDoubleType::id; // Text.originX
    typeMap[367] = rive::CoreDoubleType::id; // Text.originY
    typeMap[371] = rive::CoreDoubleType::id; // Text.paragraphSpacing
    typeMap[377] = rive::CoreUintType::id;   // Text.originValue
    typeMap[390] = rive::CoreDoubleType::id; // TextStyle.letterSpacing
    typeMap[683] = rive::CoreUintType::id;   // Text.wrapValue
    typeMap[685] = rive::CoreUintType::id;   // Text.verticalAlignValue
    typeMap[703] = rive::CoreBoolType::id;   // Text.fitFromBaseline
    
    // Backboard & Component
    // NOTE: Field 3 (Component ID) does NOT exist in Rive format! It's internal only.
    // typeMap[3] = rive::CoreUintType::id; // WRONG - no such property!
    typeMap[4] = rive::CoreStringType::id; // name
    typeMap[5] = rive::CoreUintType::id; // parentId
    typeMap[7] = rive::CoreDoubleType::id; // artboard width
    typeMap[8] = rive::CoreDoubleType::id; // artboard height
    typeMap[44] = rive::CoreUintType::id; // mainArtboardId
    typeMap[196] = rive::CoreBoolType::id; // clip
    
    // LayoutComponentStyle (420) - Layout system properties
    typeMap[498] = rive::CoreDoubleType::id; // gapHorizontal
    typeMap[499] = rive::CoreDoubleType::id; // gapVertical
    typeMap[500] = rive::CoreDoubleType::id; // maxWidth
    typeMap[501] = rive::CoreDoubleType::id; // maxHeight
    typeMap[502] = rive::CoreDoubleType::id; // minWidth
    typeMap[503] = rive::CoreDoubleType::id; // minHeight
    typeMap[504] = rive::CoreDoubleType::id; // borderLeft
    typeMap[505] = rive::CoreDoubleType::id; // borderRight
    typeMap[506] = rive::CoreDoubleType::id; // borderTop
    typeMap[507] = rive::CoreDoubleType::id; // borderBottom
    typeMap[508] = rive::CoreDoubleType::id; // marginLeft
    typeMap[509] = rive::CoreDoubleType::id; // marginRight
    typeMap[510] = rive::CoreDoubleType::id; // marginTop
    typeMap[511] = rive::CoreDoubleType::id; // marginBottom
    typeMap[512] = rive::CoreDoubleType::id; // paddingLeft
    typeMap[513] = rive::CoreDoubleType::id; // paddingRight
    typeMap[514] = rive::CoreDoubleType::id; // paddingTop
    typeMap[515] = rive::CoreDoubleType::id; // paddingBottom
    typeMap[516] = rive::CoreDoubleType::id; // positionLeft
    typeMap[517] = rive::CoreDoubleType::id; // positionRight
    typeMap[518] = rive::CoreDoubleType::id; // positionTop
    typeMap[519] = rive::CoreDoubleType::id; // positionBottom
    typeMap[520] = rive::CoreDoubleType::id; // flex
    typeMap[521] = rive::CoreDoubleType::id; // flexGrow
    typeMap[522] = rive::CoreDoubleType::id; // flexShrink
    typeMap[523] = rive::CoreDoubleType::id; // flexBasis
    typeMap[524] = rive::CoreDoubleType::id; // aspectRatio
    typeMap[589] = rive::CoreUintType::id; // animationStyleType
    typeMap[590] = rive::CoreUintType::id; // interpolationType
    typeMap[591] = rive::CoreUintType::id; // interpolatorId
    typeMap[592] = rive::CoreDoubleType::id; // interpolationTime
    typeMap[586] = rive::CoreUintType::id; // DataBind.propertyKey
    typeMap[660] = rive::CoreUintType::id; // DataBind.converterId
    typeMap[588] = rive::CoreBytesType::id; // DataBindContext.sourcePathIds
    typeMap[596] = rive::CoreUintType::id; // displayValue
    typeMap[597] = rive::CoreUintType::id; // positionTypeValue
    typeMap[598] = rive::CoreUintType::id; // flexDirectionValue
    typeMap[599] = rive::CoreUintType::id; // directionValue
    typeMap[600] = rive::CoreUintType::id; // alignContentValue
    typeMap[601] = rive::CoreUintType::id; // alignItemsValue
    typeMap[602] = rive::CoreUintType::id; // alignSelfValue
    typeMap[603] = rive::CoreUintType::id; // justifyContentValue
    typeMap[604] = rive::CoreUintType::id; // flexWrapValue
    typeMap[605] = rive::CoreUintType::id; // overflowValue
    typeMap[606] = rive::CoreBoolType::id; // intrinsicallySizedValue
    typeMap[607] = rive::CoreUintType::id; // widthUnitsValue
    typeMap[608] = rive::CoreUintType::id; // heightUnitsValue
    typeMap[609] = rive::CoreUintType::id; // borderLeftUnitsValue
    typeMap[610] = rive::CoreUintType::id; // borderRightUnitsValue
    typeMap[611] = rive::CoreUintType::id; // borderTopUnitsValue
    typeMap[612] = rive::CoreUintType::id; // borderBottomUnitsValue
    typeMap[613] = rive::CoreUintType::id; // marginLeftUnitsValue
    typeMap[614] = rive::CoreUintType::id; // marginRightUnitsValue
    typeMap[615] = rive::CoreUintType::id; // marginTopUnitsValue
    typeMap[616] = rive::CoreUintType::id; // marginBottomUnitsValue
    typeMap[617] = rive::CoreUintType::id; // paddingLeftUnitsValue
    typeMap[618] = rive::CoreUintType::id; // paddingRightUnitsValue
    typeMap[619] = rive::CoreUintType::id; // paddingTopUnitsValue
    typeMap[620] = rive::CoreUintType::id; // paddingBottomUnitsValue
    typeMap[621] = rive::CoreUintType::id; // positionLeftUnitsValue
    typeMap[622] = rive::CoreUintType::id; // positionRightUnitsValue
    typeMap[623] = rive::CoreUintType::id; // positionTopUnitsValue
    typeMap[624] = rive::CoreUintType::id; // positionBottomUnitsValue
    typeMap[625] = rive::CoreUintType::id; // gapHorizontalUnitsValue
    typeMap[626] = rive::CoreUintType::id; // gapVerticalUnitsValue
    typeMap[627] = rive::CoreUintType::id; // minWidthUnitsValue
    typeMap[628] = rive::CoreUintType::id; // minHeightUnitsValue
    typeMap[629] = rive::CoreUintType::id; // maxWidthUnitsValue
    typeMap[630] = rive::CoreUintType::id; // maxHeightUnitsValue
    typeMap[632] = rive::CoreUintType::id; // layoutAlignmentType
    typeMap[639] = rive::CoreBoolType::id; // linkCornerRadius
    typeMap[640] = rive::CoreDoubleType::id; // cornerRadiusTL
    typeMap[641] = rive::CoreDoubleType::id; // cornerRadiusTR
    typeMap[642] = rive::CoreDoubleType::id; // cornerRadiusBL
    typeMap[643] = rive::CoreDoubleType::id; // cornerRadiusBR
    typeMap[655] = rive::CoreUintType::id; // layoutWidthScaleType
    typeMap[656] = rive::CoreUintType::id; // layoutHeightScaleType
    typeMap[705] = rive::CoreUintType::id; // flexBasisUnitsValue
    
    // Animation properties
    typeMap[55] = rive::CoreStringType::id; // AnimationBase name
    typeMap[56] = rive::CoreUintType::id; // LinearAnimation fps
    typeMap[57] = rive::CoreUintType::id; // LinearAnimation duration
    typeMap[58] = rive::CoreDoubleType::id; // LinearAnimation speed
    typeMap[59] = rive::CoreUintType::id; // LinearAnimation loopValue
    typeMap[60] = rive::CoreUintType::id; // LinearAnimation workStart
    typeMap[61] = rive::CoreUintType::id; // LinearAnimation workEnd
    typeMap[62] = rive::CoreBoolType::id; // LinearAnimation enableWorkArea
    typeMap[376] = rive::CoreBoolType::id; // LinearAnimation quantize
    
    // State Machine properties
    typeMap[138] = rive::CoreStringType::id; // StateMachineComponentBase name
    typeMap[140] = rive::CoreDoubleType::id; // StateMachineNumber value
    typeMap[141] = rive::CoreBoolType::id; // StateMachineBool value
    typeMap[149] = rive::CoreUintType::id; // AnimationState animationId
    typeMap[151] = rive::CoreUintType::id; // StateTransition stateToId
    typeMap[152] = rive::CoreUintType::id; // StateTransition flags
    typeMap[158] = rive::CoreUintType::id; // StateTransition duration
    typeMap[160] = rive::CoreUintType::id; // StateTransition exitTime
    typeMap[349] = rive::CoreUintType::id; // StateTransition interpolationType
    typeMap[350] = rive::CoreUintType::id; // StateTransition interpolatorId
    typeMap[537] = rive::CoreDoubleType::id; // StateTransition randomWeight
}

CoreDocument build_from_universal_json(const nlohmann::json& data, PropertyTypeMap& outTypeMap) {
    std::cout << "=== UNIVERSAL JSON TO RIV BUILDER ===" << std::endl;
    
    // PR3: Re-enable keyed data with safe emission (animation-block grouping)
    constexpr bool OMIT_KEYED = false; // PR3: Keyed data re-enabled
    constexpr bool OMIT_STATE_MACHINE = false; // PR-SM: StateMachine re-enabled
    
    CoreBuilder builder;
    PropertyTypeMap typeMap;
    initUniversalTypeMap(typeMap);
    std::vector<uint8_t> embeddedFontData;
    
    // Add Backboard
    auto& backboard = builder.addCore(new rive::Backboard());
    builder.set(backboard, 44, static_cast<uint32_t>(0)); // mainArtboardId
    
    // Process each artboard
    for (size_t abIdx = 0; abIdx < data["artboards"].size(); ++abIdx) {
        const auto& abJson = data["artboards"][abIdx];
        
        // Guard against zero-sized or empty artboards that cause grey screens.
        float abWidth = abJson.value("width", 0.0f);
        float abHeight = abJson.value("height", 0.0f);
        if ((abWidth == 0.0f && abHeight == 0.0f) || abJson["objects"].empty()) {
            std::cout << "Skipping zero-sized/empty artboard " << abIdx << ": " << abJson["name"] << std::endl;
            continue;
        }

        std::cout << "Building artboard " << abIdx << ": " << abJson["name"] << std::endl;
        std::cout << "  Objects: " << abJson["objects"].size() << std::endl;
        
        struct PendingObject
        {
            CoreObject* core = nullptr;
            uint16_t typeKey = 0;
            std::optional<uint32_t> localId;
            uint32_t parentLocalId = 0xFFFFFFFFu;
        };

        struct StateMachineBindingInfo
        {
            CoreObject* stateMachine = nullptr;
            std::unordered_map<std::string, CoreObject*> inputsByName;
            std::vector<CoreObject*> inputList;
            std::unordered_map<std::string, CoreObject*> statesByName;
            std::vector<CoreObject*> stateList;
        };

        struct DataBindContextInfo
        {
            CoreObject* context = nullptr;
            std::vector<uint16_t> originalPath;
        };

        const uint32_t invalidParent = 0xFFFFFFFFu;
        
        // PR2: Debug counters for visibility
        int shapeInserted = 0;
        int paintsMoved = 0;
        int verticesKept = 0;
        int vertexRemapAttempted = 0;
        int animNodeRemapAttempted = 0;
        
        // PR3: Animation graph counters
        int keyedObjectCount = 0;
        int keyedPropertyCount = 0;
        int keyFrameCount = 0;
        int interpolatorCount = 0;
        int objectIdRemapSuccess = 0;
        int objectIdRemapFail = 0;
        
        // Map from JSON localId to builder object id
        std::map<uint32_t, uint32_t> localIdToBuilderObjectId;
        std::unordered_map<uint32_t, uint16_t> localIdToType; // Track type per localId
        std::vector<PendingObject> pendingObjects; // Stored in creation order
        std::set<uint32_t> skippedLocalIds; // Track stub/skipped object localIds
        std::unordered_map<uint32_t, uint32_t> parentRemap; // old parent localId -> Shape container localId
        // Deferred targetId remapping (PASS3)
        struct DeferredTargetId {
            CoreObject* obj;
            uint32_t jsonTargetLocalId;
        };
        std::vector<DeferredTargetId> deferredTargetIds;
        
        // PR-DRAWTARGET: Deferred component references for DrawTarget/DrawRules
        struct DeferredComponentRef {
            CoreObject* obj;
            uint16_t propertyKey;
            uint32_t jsonComponentLocalId;
        };
        std::vector<DeferredComponentRef> deferredComponentRefs;
        std::vector<DataBindContextInfo> dataBindContexts;
        std::vector<StateMachineBindingInfo> stateMachineBindings;
        std::unordered_map<uint32_t, std::vector<uint16_t>> dataBindContextOriginalPaths;
        std::vector<uint32_t> animationLocalIdsInOrder;
        std::unordered_map<std::string, uint32_t> animationNameToLocalId;
        std::unordered_map<std::string, uint32_t> animationNameToBuilderId;
        std::unordered_map<uint32_t, uint32_t> animationLocalIdToIndex;
        std::unordered_map<std::string, uint32_t> animationNameToIndex;

        uint32_t maxLocalId = 0;
        for (const auto& objJson : abJson["objects"]) {
            if (objJson.contains("localId")) {
                maxLocalId = std::max(maxLocalId, objJson["localId"].get<uint32_t>());
            }
        }
        uint32_t nextSyntheticLocalId = maxLocalId + 1;

        uint32_t artboardLocalId = 0;
        bool artboardLocalIdValid = false;
        for (const auto& objJson : abJson["objects"]) {
            if (objJson.contains("typeKey") &&
                objJson["typeKey"].get<uint16_t>() == rive::Artboard::typeKey &&
                objJson.contains("localId"))
            {
                artboardLocalId = objJson["localId"].get<uint32_t>();
                artboardLocalIdValid = true;
                break;
            }
        }

        // PASS 0: Pre-scan all objects to build complete localId → typeKey mapping
        // This prevents false synthetic Shape injection when parent appears later in JSON
        std::cout << "  PASS 0: Building complete type mapping..." << std::endl;
        for (const auto& objJson : abJson["objects"]) {
            if (objJson.contains("localId")) {
                uint32_t localId = objJson["localId"].get<uint32_t>();
                uint16_t typeKey = objJson["typeKey"];
                
                // Skip stubs
                if (objJson.contains("__unsupported__") && objJson["__unsupported__"].get<bool>()) {
                    skippedLocalIds.insert(localId);
                    continue;
                }
                
                localIdToType[localId] = typeKey;
            }
        }
        std::cout << "  Type mapping: " << localIdToType.size() << " objects (max localId: " << maxLocalId << ")" << std::endl;
        
        // PR2: Enhanced Pass-0 - also build parent map for dependency analysis
        std::unordered_map<uint32_t, uint32_t> localIdToParent;
        for (const auto& objJson : abJson["objects"]) {
            if (objJson.contains("localId") && objJson.contains("parentId")) {
                uint32_t localId = objJson["localId"].get<uint32_t>();
                uint32_t parentId = objJson["parentId"].get<uint32_t>();
                if (skippedLocalIds.find(localId) == skippedLocalIds.end()) {
                    localIdToParent[localId] = parentId;
                }
            }
        }
        std::cout << "  Parent map: " << localIdToParent.size() << " parent relationships" << std::endl;

        auto parentTypeFor = [&](uint32_t parentLocalId) -> uint16_t {
            if (parentLocalId == invalidParent)
            {
                return 0;
            }
            if (parentLocalId == 0)
            {
                return 1; // Artboard
            }
            auto it = localIdToType.find(parentLocalId);
            return it != localIdToType.end() ? it->second : 0;
        };

        // PASS 1: Create objects with parent-first topological ordering
        std::cout << "  PASS 1: Sorting objects for parent-first emission..." << std::endl;
        
        // PR-KEYED-ORDER: Topological sort by parentId to ensure parents are created before children
        std::vector<nlohmann::json> sortedObjects;
        for (const auto& objJson : abJson["objects"]) {
            sortedObjects.push_back(objJson);
        }
        
        // Multi-pass sort: emit objects whose parents have already been emitted
        std::unordered_set<uint32_t> emittedLocalIds;
        emittedLocalIds.insert(0); // Artboard (parent of everything)
        
        std::vector<nlohmann::json> orderedObjects;
        size_t lastSize = 0;
        int pass = 0;
        
        while (!sortedObjects.empty() && sortedObjects.size() != lastSize && pass < 100) {
            lastSize = sortedObjects.size();
            std::vector<nlohmann::json> remaining;
            
            for (const auto& objJson : sortedObjects) {
                uint32_t parentId = objJson.contains("parentId") ? objJson["parentId"].get<uint32_t>() : 0;
                uint16_t typeKeyCandidate = objJson["typeKey"].get<uint16_t>();

                bool parentReady = (typeKeyCandidate == 1) || (emittedLocalIds.count(parentId) > 0);

                // Additional dependency: KeyedObject must wait for its animation target to be emitted
                if (parentReady && typeKeyCandidate == 25 && objJson.contains("properties")) {
                    const auto& props = objJson["properties"];
                    if (props.contains("objectId")) {
                        uint32_t targetLocalId = props["objectId"].get<uint32_t>();
                        if (emittedLocalIds.count(targetLocalId) == 0) {
                            parentReady = false;
                        }
                    }
                }

                // Emit if dependencies satisfied
                if (parentReady) {
                    orderedObjects.push_back(objJson);
                    if (objJson.contains("localId")) {
                        emittedLocalIds.insert(objJson["localId"].get<uint32_t>());
                    }
                } else {
                    remaining.push_back(objJson);
                }
            }
            
            sortedObjects = remaining;
            pass++;
        }
        
        if (!sortedObjects.empty()) {
            std::cerr << "  ⚠️  WARNING: " << sortedObjects.size() << " objects have forward/circular parent references (will emit anyway)" << std::endl;
            for (const auto& obj : sortedObjects) {
                orderedObjects.push_back(obj);
            }
        }
        
        std::cout << "  Topologically sorted " << orderedObjects.size() << " objects in " << pass << " passes" << std::endl;
        std::cout << "  PASS 1B: Creating objects in parent-first order..." << std::endl;
        
        // PR2: Diagnostic counters for keyed data
        std::map<uint16_t, int> keyedInJson;
        std::map<uint16_t, int> keyedCreated;
        int linearAnimCount = 0;
        int stateMachineCount = 0;
        
        bool skipKeyframeData = false; // Flag to cascade-skip KeyedProperty/KeyFrame after invalid KeyedObject
        CoreObject* lastKeyframe = nullptr; // Track last created InterpolatingKeyFrame for interpolatorId wiring
        
        for (const auto& objJson : orderedObjects) {
            uint16_t typeKey = objJson["typeKey"];
            bool isLinearAnimation = typeKey == kTypeKeyLinearAnimation;
            
            // PR2: Count keyed types in JSON
                if (typeKey == kTypeKeyKeyedObject || typeKey == kTypeKeyKeyedProperty || typeKey == 28 || typeKey == 30 || typeKey == 37 || 
                    typeKey == 50 || typeKey == 84 || typeKey == 138 || typeKey == 139 || typeKey == 142 ||
                    typeKey == 171 || typeKey == 174 || typeKey == 175 || typeKey == 450) {
                    keyedInJson[typeKey]++;
            }
            if (typeKey == 31) linearAnimCount++;
            if (typeKey == 53) stateMachineCount++;
            
            // PR2: Skip keyed set if OMIT_KEYED is enabled (A/B test for freeze isolation)
            if (OMIT_KEYED) {
                bool isKeyedType = typeKey == 25 ||  // KeyedObject
                                  typeKey == 26 ||  // KeyedProperty
                                  typeKey == 28 ||  // CubicEaseInterpolator
                                  typeKey == 30 ||  // KeyFrameDouble
                                  typeKey == 37 ||  // KeyFrameColor
                                  typeKey == 50 ||  // KeyFrameId
                                  typeKey == 84 ||  // KeyFrameBool
                                  typeKey == 138 || // CubicValueInterpolator
                                  typeKey == 139 || // CubicInterpolatorBase
                                  typeKey == 142 || // KeyFrameString
                                  typeKey == 171 || // KeyFrameCallback
                                  typeKey == 174 || // ElasticInterpolatorBase
                                  typeKey == 175 || // KeyFrameInterpolator (abstract)
                                  typeKey == 450;   // KeyFrameUint
                
                if (isKeyedType) {
                    // Count skipped keyed objects for diagnostic
                    continue; // Skip keyed data entirely for A/B test
                }
            }
            
            // PR2.3: Skip StateMachine objects if OMIT_STATE_MACHINE is enabled
            if (OMIT_STATE_MACHINE) {
                bool isSMType = typeKey == 53 ||  // StateMachine
                               typeKey == 56 ||  // StateMachineNumber
                               typeKey == 57 ||  // StateMachineLayer
                               typeKey == 58 ||  // StateMachineTrigger
                               typeKey == 59 ||  // StateMachineBool
                               typeKey == 61 ||  // AnimationState
                               typeKey == 62 ||  // AnyState
                               typeKey == 63 ||  // EntryState
                               typeKey == 64 ||  // ExitState
                               typeKey == 65;    // StateTransition
                
                if (isSMType) {
                    continue; // Skip SM data for PR2.3 test
                }
            }
            
            // Reset flag when we see a new animation/state machine (new context)
            if (typeKey == 31 || typeKey == 53) { // LinearAnimation or StateMachine
                skipKeyframeData = false;
                lastKeyframe = nullptr;
            }
            
            // Cascade-skip orphaned keyframe data (KeyedProperty, ALL KeyFrame types, ALL Interpolators)
            if (skipKeyframeData) {
                bool isKeyframeType = typeKey == 26 ||  // KeyedProperty
                                     typeKey == 28 ||  // CubicEaseInterpolator
                                     typeKey == 30 ||  // KeyFrameDouble
                                     typeKey == 37 ||  // KeyFrameColor
                                     typeKey == 50 ||  // KeyFrameId
                                     typeKey == 84 ||  // KeyFrameBool
                                     typeKey == 138 || // CubicValueInterpolator
                                     typeKey == 139 || // CubicInterpolatorBase
                                     typeKey == 142 || // KeyFrameString
                                     typeKey == 171 || // KeyFrameCallback
                                     typeKey == 174 || // ElasticInterpolatorBase
                                     typeKey == 175 || // KeyFrameInterpolator (base)
                                     typeKey == 450;   // KeyFrameUint
                
                if (isKeyframeType) {
                    std::cerr << "Cascade skip: Orphaned keyframe data typeKey=" << typeKey << std::endl;
                    continue;
                }
                // Non-keyframe type → reset flag
                skipKeyframeData = false;
                lastKeyframe = nullptr;
            }

            // Skip unsupported stub objects and their dependent children
            if (objJson.contains("__unsupported__") && objJson["__unsupported__"].get<bool>()) {
                std::cerr << "Skipping unsupported stub: typeKey=" << typeKey << std::endl;
                if (objJson.contains("localId")) {
                    skippedLocalIds.insert(objJson["localId"].get<uint32_t>());
                }
                continue;
            }
            
            // CASCADE SKIP: If parent is skipped, skip this child too
            if (objJson.contains("parentId")) {
                uint32_t candidateParent = objJson["parentId"];
                if (skippedLocalIds.count(candidateParent) > 0) {
                    std::cerr << "Cascade skip: typeKey=" << typeKey << " (parent " << candidateParent << " was skipped)" << std::endl;
                    if (objJson.contains("localId")) {
                        skippedLocalIds.insert(objJson["localId"].get<uint32_t>());
                    }
                    continue;
                }
            }

            // PR2-REVERTED: MUST skip when KeyedObject target missing
            // Emitting with objectId=0 causes runtime importer HANG (dangling reference)
            // Topological sort guarantees ordering, but filtered objects (TrimPath) still cause misses
            if (typeKey == 25) { // KeyedObject
                if (objJson.contains("properties") && objJson["properties"].contains("objectId")) {
                    uint32_t targetLocalId = objJson["properties"]["objectId"].get<uint32_t>();
                    auto it = localIdToBuilderObjectId.find(targetLocalId);
                    if (it == localIdToBuilderObjectId.end()) {
                        // Topological sort + TrimPath retention should guarantee this exists.
                        // Emit diagnostic so we can track unexpected misses but do NOT skip –
                        // dropping here resurrects the original 230 NULL-object regression.
                        std::cerr << "⚠️  WARNING: KeyedObject targets missing localId=" << targetLocalId
                                  << " (please investigate extractor ordering)" << std::endl;
                    }
                }
            }
            
            rive::Core* coreObj = createObjectByTypeKey(typeKey);
            if (!coreObj) {
                std::cerr << "Skipping unknown type: " << typeKey << std::endl;
                continue;
            }

            // Snapshot properties so we can route them between synthetic shape and original object
            std::vector<std::pair<std::string, nlohmann::json>> properties;
            if (objJson.contains("properties")) {
                for (const auto& [key, value] : objJson["properties"].items()) {
                    properties.emplace_back(key, value);
                }
            }

            uint32_t parentLocalId = objJson.contains("parentId")
                                         ? objJson["parentId"].get<uint32_t>()
                                         : invalidParent;
            if (parentLocalId != invalidParent) {
                auto remap = parentRemap.find(parentLocalId);
                if (remap != parentRemap.end()) {
                    // PR2: Paint-only remap whitelist
                    if (isPaintOrDecorator(typeKey)) {
                        parentLocalId = remap->second;
                        paintsMoved++;
                    }
                    // PR2: Blacklist enforcement - track attempts
                    else if (isVertexType(typeKey)) {
                        vertexRemapAttempted++;
                        verticesKept++;
                        // Vertices keep original PointsPath parent
                    }
                    else if (isAnimGraphType(typeKey)) {
                        animNodeRemapAttempted++;
                        // Animation nodes keep original parent
                    }
                }
            }

            std::optional<uint32_t> localId;
            if (objJson.contains("localId")) {
                localId = objJson["localId"].get<uint32_t>();
            }

            std::vector<uint16_t> originalContextPath;
            if (typeKey == rive::DataBindContext::typeKey)
            {
                for (const auto& [key, value] : properties)
                {
                    if (key == "sourcePathIds" && value.is_array())
                    {
                        for (const auto& entry : value)
                        {
                            originalContextPath.push_back(
                                static_cast<uint16_t>(entry.get<uint32_t>() & 0xFFFFu));
                        }
                    }
                }
                if (localId.has_value())
                {
                    dataBindContextOriginalPaths[*localId] = originalContextPath;
                }
            }

            // PR-KEYED-ORDER: Inject Shape container for:
            // 1. Parametric paths (Rectangle/Ellipse/etc) with non-Shape parent
            // 2. Top-level paints (Fill/Stroke) with non-Shape parent
            bool needsShapeContainer = false;
            uint16_t pType = parentTypeFor(parentLocalId);
            
            if (isParametricPathType(typeKey) && pType != 3) {
                needsShapeContainer = true;
            } else if (isTopLevelPaint(typeKey) && parentLocalId != invalidParent) {
                // For Fill/Stroke: check if parent is NOT Shape (3)
                // Parent can be: Artboard (1), Node (2), or unknown types
                if (pType != 3) {
                    needsShapeContainer = true;
                    std::cout << "  [auto] Paint typeKey=" << typeKey 
                              << " localId=" << (localId.has_value() ? *localId : 0)
                              << " parent=" << parentLocalId 
                              << " (type=" << pType << ") → inject Shape" << std::endl;
                }
            }

            std::unordered_set<std::string> consumedKeys;
            if (needsShapeContainer) {
                uint32_t shapeLocalId = nextSyntheticLocalId++;
                auto& shapeObj = builder.addCore(new rive::Shape());

                // CRITICAL: Set drawable properties so Rive Play renders the shape!
                builder.set(shapeObj, 23, static_cast<uint32_t>(3));  // blendModeValue = SrcOver
                builder.set(shapeObj, 129, static_cast<uint32_t>(4)); // drawableFlags = visible (4)

                pendingObjects.push_back({&shapeObj, 3, shapeLocalId, parentLocalId});
                localIdToBuilderObjectId[shapeLocalId] = shapeObj.id;
                localIdToType[shapeLocalId] = 3;

                // Set properties on new Shape to mirror the original path transform/name.
                for (const auto& [key, value] : properties) {
                    if (key == "x") {
                        builder.set(shapeObj, 13, value.get<float>());
                        consumedKeys.insert(key);
                    }
                    else if (key == "y") {
                        builder.set(shapeObj, 14, value.get<float>());
                        consumedKeys.insert(key);
                    }
                    else if (key == "rotation") {
                        builder.set(shapeObj, 15, value.get<float>());
                        consumedKeys.insert(key);
                    }
                    else if (key == "scaleX") {
                        builder.set(shapeObj, 16, value.get<float>());
                        consumedKeys.insert(key);
                    }
                    else if (key == "scaleY") {
                        builder.set(shapeObj, 17, value.get<float>());
                        consumedKeys.insert(key);
                    }
                    else if (key == "opacity") {
                        builder.set(shapeObj, 18, value.get<float>());
                        consumedKeys.insert(key);
                    }
                    else if (key == "name") {
                        builder.set(shapeObj, 4, value.get<std::string>());
                        consumedKeys.insert(key);
                    }
                }

                std::cout << "  [auto] Inserted Shape container (localId " << shapeLocalId
                          << ") for parametric path localId "
                          << (localId.has_value() ? *localId : 0u) << std::endl;

                parentLocalId = shapeLocalId;

                // PR2: Retroactively remap ONLY paint/decorator objects (NOT vertices/anim!)
                // that were already created and point to this parametric path
                if (localId.has_value()) {
                    uint32_t pathLocalId = *localId;
                    
                    for (auto& pending : pendingObjects) {
                        // PR2: Paint whitelist - only these get remapped
                        if (isPaintOrDecorator(pending.typeKey) && 
                            pending.parentLocalId == pathLocalId) {
                            pending.parentLocalId = shapeLocalId;
                            paintsMoved++;
                        }
                        // PR2: Vertex blacklist - explicitly keep with path
                        else if (isVertexType(pending.typeKey) && 
                                 pending.parentLocalId == pathLocalId) {
                            verticesKept++;
                            // Keep original parent (path)
                        }
                    }
                    
                    // Future objects referencing this path:
                    // - If paint/decorator: remap to Shape
                    // - If vertex/anim: keep original path (handled above)
                    parentRemap[pathLocalId] = shapeLocalId;
                    shapeInserted++;
                }
            }
            
            if (typeKey == rive::FileAssetContents::typeKey)
            {
                std::vector<uint8_t> decodedBytes;
                if (objJson.contains("properties"))
                {
                    const auto& props = objJson["properties"];
                    if (props.contains("bytes") && props["bytes"].is_string())
                    {
                        decodedBytes = decode_base64(props["bytes"].get<std::string>());
                    }
                }
                if (!decodedBytes.empty())
                {
                    embeddedFontData = std::move(decodedBytes);
                }
                delete coreObj;
                continue;
            }

            auto& obj = builder.addCore(coreObj);

            // Ensure drawable objects are visible/renderable by default
            if (obj.isDrawable) {
                builder.set(obj, 23, static_cast<uint32_t>(3));  // blendModeValue = SrcOver
                builder.set(obj, 129, static_cast<uint32_t>(4)); // drawableFlags = Visible
            }
            
            // Set Artboard name and size from artboard-level JSON
            if (typeKey == 1 && abJson.contains("name")) { // Artboard
                builder.set(obj, 4, abJson["name"].get<std::string>()); // name
                builder.set(obj, 7, abJson["width"].get<float>()); // width
                builder.set(obj, 8, abJson["height"].get<float>()); // height
                // CRITICAL FIX: Always enable clipping for round-trip files!
                // Original Rive files may have clip=false, but round-trip hierarchical format
                // requires clipping to prevent grey screen (objects outside artboard bounds)
                bool clipEnabled = true;  // FORCE TRUE for round-trip compatibility
                // Ignore JSON clip value - always enable for safety
                builder.set(obj, 196, clipEnabled);
            }

            pendingObjects.push_back({&obj, typeKey, localId, parentLocalId});

            if (typeKey == rive::DataBindContext::typeKey) {
                std::vector<uint16_t> path;
                if (localId.has_value())
                {
                    auto itPath = dataBindContextOriginalPaths.find(*localId);
                    if (itPath != dataBindContextOriginalPaths.end())
                    {
                        path = itPath->second;
                    }
                }
                dataBindContexts.push_back({&obj, std::move(path)});
            }
            
            // PR3: Track animation graph objects
            if (typeKey == kTypeKeyKeyedObject) keyedObjectCount++;
            else if (typeKey == kTypeKeyKeyedProperty) keyedPropertyCount++;
            else if (typeKey == 30 || typeKey == 37 || typeKey == 50 || typeKey == 84 || typeKey == 142 || typeKey == 450) {
                keyFrameCount++;
            }
            else if (typeKey == 28 || typeKey == 138 || typeKey == 139 || typeKey == 174) {
                interpolatorCount++;
            }

            // If we just created a keyframe, remember it for interpolatorId wiring
            if (typeKey == 30 || // KeyFrameDouble
                typeKey == 37 || // KeyFrameColor
                typeKey == 50 || // KeyFrameId
                typeKey == 84 || // KeyFrameBool
                typeKey == 142 || // KeyFrameString
                typeKey == 450)   // KeyFrameUint
            {
                lastKeyframe = &obj;
            }
            // If we just created an interpolator and have a pending keyframe, wire it
            if ((typeKey == 28 || typeKey == 138 || typeKey == 139 || typeKey == 174) && lastKeyframe != nullptr && !skipKeyframeData)
            {
                builder.set(*lastKeyframe, 69, obj.id); // InterpolatingKeyFrame.interpolatorId
                lastKeyframe = nullptr;
            }

            if (localId) {
                localIdToBuilderObjectId[*localId] = obj.id;
                localIdToType[*localId] = typeKey;
                if (isLinearAnimation) {
                    animationLocalIdsInOrder.push_back(*localId);
                }
            }
            
            // PR2d: Forward reference guard - skip objects with missing parents
            // This prevents MALFORMED errors from forward references in truncated JSON
            if (parentLocalId != invalidParent && 
                localIdToBuilderObjectId.find(parentLocalId) == localIdToBuilderObjectId.end()) {
                // Parent doesn't exist yet - this is a forward reference
                std::cerr << "  ⚠️  Skipping object typeKey=" << typeKey 
                          << " localId=" << (localId.has_value() ? *localId : 0)
                          << ", forward reference to missing parent=" << parentLocalId << std::endl;
                if (localId) {
                    skippedLocalIds.insert(*localId);
                }
                continue; // Skip creating this object entirely
            }
            
            // PR2d: TrimPath sanitization - check parent type and inject defaults
            if (typeKey == 47) { // TrimPath
                // Validate parent type (must be Fill or Stroke)
                if (parentLocalId != invalidParent) {
                    uint16_t parentType = localIdToType[parentLocalId];
                    if (parentType != 20 && parentType != 24) { // Not Fill or Stroke
                        std::cerr << "  ⚠️  Skipping TrimPath localId=" << (localId.has_value() ? *localId : 0)
                                  << ", invalid parent type=" << parentType << " (expected Fill(20) or Stroke(24))" << std::endl;
                        if (localId) {
                            skippedLocalIds.insert(*localId);
                        }
                        continue; // Skip creating this object entirely
                    }
                }
                
                // Parent is valid - inject defaults if properties empty
                bool hasStart = false, hasEnd = false, hasOffset = false, hasModeValue = false;
                for (const auto& [key, value] : properties) {
                    if (key == "start") hasStart = true;
                    else if (key == "end") hasEnd = true;
                    else if (key == "offset") hasOffset = true;
                    else if (key == "modeValue" || key == "mode") hasModeValue = true;
                }
                
                if (!hasStart || !hasEnd || !hasOffset || !hasModeValue) {
                    if (!hasStart) builder.set(obj, 114, 0.0f);  // start
                    if (!hasEnd) builder.set(obj, 115, 0.0f);    // end
                    if (!hasOffset) builder.set(obj, 116, 0.0f); // offset
                    if (!hasModeValue)
                    {
                        builder.set(obj,
                                     117,
                                     static_cast<uint32_t>(
                                         rive::TrimPathMode::sequential));
                    }
                    
                    std::cout << "  ℹ️  TrimPath localId=" << (localId.has_value() ? *localId : 0)
                              << " → defaults injected (114,115,116,117)" << std::endl;
                }
            }

            if (typeKey == 165) { // FollowPathConstraint
                bool hasDistance = false;
                bool hasOrient = false;
                bool hasOffset = false;
                bool hasTargetId = false;
                bool hasSourceSpace = false;
                bool hasDestSpace = false;

                for (const auto& [key, value] : properties) {
                    if (key == "distance") hasDistance = true;
                    else if (key == "orient") hasOrient = true;
                    else if (key == "offset") hasOffset = true;
                    else if (key == "targetId") {
                        hasTargetId = true;
                        // Note: targetId already set in PASS1 if -1, or deferred if valid
                    }
                    else if (key == "sourceSpaceValue") hasSourceSpace = true;
                    else if (key == "destSpaceValue") hasDestSpace = true;
                }

                // FollowPathConstraint specific properties
                if (!hasDistance) {
                    builder.set(obj, 363, 0.0f);
                }
                if (!hasOrient) {
                    builder.set(obj, 364, true);
                }
                if (!hasOffset) {
                    builder.set(obj, 365, false);
                }
                
                // TargetedConstraint base property (CRITICAL!)
                if (!hasTargetId) {
                    // Default: -1 (Core.missingId) = 0xFFFFFFFF as uint
                    builder.set(obj, 173, static_cast<uint32_t>(-1));
                }
                
                // TransformSpaceConstraint base properties
                if (!hasSourceSpace) {
                    builder.set(obj, 179, static_cast<uint32_t>(0));
                }
                if (!hasDestSpace) {
                    builder.set(obj, 180, static_cast<uint32_t>(0));
                }

                if (!hasDistance || !hasOrient || !hasOffset || 
                    !hasTargetId || !hasSourceSpace || !hasDestSpace) {
                    std::cout << "  ℹ️  FollowPathConstraint localId="
                              << (localId.has_value() ? *localId : 0)
                              << " → defaults injected (173,179,180,363,364,365)" << std::endl;
                }
            }
            
            // PR2: Count created keyed objects (when not omitted)
            if (!OMIT_KEYED) {
                if (typeKey == kTypeKeyKeyedObject || typeKey == kTypeKeyKeyedProperty || typeKey == 28 || typeKey == 30 || typeKey == 37 || 
                    typeKey == 50 || typeKey == 84 || typeKey == 138 || typeKey == 139 || typeKey == 142 ||
                    typeKey == 171 || typeKey == 174 || typeKey == 175 || typeKey == 450) {
                    keyedCreated[typeKey]++;
                }
            }

            for (const auto& [key, value] : properties) {
                if (consumedKeys.count(key) != 0) {
                    continue; // Already applied to synthetic Shape container
                }

                if (key == "x") {
                    if (typeKey == 5 || typeKey == 6 || typeKey == 35) {
                        builder.set(obj, 24, value.get<float>());
                    } else {
                        builder.set(obj, 13, value.get<float>());
                    }
                }
                else if (key == "y") {
                    if (typeKey == 5 || typeKey == 6 || typeKey == 35) {
                        builder.set(obj, 25, value.get<float>());
                    } else {
                        builder.set(obj, 14, value.get<float>());
                    }
                }
                else if (key == "width") {
                    if (typeKey == 1) {
                        builder.set(obj, 7, value.get<float>());
                    } else if (typeKey == 134) {
                        builder.set(obj, 285, value.get<float>());
                    } else {
                        builder.set(obj, 20, value.get<float>());
                    }
                }
                else if (key == "height") {
                    if (typeKey == 1) {
                        builder.set(obj, 8, value.get<float>());
                    } else if (typeKey == 134) {
                        builder.set(obj, 286, value.get<float>());
                    } else {
                        builder.set(obj, 21, value.get<float>());
                    }
                }
                else if (key == "styleId" && value.is_number()) {
                    int64_t styleLocalId = value.get<int64_t>();
                    if (styleLocalId < 0) {
                        builder.set(obj, 272, static_cast<uint32_t>(styleLocalId));
                    } else {
                        deferredComponentRefs.push_back({&obj, 272, static_cast<uint32_t>(styleLocalId)});
                    }
                }
                else if (key == "fontAssetId" && value.is_number()) {
                    int64_t fontLocalId = value.get<int64_t>();
                    if (fontLocalId < 0) {
                        builder.set(obj, 279, static_cast<uint32_t>(fontLocalId));
                    } else {
                        deferredComponentRefs.push_back({&obj, 279, static_cast<uint32_t>(fontLocalId)});
                    }
                }
                else if ((key == "mode" || key == "modeValue") &&
                         typeKey == rive::TrimPath::typeKey)
                {
                    uint32_t rawMode = value.get<uint32_t>();
                    if (key == "mode")
                    {
                        rawMode = rawMode == 0
                                      ? static_cast<uint32_t>(
                                            rive::TrimPathMode::sequential)
                                      : static_cast<uint32_t>(
                                            rive::TrimPathMode::synchronized);
                    }
                    if (rawMode == 0)
                    {
                        rawMode = static_cast<uint32_t>(
                            rive::TrimPathMode::sequential);
                    }
                    builder.set(obj, 117, rawMode);
                }
                // Defer targetId for PASS3 (needs complete object ID mapping)
                else if (key == "targetId" && value.is_number()) {
                        // Use int64_t to preserve full uint32_t range while detecting -1 sentinel
                        int64_t targetIdWide = value.get<int64_t>();
                        if (targetIdWide == -1) {
                            // Missing target sentinel - set default immediately
                            builder.set(obj, 173, static_cast<uint32_t>(-1));
                        } else if (targetIdWide >= 0 && targetIdWide <= 0xFFFFFFFF) {
                            // Valid target in uint32_t range - defer for remapping in PASS3
                            deferredTargetIds.push_back({&obj, static_cast<uint32_t>(targetIdWide)});
                        }
                        // Out-of-range values are invalid, skip silently
                    }
                else if (key == "name" && isLinearAnimation && value.is_string()) {
                    std::string animName = value.get<std::string>();
                    builder.set(obj, rive::AnimationBase::namePropertyKey, animName);
                    if (localId.has_value()) {
                        animationNameToLocalId[animName] = *localId;
                    }
                    animationNameToBuilderId[animName] = obj.id;
                }
                else {
                    setProperty(builder, obj, key, value, localIdToBuilderObjectId, objectIdRemapSuccess, objectIdRemapFail);
                }
            }
            
            // PR-DRAWTARGET: DrawTarget properties (read directly from JSON)
            if (typeKey == 48 && objJson.contains("properties")) { // DrawTarget
                const auto& props = objJson["properties"];
                if (props.contains("drawableId")) {
                    uint32_t drawableId = props["drawableId"].get<uint32_t>();
                    // Defer for PASS 3 remapping (needs component ID translation)
                    deferredComponentRefs.push_back({&obj, 119, drawableId});
                }
                if (props.contains("placementValue")) {
                    uint32_t placement = props["placementValue"].get<uint32_t>();
                    builder.set(obj, 120, placement);
                }
            }
            
            // PR-DRAWTARGET: DrawRules properties (read directly from JSON)
            if (typeKey == 49 && objJson.contains("properties")) { // DrawRules
                const auto& props = objJson["properties"];
                if (props.contains("drawTargetId")) {
                    uint32_t targetId = props["drawTargetId"].get<uint32_t>();
                    // Defer for PASS 3 remapping
                    deferredComponentRefs.push_back({&obj, 121, targetId});
                }
            }
            
            // CRITICAL FIX: KeyFrame interpolatorId (read directly from JSON, defer for PASS3 remap)
            if ((typeKey == 30 || // KeyFrameDouble
                 typeKey == 37 || // KeyFrameColor
                 typeKey == 50 || // KeyFrameId
                 typeKey == 84 || // KeyFrameBool
                 typeKey == 142 || // KeyFrameString
                 typeKey == 450)   // KeyFrameUint
                 && objJson.contains("properties")) {
                const auto& props = objJson["properties"];
                if (props.contains("interpolatorId")) {
                    uint32_t interpolatorLocalId = props["interpolatorId"].get<uint32_t>();
                    // Defer for PASS 3 remapping (localId → component ID)
                    deferredComponentRefs.push_back({&obj, 69, interpolatorLocalId});
                }
            }
        }

        int hierarchicalAnimationsCreated = 0;
        int hierarchicalKeyedObjectsCreated = 0;
        int hierarchicalKeyedPropertiesCreated = 0;
        int hierarchicalKeyframesCreated = 0;

        if (abJson.contains("animations") && abJson["animations"].is_array())
        {
            const auto& animationsJson = abJson["animations"];
            if (!animationsJson.empty())
            {
                std::cout << "  PASS 1B: Integrating " << animationsJson.size()
                          << " hierarchical animation definitions" << std::endl;
            }

            for (const auto& animJson : animationsJson)
            {
                rive_converter::AnimationData animData = rive_converter::parse_animation_json(animJson);

                auto& animationObj = builder.addCore(new rive::LinearAnimation());
                uint32_t animationLocalId = nextSyntheticLocalId++;

                builder.set(animationObj, rive::AnimationBase::namePropertyKey, animData.name);
                builder.set(animationObj, rive::LinearAnimationBase::fpsPropertyKey, animData.fps);
                builder.set(animationObj, rive::LinearAnimationBase::durationPropertyKey, animData.duration);
                builder.set(animationObj, rive::LinearAnimationBase::loopValuePropertyKey, animData.loop);

                if (animData.speed.has_value())
                {
                    builder.set(animationObj, rive::LinearAnimationBase::speedPropertyKey, *animData.speed);
                }
                if (animData.workStart.has_value())
                {
                    builder.set(animationObj, rive::LinearAnimationBase::workStartPropertyKey, *animData.workStart);
                }
                if (animData.workEnd.has_value())
                {
                    builder.set(animationObj, rive::LinearAnimationBase::workEndPropertyKey, *animData.workEnd);
                }
                if (animData.enableWorkArea.has_value())
                {
                    builder.set(animationObj, rive::LinearAnimationBase::enableWorkAreaPropertyKey,
                                *animData.enableWorkArea);
                }
                if (animData.quantize.has_value())
                {
                    builder.set(animationObj, rive::LinearAnimationBase::quantizePropertyKey,
                                static_cast<bool>(*animData.quantize));
                }

                pendingObjects.push_back({&animationObj, kTypeKeyLinearAnimation, animationLocalId, invalidParent});
                localIdToBuilderObjectId[animationLocalId] = animationObj.id;
                localIdToType[animationLocalId] = kTypeKeyLinearAnimation;
                animationLocalIdsInOrder.push_back(animationLocalId);
                if (!animData.name.empty())
                {
                    animationNameToLocalId[animData.name] = animationLocalId;
                    animationNameToBuilderId[animData.name] = animationObj.id;
                }

                linearAnimCount++;
                hierarchicalAnimationsCreated++;

                for (const auto& keyedObjectData : animData.keyedObjects)
                {
                    auto& keyedObject = builder.addCore(new rive::KeyedObject());
                    uint32_t keyedObjectLocalId = nextSyntheticLocalId++;

                    pendingObjects.push_back({&keyedObject, kTypeKeyKeyedObject, keyedObjectLocalId, invalidParent});
                    localIdToBuilderObjectId[keyedObjectLocalId] = keyedObject.id;
                    localIdToType[keyedObjectLocalId] = kTypeKeyKeyedObject;
                    keyedObjectCount++;
                    hierarchicalKeyedObjectsCreated++;
                    if (!OMIT_KEYED)
                    {
                        keyedCreated[kTypeKeyKeyedObject]++;
                    }

                    if (keyedObjectData.objectId != 0)
                    {
                        deferredComponentRefs.push_back({&keyedObject, 51, keyedObjectData.objectId});
                    }

                    for (const auto& keyedPropertyData : keyedObjectData.keyedProperties)
                    {
                        auto& keyedProperty = builder.addCore(new rive::KeyedProperty());
                        uint32_t keyedPropertyLocalId = nextSyntheticLocalId++;

                        pendingObjects.push_back({&keyedProperty, kTypeKeyKeyedProperty, keyedPropertyLocalId,
                                                 invalidParent});
                        localIdToBuilderObjectId[keyedPropertyLocalId] = keyedProperty.id;
                        localIdToType[keyedPropertyLocalId] = kTypeKeyKeyedProperty;
                        builder.set(keyedProperty, rive::KeyedPropertyBase::propertyKeyPropertyKey,
                                    keyedPropertyData.propertyKey);
                        keyedPropertyCount++;
                        hierarchicalKeyedPropertiesCreated++;
                        if (!OMIT_KEYED)
                        {
                            keyedCreated[kTypeKeyKeyedProperty]++;
                        }

                        int propertyFieldType = rive::CoreDoubleType::id;
                        auto typeIt = typeMap.find(keyedPropertyData.propertyKey);
                        if (typeIt != typeMap.end())
                        {
                            propertyFieldType = typeIt->second;
                        }

                        for (const auto& keyframeData : keyedPropertyData.keyframes)
                        {
                            uint16_t keyframeTypeKey = selectKeyFrameType(keyframeData, propertyFieldType);
                            rive::Core* keyframeCore = nullptr;
                            switch (keyframeTypeKey)
                            {
                                case rive::KeyFrameColor::typeKey:
                                    keyframeCore = new rive::KeyFrameColor();
                                    break;
                                case rive::KeyFrameBool::typeKey:
                                    keyframeCore = new rive::KeyFrameBool();
                                    break;
                                case rive::KeyFrameString::typeKey:
                                    keyframeCore = new rive::KeyFrameString();
                                    break;
                                case rive::KeyFrameUint::typeKey:
                                    keyframeCore = new rive::KeyFrameUint();
                                    break;
                                case rive::KeyFrameId::typeKey:
                                    keyframeCore = new rive::KeyFrameId();
                                    break;
                                case rive::KeyFrameDouble::typeKey:
                                default:
                                    keyframeCore = new rive::KeyFrameDouble();
                                    keyframeTypeKey = rive::KeyFrameDouble::typeKey;
                                    break;
                            }

                            auto& keyframeObj = builder.addCore(keyframeCore);
                            pendingObjects.push_back({&keyframeObj, keyframeTypeKey, std::nullopt, invalidParent});

                            builder.set(keyframeObj, rive::KeyFrameBase::framePropertyKey, keyframeData.frame);
                            builder.set(keyframeObj, rive::InterpolatingKeyFrameBase::interpolationTypePropertyKey,
                                        keyframeData.interpolationType);

                            if (keyframeData.interpolatorId.has_value())
                            {
                                deferredComponentRefs.push_back(
                                    {&keyframeObj, rive::InterpolatingKeyFrameBase::interpolatorIdPropertyKey,
                                     *keyframeData.interpolatorId});
                            }

                            applyKeyFrameValue(builder, keyframeObj, keyframeTypeKey, keyframeData, propertyFieldType);

                            keyFrameCount++;
                            hierarchicalKeyframesCreated++;
                            if (!OMIT_KEYED)
                            {
                                keyedCreated[keyframeTypeKey]++;
                            }
                        }
                    }
                }
            }
        }

        if (hierarchicalAnimationsCreated > 0)
        {
            std::cout << "  → Added " << hierarchicalAnimationsCreated << " animations"
                      << " (keyedObjects=" << hierarchicalKeyedObjectsCreated
                      << ", keyedProperties=" << hierarchicalKeyedPropertiesCreated
                      << ", keyframes=" << hierarchicalKeyframesCreated << ")" << std::endl;
        }

        // Build animation localId → index map (artboard-local ordering)
        animationLocalIdToIndex.clear();
        for (size_t idx = 0; idx < animationLocalIdsInOrder.size(); ++idx)
        {
            animationLocalIdToIndex[animationLocalIdsInOrder[idx]] = static_cast<uint32_t>(idx);
        }

        animationNameToIndex.clear();
        for (const auto& [name, localId] : animationNameToLocalId)
        {
            auto idxIt = animationLocalIdToIndex.find(localId);
            if (idxIt != animationLocalIdToIndex.end())
            {
                animationNameToIndex[name] = idxIt->second;
            }
        }

        // PASS 1C: Flatten hierarchical state machine definitions into runtime objects
        if (abJson.contains("stateMachines") && abJson["stateMachines"].is_array()) {
            if (!artboardLocalIdValid) {
                std::cerr << "  ⚠️  Unable to flatten state machines: missing artboard localId" << std::endl;
            } else {
                for (const auto& smJson : abJson["stateMachines"]) {
                    StateMachineBindingInfo bindingInfo;

                    auto& smObj = builder.addCore(new rive::StateMachine());
                    if (smJson.contains("name")) {
                        builder.set(smObj,
                                    rive::AnimationBase::namePropertyKey,
                                    smJson["name"].get<std::string>());
                    }

                    uint32_t smLocalId = nextSyntheticLocalId++;
                    pendingObjects.push_back({&smObj, rive::StateMachine::typeKey, smLocalId, artboardLocalId});
                    localIdToBuilderObjectId[smLocalId] = smObj.id;
                    localIdToType[smLocalId] = rive::StateMachine::typeKey;
                    stateMachineCount++;
                    bindingInfo.stateMachine = &smObj;

                    if (smJson.contains("inputs") && smJson["inputs"].is_array()) {
                        for (const auto& inputJson : smJson["inputs"]) {
                            std::string inputType = inputJson.value("type", std::string("number"));
                            std::string inputName = inputJson.value("name", std::string());

                            CoreObject* inputCore = nullptr;
                            uint16_t inputTypeKey = 0;

                            if (inputType == "number") {
                                auto& numberObj = builder.addCore(new rive::StateMachineNumber());
                                if (!inputName.empty()) {
                                    builder.set(numberObj,
                                                rive::StateMachineComponentBase::namePropertyKey,
                                                inputName);
                                }
                                double defaultValue = inputJson.value("value",
                                                                     inputJson.value("defaultValue", 0.0));
                                builder.set(numberObj,
                                            rive::StateMachineNumberBase::valuePropertyKey,
                                            static_cast<float>(defaultValue));
                                inputCore = &numberObj;
                                inputTypeKey = rive::StateMachineNumber::typeKey;
                            }
                            else if (inputType == "bool") {
                                auto& boolObj = builder.addCore(new rive::StateMachineBool());
                                if (!inputName.empty()) {
                                    builder.set(boolObj,
                                                rive::StateMachineComponentBase::namePropertyKey,
                                                inputName);
                                }
                                bool defaultValue = inputJson.value("value",
                                                                   inputJson.value("defaultValue", false));
                                builder.set(boolObj,
                                            rive::StateMachineBoolBase::valuePropertyKey,
                                            defaultValue);
                                inputCore = &boolObj;
                                inputTypeKey = rive::StateMachineBool::typeKey;
                            }
                            else if (inputType == "trigger") {
                                auto& triggerObj = builder.addCore(new rive::StateMachineTrigger());
                                if (!inputName.empty()) {
                                    builder.set(triggerObj,
                                                rive::StateMachineComponentBase::namePropertyKey,
                                                inputName);
                                }
                                inputCore = &triggerObj;
                                inputTypeKey = rive::StateMachineTrigger::typeKey;
                            }

                            if (inputCore == nullptr) {
                                continue;
                            }

                            uint32_t inputLocalId = nextSyntheticLocalId++;
                            pendingObjects.push_back({inputCore, inputTypeKey, inputLocalId, smLocalId});
                            localIdToBuilderObjectId[inputLocalId] = inputCore->id;
                            localIdToType[inputLocalId] = inputTypeKey;

                            if (!inputName.empty()) {
                                bindingInfo.inputsByName[inputName] = inputCore;
                            }
                            bindingInfo.inputList.push_back(inputCore);
                        }
                    }

                    auto emitLayerStates = [&](uint32_t layerLocalId,
                                               const std::string& layerName,
                                               const nlohmann::json* statesArray) {
                        std::unordered_map<std::string, uint32_t> layerStateLocalIds;

                        auto registerAlias = [&](const std::string& key,
                                                 uint32_t localId,
                                                 CoreObject* stateCore) {
                            if (key.empty()) {
                                return;
                            }
                            layerStateLocalIds[key] = localId;
                            bindingInfo.statesByName[key] = stateCore;
                        };

                        auto addState = [&](rive::LayerState* state,
                                            uint16_t typeKey,
                                            const std::string& displayName,
                                            const std::vector<std::string>& aliases) -> CoreObject* {
                            auto& stateObj = builder.addCore(state);
                            if (!displayName.empty()) {
                                builder.set(stateObj,
                                            rive::StateMachineComponentBase::namePropertyKey,
                                            displayName);
                            }
                            uint32_t stateLocalId = nextSyntheticLocalId++;
                            pendingObjects.push_back({&stateObj, typeKey, stateLocalId, layerLocalId});
                            localIdToBuilderObjectId[stateLocalId] = stateObj.id;
                            localIdToType[stateLocalId] = typeKey;
                            bindingInfo.stateList.push_back(&stateObj);

                            if (!displayName.empty()) {
                                registerAlias(displayName, stateLocalId, &stateObj);
                            }
                            for (const auto& alias : aliases) {
                                if (!alias.empty() && alias != displayName) {
                                    registerAlias(alias, stateLocalId, &stateObj);
                                }
                            }

                            return &stateObj;
                        };

                        auto renameState = [&](const std::string& canonicalKey,
                                               const std::string& newName) {
                            if (newName.empty()) {
                                return;
                            }
                            auto statePtrIt = bindingInfo.statesByName.find(canonicalKey);
                            if (statePtrIt == bindingInfo.statesByName.end()) {
                                return;
                            }
                            CoreObject* stateCore = statePtrIt->second;
                            if (stateCore == nullptr) {
                                return;
                            }
                            builder.set(*stateCore,
                                        rive::StateMachineComponentBase::namePropertyKey,
                                        newName);
                            auto localIdIt = layerStateLocalIds.find(canonicalKey);
                            if (localIdIt != layerStateLocalIds.end()) {
                                registerAlias(newName, localIdIt->second, stateCore);
                            }
                        };

                        const std::string entryDisplay = layerName.empty() ? std::string("Entry") : layerName + " Entry";
                        const std::string exitDisplay = layerName.empty() ? std::string("Exit") : layerName + " Exit";
                        const std::string anyDisplay = layerName.empty() ? std::string("Any") : layerName + " Any";

                        addState(new rive::EntryState(),
                                 rive::EntryState::typeKey,
                                 entryDisplay,
                                 {"Entry"});
                        addState(new rive::AnyState(),
                                 rive::AnyState::typeKey,
                                 anyDisplay,
                                 {"Any"});
                        addState(new rive::ExitState(),
                                 rive::ExitState::typeKey,
                                 exitDisplay,
                                 {"Exit"});

                        if (statesArray != nullptr && statesArray->is_array()) {
                            for (const auto& stateJson : *statesArray) {
                                uint16_t stateTypeKey = 0;
                                if (stateJson.contains("typeKey") && stateJson["typeKey"].is_number_unsigned()) {
                                    stateTypeKey = static_cast<uint16_t>(stateJson["typeKey"].get<uint32_t>());
                                }

                                std::string stateType;
                                if (stateJson.contains("type") && stateJson["type"].is_string()) {
                                    stateType = stateJson["type"].get<std::string>();
                                }

                                if (stateType.empty()) {
                                    switch (stateTypeKey)
                                    {
                                        case rive::EntryState::typeKey:
                                            stateType = "entry";
                                            break;
                                        case rive::ExitState::typeKey:
                                            stateType = "exit";
                                            break;
                                        case rive::AnyState::typeKey:
                                            stateType = "any";
                                            break;
                                        default:
                                            stateType = "animation";
                                            break;
                                    }
                                }

                                std::string explicitName = stateJson.value("name", std::string());

                                if (stateType == "animation") {
                                    std::string animationName = stateJson.value("animationName", std::string());
                                    std::string stateDisplayName = !explicitName.empty() ? explicitName : animationName;
                                    if (stateDisplayName.empty()) {
                                        stateDisplayName = layerName.empty() ? std::string("Animation State")
                                                                             : layerName + " Animation";
                                    }

                                    std::vector<std::string> aliases;
                                    if (!stateDisplayName.empty()) {
                                        aliases.push_back(stateDisplayName);
                                    }
                                    if (!animationName.empty() && animationName != stateDisplayName) {
                                        aliases.push_back(animationName);
                                    }

                                    CoreObject* animStateCore = addState(new rive::AnimationState(),
                                                                         rive::AnimationState::typeKey,
                                                                         stateDisplayName,
                                                                         aliases);

                                    uint32_t resolvedAnimationIndex = std::numeric_limits<uint32_t>::max();
                                    bool animationResolved = false;

                                    if (!animationName.empty()) {
                                        auto idxIt = animationNameToIndex.find(animationName);
                                        if (idxIt != animationNameToIndex.end()) {
                                            resolvedAnimationIndex = idxIt->second;
                                            animationResolved = true;
                                        } else {
                                            std::cerr << "  ⚠️  StateMachine '"
                                                      << smJson.value("name", std::string())
                                                      << "' layer '" << layerName
                                                      << "' references unknown animation '"
                                                      << animationName << "'" << std::endl;
                                        }
                                    }

                                    if (!animationResolved && stateJson.contains("animationId") &&
                                        stateJson["animationId"].is_number_unsigned()) {
                                        uint32_t jsonIndex = stateJson["animationId"].get<uint32_t>();
                                        if (jsonIndex < animationLocalIdsInOrder.size()) {
                                            resolvedAnimationIndex = jsonIndex;
                                            animationResolved = true;
                                        } else {
                                            std::cerr << "  ⚠️  StateMachine '"
                                                      << smJson.value("name", std::string())
                                                      << "' layer '" << layerName
                                                      << "' references animation index " << jsonIndex
                                                      << " out of range" << std::endl;
                                        }
                                    }

                                    if (animationResolved) {
                                        builder.set(*animStateCore,
                                                    rive::AnimationStateBase::animationIdPropertyKey,
                                                    resolvedAnimationIndex);
                                    }
                                }
                                else if (stateType == "entry" || stateType == "exit" || stateType == "any") {
                                    if (!explicitName.empty()) {
                                        const std::string canonicalKey =
                                            stateType == "entry" ? "Entry"
                                                                  : (stateType == "exit" ? "Exit" : "Any");
                                        renameState(canonicalKey, explicitName);
                                    }
                                }
                                else {
                                    std::cerr << "  ⚠️  Unsupported state type '" << stateType
                                              << "' in state machine '" << smJson.value("name", std::string())
                                              << "' layer '" << layerName << "'" << std::endl;
                                }
                            }
                        }
                    };

                    // Minimal layer so runtime considers the state machine valid
                    if (smJson.contains("layers") && smJson["layers"].is_array() &&
                        !smJson["layers"].empty())
                    {
                        for (const auto& layerJson : smJson["layers"]) {
                            auto& layerObj = builder.addCore(new rive::StateMachineLayer());
                            if (layerJson.contains("name")) {
                                builder.set(layerObj,
                                            rive::StateMachineComponentBase::namePropertyKey,
                                            layerJson["name"].get<std::string>());
                            }
                            uint32_t layerLocalId = nextSyntheticLocalId++;
                            pendingObjects.push_back({&layerObj, rive::StateMachineLayer::typeKey, layerLocalId, smLocalId});
                            localIdToBuilderObjectId[layerLocalId] = layerObj.id;
                            localIdToType[layerLocalId] = rive::StateMachineLayer::typeKey;

                            const std::string layerName =
                                layerJson.value("name", std::string());
                            const nlohmann::json* statesArray =
                                (layerJson.contains("states") && layerJson["states"].is_array())
                                    ? &layerJson["states"]
                                    : nullptr;
                            emitLayerStates(layerLocalId, layerName, statesArray);
                        }
                    }
                    else
                    {
                        auto& layerObj = builder.addCore(new rive::StateMachineLayer());
                        builder.set(layerObj,
                                    rive::StateMachineComponentBase::namePropertyKey,
                                    std::string("Layer"));
                        uint32_t layerLocalId = nextSyntheticLocalId++;
                        pendingObjects.push_back({&layerObj, rive::StateMachineLayer::typeKey, layerLocalId, smLocalId});
                        localIdToBuilderObjectId[layerLocalId] = layerObj.id;
                        localIdToType[layerLocalId] = rive::StateMachineLayer::typeKey;

                        emitLayerStates(layerLocalId, std::string("Layer"), nullptr);
                    }

                    stateMachineBindings.push_back(std::move(bindingInfo));
                }
            }
        }

        auto writeVarUint = [](std::vector<uint8_t>& out, uint32_t value) {
            while (true) {
                uint8_t byte = static_cast<uint8_t>(value & 0x7Fu);
                value >>= 7;
                if (value != 0) {
                    byte |= 0x80u;
                }
                out.push_back(byte);
                if (value == 0) {
                    break;
                }
            }
        };

        auto makePathBytes = [&](const std::vector<uint32_t>& ids) {
            std::vector<uint8_t> bytes;
            for (uint32_t id : ids) {
                writeVarUint(bytes, id);
            }
            return bytes;
        };

        if (!dataBindContexts.empty()) {
            for (auto& ctxInfo : dataBindContexts) {
                if (ctxInfo.originalPath.empty()) {
                    continue;
                }
                uint16_t smIndex = ctxInfo.originalPath[0];
                if (smIndex >= stateMachineBindings.size()) {
                    continue;
                }
                const auto& smBinding = stateMachineBindings[smIndex];
                if (smBinding.stateMachine == nullptr) {
                    continue;
                }
                std::vector<uint32_t> resolved;
                resolved.push_back(smBinding.stateMachine->id);

                if (ctxInfo.originalPath.size() > 1) {
                    uint16_t inputIndex = ctxInfo.originalPath[1];
                    CoreObject* inputCore = nullptr;

                    if (inputIndex > 0) {
                        size_t legacyIndex = static_cast<size_t>(inputIndex - 1);
                        if (legacyIndex < smBinding.inputList.size()) {
                            inputCore = smBinding.inputList[legacyIndex];
                        }
                    }

                    if (inputCore == nullptr && inputIndex < smBinding.inputList.size()) {
                        inputCore = smBinding.inputList[inputIndex];
                    }

                    if (inputCore != nullptr) {
                        resolved.push_back(inputCore->id);
                    } else {
                        continue;
                    }
                }

                for (size_t i = 2; i < ctxInfo.originalPath.size(); ++i) {
                    resolved.push_back(ctxInfo.originalPath[i]);
                }

                auto& ctx = ctxInfo.context;
                auto pathBytes = makePathBytes(resolved);
                for (auto it = ctx->properties.begin(); it != ctx->properties.end(); ++it) {
                    if (it->key == 588) {
                        ctx->properties.erase(it);
                        break;
                    }
                }
                builder.set(*ctx, 588, pathBytes);
            }
        }

        // PR2: Print diagnostic summary for keyed data
        std::cout << "\n  === PR2 KEYED DATA DIAGNOSTICS ===" << std::endl;
        std::cout << "  OMIT_KEYED flag: " << (OMIT_KEYED ? "ENABLED (keyed data skipped)" : "DISABLED (keyed data included)") << std::endl;
        std::cout << "  LinearAnimation count: " << linearAnimCount << std::endl;
        std::cout << "  StateMachine count: " << stateMachineCount << std::endl;
        
        if (!keyedInJson.empty()) {
            std::cout << "\n  Keyed types in JSON:" << std::endl;
            int totalKeyedInJson = 0;
            for (const auto& [tk, count] : keyedInJson) {
                std::cout << "    typeKey " << tk << ": " << count << std::endl;
                totalKeyedInJson += count;
            }
            std::cout << "  Total keyed in JSON: " << totalKeyedInJson << std::endl;
        }
        
        if (!keyedCreated.empty()) {
            std::cout << "\n  Keyed types created:" << std::endl;
            int totalKeyedCreated = 0;
            for (const auto& [tk, count] : keyedCreated) {
                std::cout << "    typeKey " << tk << ": " << count << std::endl;
                totalKeyedCreated += count;
            }
            std::cout << "  Total keyed created: " << totalKeyedCreated << std::endl;
        } else if (OMIT_KEYED && !keyedInJson.empty()) {
            std::cout << "  Keyed types created: 0 (all skipped by OMIT_KEYED)" << std::endl;
        }
        
        if (linearAnimCount > 0 && !keyedInJson.empty()) {
            int totalKeyed = 0;
            for (const auto& [tk, count] : keyedInJson) {
                totalKeyed += count;
            }
            double avgPerAnim = static_cast<double>(totalKeyed) / linearAnimCount;
            std::cout << "  Avg keyed objects per animation: " << avgPerAnim << std::endl;
        }
        std::cout << "  =================================\n" << std::endl;
        
        // PASS 1.5: Auto-fix orphan Fill/Stroke (PR-ORPHAN-FIX)
        std::cout << "  PASS 1.5: Fixing orphan paints..." << std::endl;
        
        int orphanFixed = 0;
        std::vector<PendingObject> newShapes;
        std::unordered_map<uint32_t, uint32_t> orphanShapeMap; // original parent localId -> synthetic Shape localId
        
        for (auto& pending : pendingObjects) {
            // Check if this is a TOP-LEVEL Paint (Fill/Stroke only) with a valid parent
            // DO NOT process gradient components (LinearGradient, GradientStop, etc.)
            // which are valid children of Fill/Stroke
            if (isTopLevelPaint(pending.typeKey) && 
                pending.parentLocalId != invalidParent)
            {
                uint16_t parentType = parentTypeFor(pending.parentLocalId);
                
                // PR-KEYED-ORDER: If parent is NOT a Shape (typeKey 3), inject synthetic Shape
                // This includes Artboard (1), Node (2), and any other non-Shape parent
                if (parentType != 0 && parentType != 3) {
                    uint32_t originalParent = pending.parentLocalId;
                    uint32_t shapeLocalId;
                    
                    // PR-ORPHAN-FIX: Check if this parent already got a synthetic Shape in PASS 1
                    auto existingShape = orphanShapeMap.find(originalParent);
                    if (existingShape != orphanShapeMap.end()) {
                        // Reuse existing synthetic Shape (e.g., created for parametric path)
                        shapeLocalId = existingShape->second;
                        pending.parentLocalId = shapeLocalId;
                        orphanFixed++;
                        
                        std::cerr << "  ⚠️  AUTO-FIX: Orphan paint (typeKey " << pending.typeKey 
                                  << " localId=" << (pending.localId.has_value() ? *pending.localId : 0)
                                  << ") → REUSING synthetic Shape " << shapeLocalId 
                                  << " (original parent typeKey=" << parentType << ")" << std::endl;
                    } else {
                        // Create NEW synthetic Shape
                        shapeLocalId = nextSyntheticLocalId++;
                        auto& shapeObj = builder.addCore(new rive::Shape());
                        
                        // CRITICAL: Set drawable properties so Rive Play renders the shape!
                        builder.set(shapeObj, 23, static_cast<uint32_t>(3));   // blendModeValue = SrcOver
                        builder.set(shapeObj, 129, static_cast<uint32_t>(4));  // drawableFlags = visible (4)
                        
                        localIdToBuilderObjectId[shapeLocalId] = shapeObj.id;
                        localIdToType[shapeLocalId] = 3;
                        orphanShapeMap[originalParent] = shapeLocalId;
                        
                        newShapes.push_back({&shapeObj, 3, shapeLocalId, originalParent});
                        
                        // Reparent the orphan paint to the synthetic Shape
                        pending.parentLocalId = shapeLocalId;
                        orphanFixed++;
                        
                        std::cerr << "  ⚠️  AUTO-FIX: Orphan paint (typeKey " << pending.typeKey 
                                  << " localId=" << (pending.localId.has_value() ? *pending.localId : 0)
                                  << ") → NEW synthetic Shape " << shapeLocalId 
                                  << " (original parent typeKey=" << parentType << ")" << std::endl;
                    }
                }
            }
        }
        
        // Add synthetic Shapes to pendingObjects
        for (auto& newShape : newShapes) {
            pendingObjects.push_back(newShape);
        }
        
        std::cout << "  ✅ Fixed " << orphanFixed << " orphan paints" << std::endl;
        
        // PASS 2: Set all parent relationships (now with complete type mapping and synthetic shapes)
        std::cout << "  PASS 2: Setting parent relationships for " << pendingObjects.size() << " objects..." << std::endl;
        int successCount = 0;
        int missingParentCount = 0;
        
        for (const auto& pending : pendingObjects)
        {
            if (pending.parentLocalId == invalidParent)
            {
                continue;
            }
            
            // P0 FIX: Skip setParent for animation graph types (managed by animation system, not Component hierarchy)
            // KeyedObject/KeyedProperty/KeyFrame have parentId=0 for topological sort but shouldn't be reparented
            if (isAnimGraphType(pending.typeKey))
            {
                continue;
            }

            auto it = localIdToBuilderObjectId.find(pending.parentLocalId);
            if (it != localIdToBuilderObjectId.end())
            {
                builder.setParent(*pending.core, it->second);
                successCount++;
            }
            else
            {
                std::cerr << "  ⚠️  WARNING: Object has missing parent localId="
                          << pending.parentLocalId << std::endl;
                missingParentCount++;
            }
        }
        std::cout << "  ✅ Set " << successCount << " parent relationships" << std::endl;
        if (missingParentCount > 0) {
            std::cerr << "  ⚠️  " << missingParentCount << " objects have missing parents (check cascade skip logic)" << std::endl;
        }
        
        // PR2/PR3: Debug summary
        std::cout << "\n  === PR2 Hierarchy Debug Summary ===" << std::endl;
        std::cout << "  Shapes inserted:         " << shapeInserted << std::endl;
        std::cout << "  Paints moved:            " << paintsMoved << std::endl;
        std::cout << "  Vertices kept:           " << verticesKept << std::endl;
        std::cout << "  Vertex remap attempted:  " << vertexRemapAttempted << " (should be 0)" << std::endl;
        std::cout << "  AnimNode remap attempted: " << animNodeRemapAttempted << " (should be 0)" << std::endl;
        if (vertexRemapAttempted > 0 || animNodeRemapAttempted > 0) {
            std::cerr << "  ⚠️  WARNING: Blacklist violation detected!" << std::endl;
        }
        std::cout << "  ===================================\n" << std::endl;
        
        // PR3: Animation graph summary
        std::cout << "  === PR3 Animation Graph Summary ===" << std::endl;
        std::cout << "  KeyedObjects:            " << keyedObjectCount << std::endl;
        std::cout << "  KeyedProperties:         " << keyedPropertyCount << std::endl;
        std::cout << "  KeyFrames:               " << keyFrameCount << std::endl;
        std::cout << "  Interpolators:           " << interpolatorCount << std::endl;
        std::cout << "  objectId remap success:  " << objectIdRemapSuccess << std::endl;
        std::cout << "  objectId remap fail:     " << objectIdRemapFail << " (should be 0)" << std::endl;
        std::cout << "  ===================================\n" << std::endl;
        
        // PASS 3: Remap deferred targetId references (after all objects created)
        int targetIdRemapSuccess = 0;
        int targetIdRemapFail = 0;
        for (const auto& deferred : deferredTargetIds) {
            auto it = localIdToBuilderObjectId.find(deferred.jsonTargetLocalId);
            if (it != localIdToBuilderObjectId.end()) {
                builder.set(*deferred.obj, 173, it->second);
                targetIdRemapSuccess++;
            } else {
                std::cerr << "  ⚠️  targetId " << deferred.jsonTargetLocalId 
                          << " not found in object map" << std::endl;
                targetIdRemapFail++;
            }
        }
        if (!deferredTargetIds.empty()) {
            std::cout << "  === Constraint targetId Remapping ===" << std::endl;
            std::cout << "  targetId remap success:  " << targetIdRemapSuccess << std::endl;
            std::cout << "  targetId remap fail:     " << targetIdRemapFail << " (should be 0)" << std::endl;
            std::cout << "  ===================================\n" << std::endl;
        }
        
        // PR-DRAWTARGET: PASS 3 - Remap DrawTarget/DrawRules component references
        int drawTargetRemapSuccess = 0;
        int drawTargetRemapFail = 0;
        int drawRulesRemapSuccess = 0;
        int drawRulesRemapFail = 0;
        int interpolatorIdRemapSuccess = 0;
        int interpolatorIdRemapFail = 0;
        
        for (const auto& deferred : deferredComponentRefs) {
            auto it = localIdToBuilderObjectId.find(deferred.jsonComponentLocalId);
            if (it != localIdToBuilderObjectId.end()) {
                builder.set(*deferred.obj, deferred.propertyKey, it->second);
                
                if (deferred.propertyKey == 51) { // KeyedObject.objectId
                    objectIdRemapSuccess++;
                }
                else if (deferred.propertyKey == 119) { // drawableId
                    drawTargetRemapSuccess++;
                } else if (deferred.propertyKey == 121) { // drawTargetId
                    drawRulesRemapSuccess++;
                } else if (deferred.propertyKey == 69) { // interpolatorId
                    interpolatorIdRemapSuccess++;
                }
            } else {
                if (deferred.propertyKey == 51) {
                    objectIdRemapFail++;
                    std::cerr << "  ⚠️  KeyedObject.objectId remap FAILED: "
                              << deferred.jsonComponentLocalId << " not found" << std::endl;
                }
                else if (deferred.propertyKey == 119) {
                    drawTargetRemapFail++;
                    std::cerr << "  ⚠️  DrawTarget.drawableId remap FAILED: "
                              << deferred.jsonComponentLocalId << " not found" << std::endl;
                } else if (deferred.propertyKey == 121) {
                    drawRulesRemapFail++;
                    std::cerr << "  ⚠️  DrawRules.drawTargetId remap FAILED: "
                              << deferred.jsonComponentLocalId << " not found" << std::endl;
                } else if (deferred.propertyKey == 69) {
                    interpolatorIdRemapFail++;
                    std::cerr << "  ⚠️  KeyFrame.interpolatorId remap FAILED: "
                              << deferred.jsonComponentLocalId << " not found" << std::endl;
                }
            }
        }
        
        if (drawTargetRemapSuccess > 0 || drawTargetRemapFail > 0 || 
            drawRulesRemapSuccess > 0 || drawRulesRemapFail > 0) {
            std::cout << "  === DrawTarget/DrawRules Remapping ===" << std::endl;
            std::cout << "  DrawTarget.drawableId success: " << drawTargetRemapSuccess << std::endl;
            std::cout << "  DrawTarget.drawableId fail:    " << drawTargetRemapFail << " (should be 0)" << std::endl;
            std::cout << "  DrawRules.drawTargetId success: " << drawRulesRemapSuccess << std::endl;
            std::cout << "  DrawRules.drawTargetId fail:    " << drawRulesRemapFail << " (should be 0)" << std::endl;
            std::cout << "  ======================================\n" << std::endl;
        }
        
        if (interpolatorIdRemapSuccess > 0 || interpolatorIdRemapFail > 0) {
            std::cout << "  === KeyFrame interpolatorId Remapping ===" << std::endl;
            std::cout << "  interpolatorId remap success: " << interpolatorIdRemapSuccess << std::endl;
            std::cout << "  interpolatorId remap fail:    " << interpolatorIdRemapFail << " (should be 0)" << std::endl;
            std::cout << "  =========================================\n" << std::endl;
        }
        
        // PR2c: Cycle detection on component parent graph
        // Build graph: node -> parent
        std::unordered_map<uint32_t, uint32_t> childToParent;
        for (const auto& pending : pendingObjects)
        {
            if (pending.parentLocalId != invalidParent && pending.localId.has_value())
            {
                childToParent[*pending.localId] = pending.parentLocalId;
            }
        }
        
        auto detectCycleFrom = [&](uint32_t start) {
            std::unordered_set<uint32_t> visiting;
            std::unordered_set<uint32_t> visited;
            uint32_t cur = start;
            std::vector<uint32_t> stack;
            while (true)
            {
                if (visiting.count(cur))
                {
                    // Cycle found, print stack
                    std::cerr << "  ❌ CYCLE detected: ";
                    bool printing = false;
                    for (auto id : stack)
                    {
                        if (id == cur) printing = true;
                        if (printing) std::cerr << id << " -> ";
                    }
                    std::cerr << cur << std::endl;
                    return true;
                }
                if (visited.count(cur)) return false;
                visiting.insert(cur);
                stack.push_back(cur);
                auto itp = childToParent.find(cur);
                if (itp == childToParent.end()) return false;
                cur = itp->second;
            }
        };
        
        bool anyCycle = false;
        for (const auto& kv : childToParent)
        {
            if (detectCycleFrom(kv.first)) { anyCycle = true; break; }
        }
        if (!anyCycle)
        {
            std::cout << "  🧭 No cycles detected in component graph" << std::endl;
        }
        
        // Animation and StateMachine building moved inline after Artboard creation (see above)
    }
    
    // Build final document
    auto doc = builder.build(typeMap);
    if (!embeddedFontData.empty())
    {
        doc.fontData = std::move(embeddedFontData);
    }
    
    // Copy typeMap to output (for serializer)
    outTypeMap = typeMap;
    
    return doc;
}

} // namespace rive_converter
