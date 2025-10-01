#include "core_builder.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
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
#include "rive/shapes/clipping_shape.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/animation/keyframe_double.hpp"
#include "rive/animation/keyframe_color.hpp"
#include "rive/animation/keyframe_id.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/any_state.hpp"
#include "rive/animation/entry_state.hpp"
#include "rive/animation/exit_state.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/animation/state_machine_bool.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/animation/cubic_ease_interpolator.hpp"
#include "rive/animation/cubic_value_interpolator.hpp"
#include "rive/bones/bone.hpp"
#include "rive/bones/root_bone.hpp"
#include "rive/constraints/translation_constraint.hpp"
#include "rive/constraints/follow_path_constraint.hpp"
#include "rive/shapes/paint/trim_path.hpp"
// Layout system requires WITH_RIVE_LAYOUT (yoga dependency)
// #include "rive/layout/layout_component_style.hpp"
#include "rive/generated/backboard_base.hpp"

namespace rive_converter
{

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
        // Animation keyframe types
        case 25: return new rive::KeyedObject();
        case 26: return new rive::KeyedProperty();
        case 28: return new rive::CubicEaseInterpolator(); // Bezier interpolator
        case 30: return new rive::KeyFrameDouble();
        case 31: return new rive::LinearAnimation();
        case 35: return new rive::CubicMirroredVertex();
        case 37: return new rive::KeyFrameColor();
        case 50: return new rive::KeyFrameId();
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
        // case 420: return new rive::LayoutComponentStyle(); // Requires WITH_RIVE_LAYOUT - skipped for now
        case 533: return new rive::Feather();
        // Add more as needed
        default:
            std::cerr << "Unknown typeKey: " << typeKey << std::endl;
            return nullptr;
    }
}

// Set property on object based on key name and value
// Note: x/y are handled in main loop (different keys for Node vs Vertex)
static void setProperty(CoreBuilder& builder, CoreObject& obj, const std::string& key, const nlohmann::json& value, const std::map<uint32_t, uint32_t>& idMapping) {
    // Transform properties (x/y handled in main loop!)
    if (key == "rotation") builder.set(obj, 15, value.get<float>());
    else if (key == "scaleX") builder.set(obj, 16, value.get<float>());
    else if (key == "scaleY") builder.set(obj, 17, value.get<float>());
    else if (key == "opacity") builder.set(obj, 18, value.get<float>());
    
    // Name (Component)
    else if (key == "name") builder.set(obj, 4, value.get<std::string>());
    
    // Parametric shapes (width/height handled in main loop for artboard vs shape distinction)
    else if (key == "linkCornerRadius") builder.set(obj, rive::RectangleBase::linkCornerRadiusPropertyKey, value.get<bool>());
    
    // Path
    else if (key == "isClosed") builder.set(obj, 120, value.get<bool>());
    else if (key == "pathFlags") builder.set(obj, 128, value.get<uint32_t>());
    
    // Vertices
    else if (key == "radius") builder.set(obj, 26, value.get<float>());
    else if (key == "inRotation") builder.set(obj, 84, value.get<float>());
    else if (key == "inDistance") builder.set(obj, 85, value.get<float>());
    else if (key == "outRotation") builder.set(obj, 86, value.get<float>());
    else if (key == "outDistance") builder.set(obj, 87, value.get<float>());
    // CubicMirrored
    else if (key == "distance") builder.set(obj, 80, value.get<float>());
    
    // Paint
    else if (key == "color") builder.set(obj, 37, parse_color(value.get<std::string>()));
    else if (key == "isVisible") builder.set(obj, 41, value.get<bool>());
    else if (key == "thickness") builder.set(obj, 140, value.get<float>());
    else if (key == "cap") builder.set(obj, 48, value.get<uint32_t>());
    else if (key == "join") builder.set(obj, 49, value.get<uint32_t>());
    
    // Gradient
    else if (key == "position") builder.set(obj, 39, value.get<float>());
    
    // Feather
    else if (key == "strength") builder.set(obj, 749, value.get<float>());
    else if (key == "offsetX") builder.set(obj, 750, value.get<float>());
    else if (key == "offsetY") builder.set(obj, 751, value.get<float>());
    else if (key == "inner") builder.set(obj, 752, value.get<bool>());
    
    // Bone
    else if (key == "length") builder.set(obj, 89, value.get<float>());
    
    // Cubic Interpolator (animation)
    else if (key == "x1") builder.set(obj, 63, value.get<float>());
    else if (key == "y1") builder.set(obj, 64, value.get<float>());
    else if (key == "x2") builder.set(obj, 65, value.get<float>());
    else if (key == "y2") builder.set(obj, 66, value.get<float>());
    
    // Animation keyframe properties
    else if (key == "objectId") {
        // Remap localId to builderId for KeyedObject
        uint32_t localId = value.get<uint32_t>();
        auto it = idMapping.find(localId);
        if (it != idMapping.end()) {
            builder.set(obj, 51, it->second); // Use remapped builderId
        } else {
            std::cerr << "⚠️  WARNING: objectId localId=" << localId << " not found in mapping, using raw value" << std::endl;
            builder.set(obj, 51, localId); // Fallback
        }
    }
    else if (key == "propertyKey") builder.set(obj, 53, value.get<uint32_t>()); // KeyedProperty (not an ID)
    else if (key == "frame") builder.set(obj, 67, value.get<uint32_t>()); // KeyFrame
    else if (key == "value") {
        // KeyFrame value - property key depends on KeyFrame subclass type
        uint16_t typeKey = obj.core->coreType();
        
        if (typeKey == 30) { // KeyFrameDouble
            builder.set(obj, 70, value.get<float>());
        } else if (typeKey == 37) { // KeyFrameColor
            builder.set(obj, 88, value.get<uint32_t>());
        } else if (typeKey == 50) { // KeyFrameId
            builder.set(obj, 122, value.get<uint32_t>());
        } else {
            // Fallback: try to infer from value type
            if (value.is_number_float()) {
                builder.set(obj, 70, value.get<float>());
            } else {
                builder.set(obj, 88, value.get<uint32_t>());
            }
        }
    }
    // Animation properties
    else if (key == "fps") builder.set(obj, 56, value.get<uint32_t>());
    else if (key == "duration") {
        // Could be LinearAnimation (57) or StateTransition (158)
        uint16_t typeKey = obj.core->coreType();
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
    typeMap[382] = rive::CoreBoolType::id; // linkCornerRadius
    
    // Path
    typeMap[120] = rive::CoreBoolType::id; // isClosed
    typeMap[128] = rive::CoreUintType::id; // pathFlags
    
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
    typeMap[70] = rive::CoreDoubleType::id; // KeyFrameDouble.value
    typeMap[88] = rive::CoreColorType::id; // KeyFrameColor.value (MUST be Color, not Uint!)
    typeMap[122] = rive::CoreUintType::id; // KeyFrameId.value
    
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
    typeMap[56] = rive::CoreUintType::id; // LinearAnimation fps
    typeMap[57] = rive::CoreUintType::id; // LinearAnimation duration
    typeMap[58] = rive::CoreDoubleType::id; // LinearAnimation speed
    typeMap[59] = rive::CoreUintType::id; // LinearAnimation loopValue
    
    // State Machine properties
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
    
    CoreBuilder builder;
    PropertyTypeMap typeMap;
    initUniversalTypeMap(typeMap);
    
    // Add Backboard
    auto& backboard = builder.addCore(new rive::Backboard());
    builder.set(backboard, 44, static_cast<uint32_t>(0)); // mainArtboardId
    
    // Process each artboard
    for (size_t abIdx = 0; abIdx < data["artboards"].size(); ++abIdx) {
        const auto& abJson = data["artboards"][abIdx];
        
        std::cout << "Building artboard " << abIdx << ": " << abJson["name"] << std::endl;
        std::cout << "  Objects: " << abJson["objects"].size() << std::endl;
        
        struct PendingObject
        {
            CoreObject* core = nullptr;
            uint16_t typeKey = 0;
            std::optional<uint32_t> localId;
            uint32_t parentLocalId = 0xFFFFFFFFu;
        };

        const uint32_t invalidParent = 0xFFFFFFFFu;
        auto isParametricPathType = [](uint16_t key) {
            switch (key)
            {
                case 4:  // Ellipse
                case 7:  // Rectangle
                case 16: // PointsPath
                    return true;
                default:
                    return false;
            }
        };

        // Map from JSON localId to builder object id
        std::map<uint32_t, uint32_t> localIdToBuilderObjectId;
        std::unordered_map<uint32_t, uint16_t> localIdToType; // Track type per localId
        std::vector<PendingObject> pendingObjects; // Stored in creation order
        std::set<uint32_t> skippedLocalIds; // Track stub/skipped object localIds
        std::unordered_map<uint32_t, uint32_t> parentRemap; // old parent localId -> Shape container localId

        uint32_t maxLocalId = 0;
        for (const auto& objJson : abJson["objects"]) {
            if (objJson.contains("localId")) {
                maxLocalId = std::max(maxLocalId, objJson["localId"].get<uint32_t>());
            }
        }
        uint32_t nextSyntheticLocalId = maxLocalId + 1;

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

        // PASS 1: Create objects with accurate parent type info
        std::cout << "  PASS 1: Creating objects with synthetic Shape injection (when needed)..." << std::endl;
        for (const auto& objJson : abJson["objects"]) {
            uint16_t typeKey = objJson["typeKey"];

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
                    // Only remap Fill (20) and Stroke (24) - NOT vertices (5,6,35)!
                    if (typeKey == 20 || typeKey == 24) {
                        parentLocalId = remap->second;
                    }
                    // Vertices keep original PointsPath parent
                }
            }

            std::optional<uint32_t> localId;
            if (objJson.contains("localId")) {
                localId = objJson["localId"].get<uint32_t>();
            }

            bool needsShapeContainer =
                isParametricPathType(typeKey) && parentTypeFor(parentLocalId) != 3;

            std::unordered_set<std::string> consumedKeys;
            if (needsShapeContainer) {
                uint32_t shapeLocalId = nextSyntheticLocalId++;
                auto& shapeObj = builder.addCore(new rive::Shape());
                pendingObjects.push_back({&shapeObj, 3, shapeLocalId, parentLocalId});
                localIdToBuilderObjectId[shapeLocalId] = shapeObj.id;
                localIdToType[shapeLocalId] = 3;

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

                // Retroactively remap ONLY Fill/Stroke objects (NOT vertices!)
                // that were already created and point to this parametric path
                if (localId.has_value()) {
                    uint32_t pathLocalId = *localId;
                    
                    for (auto& pending : pendingObjects) {
                        // Only remap Fill (20) and Stroke (24) - NOT vertices (5,6,35)!
                        if ((pending.typeKey == 20 || pending.typeKey == 24) && 
                            pending.parentLocalId == pathLocalId) {
                            pending.parentLocalId = shapeLocalId;
                        }
                    }
                    
                    // Future objects referencing this path:
                    // - If Fill/Stroke: remap to Shape
                    // - If Vertex: keep original path
                    parentRemap[pathLocalId] = shapeLocalId;
                }
            }

            auto& obj = builder.addCore(coreObj);

            // Set Artboard name and size from artboard-level JSON
            if (typeKey == 1 && abJson.contains("name")) { // Artboard
                builder.set(obj, 4, abJson["name"].get<std::string>()); // name
                builder.set(obj, 7, abJson["width"].get<float>()); // width
                builder.set(obj, 8, abJson["height"].get<float>()); // height
                builder.set(obj, 196, true); // clip
            }

            pendingObjects.push_back({&obj, typeKey, localId, parentLocalId});

            if (localId) {
                localIdToBuilderObjectId[*localId] = obj.id;
                localIdToType[*localId] = typeKey;
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
                    } else {
                        builder.set(obj, 20, value.get<float>());
                    }
                }
                else if (key == "height") {
                    if (typeKey == 1) {
                        builder.set(obj, 8, value.get<float>());
                    } else {
                        builder.set(obj, 21, value.get<float>());
                    }
                }
                else {
                    setProperty(builder, obj, key, value, localIdToBuilderObjectId);
                }
            }
        }

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
        
        // Animation and StateMachine building moved inline after Artboard creation (see above)
    }
    
    // Build final document
    auto doc = builder.build(typeMap);
    
    // Copy typeMap to output (for serializer)
    outTypeMap = typeMap;
    
    return doc;
}

} // namespace rive_converter
