// UNIVERSAL EXTRACTOR - Extract EVERYTHING from any RIV file
// Generic approach: Export all objects with typeKey, properties, hierarchy
// No type-specific code - truly universal!
// PR-Extractor-Fix: Now with topological ordering, required defaults, and parent sanity checks

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include "extractor_postprocess.hpp"
#include "rive/file.hpp"
#include "rive/artboard.hpp"
#include "rive/component.hpp"
#include "rive/node.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/animation/keyframe.hpp"
#include "rive/animation/keyframe_double.hpp"
#include "rive/animation/keyframe_color.hpp"
#include "rive/animation/keyframe_id.hpp"
#include "rive/animation/interpolating_keyframe.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/animation/cubic_ease_interpolator.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/layer_state.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/any_state.hpp"
#include "rive/animation/entry_state.hpp"
#include "rive/animation/exit_state.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/animation/state_machine_bool.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/animation/cubic_interpolator.hpp"
#include "rive/transform_component.hpp"
#include "rive/world_transform_component.hpp"
#include "rive/shapes/ellipse.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/shape.hpp"
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
#include "rive/bones/bone.hpp"
#include "rive/bones/root_bone.hpp"
#include "rive/constraints/follow_path_constraint.hpp"
#include "rive/draw_target.hpp"
#include "rive/draw_rules.hpp"
#include "utils/no_op_factory.hpp"

using json = nlohmann::json;
using namespace rive;

struct ObjectInfo {
    uint16_t typeKey;
    uint32_t localId;  // Artboard-local component ID
    uint32_t parentId; // Artboard-local parent ID
    bool isComponent;
    json properties; // Generic property storage
};

// Helper to get type name (for debugging)
const char* getTypeName(uint16_t typeKey) {
    switch(typeKey) {
        case 1: return "Artboard";
        case 2: return "Node";
        case 3: return "Shape";
        case 4: return "Ellipse";
        case 5: return "StraightVertex";
        case 6: return "CubicDetachedVertex";
        case 7: return "Rectangle";
        case 16: return "PointsPath";
        case 17: return "RadialGradient";
        case 18: return "SolidColor";
        case 19: return "GradientStop";
        case 20: return "Fill";
        case 22: return "LinearGradient";
        case 24: return "Stroke";
        case 31: return "LinearAnimation";
        case 28: return "CubicEaseInterpolator";
        case 35: return "CubicMirroredVertex";
        case 40: return "Bone";
        case 41: return "RootBone";
        case 42: return "Bone";
        case 47: return "TrimPath";
        case 53: return "StateMachine";
        case 57: return "StateMachineLayer";
        case 61: return "AnimationState";
        case 63: return "EntryState";
        case 64: return "ExitState";
        case 87: return "TranslationConstraint";
        case 128: return "Event";
        case 134: return "Text";
        case 135: return "TextValueRun";
        case 137: return "TextStylePaint";
        case 138: return "CubicValueInterpolator";
        case 141: return "FontAsset";
        case 165: return "FollowPathConstraint";
        case 420: return "LayoutComponentStyle";
        case 533: return "Feather";
        case 48: return "DrawTarget";
        case 49: return "DrawRules";
        default: return "Unknown";
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.riv> <output.json>" << std::endl;
        return 1;
    }

    std::cout << "UNIVERSAL EXTRACTOR - Extract EVERYTHING" << std::endl;
    std::cout << std::string(79, '=') << std::endl;
    
    std::ifstream inFile(argv[1], std::ios::binary | std::ios::ate);
    if (!inFile) {
        std::cerr << "Failed to open: " << argv[1] << std::endl;
        return 1;
    }
    
    size_t fileSize = inFile.tellg();
    inFile.seekg(0);
    std::vector<uint8_t> buffer(fileSize);
    inFile.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    
    NoOpFactory factory;
    ImportResult importResult;
    auto file = File::import(buffer, &factory, &importResult);
    
    if (!file) {
        std::cerr << "Failed to import RIV file" << std::endl;
        return 1;
    }

    json output;
    output["artboards"] = json::array();
    
    // Process ALL artboards
    for (size_t artboardIdx = 0; artboardIdx < file->artboardCount(); ++artboardIdx) {
        auto* artboard = file->artboard(artboardIdx);
        if (!artboard) continue;
        
        std::cout << "\nArtboard #" << artboardIdx << ": " << artboard->name() << std::endl;
        std::cout << "  Size: " << artboard->width() << "x" << artboard->height() << std::endl;
        std::cout << "  Objects: " << artboard->objects().size() << std::endl;
        
        json artboardJson;
        artboardJson["name"] = artboard->name();
        artboardJson["width"] = artboard->width();
        artboardJson["height"] = artboard->height();
        artboardJson["clip"] = artboard->clip();  // CRITICAL: Export clip property (fixes grey screen!)
        artboardJson["objects"] = json::array();
        
        // Extract ALL objects with typeKey
        std::map<uint16_t, int> typeCounts;
        std::map<Component*, uint32_t> compToLocalId; // Component → localId mapping
        std::map<uint32_t, uint32_t> coreIdToLocalId; // Runtime Core ID → localId (for KeyedObject.objectId)
        uint32_t nextLocalId = 0;
        
        // First pass: Assign localIds and build Core ID mapping
        for (auto* obj : artboard->objects()) {
            if (auto* comp = dynamic_cast<Component*>(obj)) {
                uint32_t runtimeCoreId = artboard->idOf(obj);
                if (obj->is<Artboard>()) {
                    compToLocalId[comp] = 0;
                    coreIdToLocalId[runtimeCoreId] = 0;
                    nextLocalId = 1;
                } else {
                    compToLocalId[comp] = nextLocalId;
                    coreIdToLocalId[runtimeCoreId] = nextLocalId;
                    nextLocalId++;
                }
            }
        }
        
        // Second pass: Extract with correct parentId mapping
        for (auto* obj : artboard->objects()) {
            json objJson;
            uint16_t tk = obj->coreType();
            
            objJson["typeKey"] = tk;
            objJson["typeName"] = getTypeName(tk);
            
            // Mark unsupported types as stubs (keep for correct ID mapping)
            if (tk == 420 || tk == 123) {
                objJson["__unsupported__"] = true;
                std::cerr << "⚠️  Marking unsupported typeKey as stub: " << tk << " (" << getTypeName(tk) << ")" << std::endl;
            }
            
            typeCounts[tk]++;
            
            // If it's a Component, record ID and parentId
            if (auto* comp = dynamic_cast<Component*>(obj)) {
                objJson["localId"] = compToLocalId[comp];
                
                // For non-artboards, find parent and map its ID
                if (!obj->is<Artboard>() && comp->parent()) {
                    auto* parentComp = comp->parent();
                    auto it = compToLocalId.find(parentComp);
                    if (it != compToLocalId.end()) {
                        objJson["parentId"] = it->second;
                    } else {
                        objJson["parentId"] = 0; // Fallback to artboard
                    }
                }
            }
            
            // Extract ALL properties based on type
            objJson["properties"] = json::object();
            
            // Transform properties (Node, TransformComponent, WorldTransformComponent)
            if (auto* node = dynamic_cast<Node*>(obj)) {
                objJson["properties"]["x"] = node->x();
                objJson["properties"]["y"] = node->y();
            }
            if (auto* tc = dynamic_cast<TransformComponent*>(obj)) {
                objJson["properties"]["rotation"] = tc->rotation();
                objJson["properties"]["scaleX"] = tc->scaleX();
                objJson["properties"]["scaleY"] = tc->scaleY();
            }
            if (auto* wtc = dynamic_cast<WorldTransformComponent*>(obj)) {
                objJson["properties"]["opacity"] = wtc->opacity();
            }
            
            // Parametric shapes
            if (auto* ellipse = dynamic_cast<Ellipse*>(obj)) {
                objJson["properties"]["width"] = ellipse->width();
                objJson["properties"]["height"] = ellipse->height();
            }
            if (auto* rect = dynamic_cast<Rectangle*>(obj)) {
                objJson["properties"]["width"] = rect->width();
                objJson["properties"]["height"] = rect->height();
                objJson["properties"]["linkCornerRadius"] = rect->linkCornerRadius();
            }
            
            // Path
            if (auto* path = dynamic_cast<PointsPath*>(obj)) {
                objJson["properties"]["isClosed"] = path->isClosed();
                objJson["properties"]["pathFlags"] = path->pathFlags();
            }
            
            // Vertices
            if (auto* sv = dynamic_cast<StraightVertex*>(obj)) {
                objJson["properties"]["x"] = sv->x();
                objJson["properties"]["y"] = sv->y();
                objJson["properties"]["radius"] = sv->radius();
            }
            if (auto* cv = dynamic_cast<CubicDetachedVertex*>(obj)) {
                objJson["properties"]["x"] = cv->x();
                objJson["properties"]["y"] = cv->y();
                objJson["properties"]["inRotation"] = cv->inRotation();
                objJson["properties"]["inDistance"] = cv->inDistance();
                objJson["properties"]["outRotation"] = cv->outRotation();
                objJson["properties"]["outDistance"] = cv->outDistance();
            }
            if (auto* cmv = dynamic_cast<CubicMirroredVertex*>(obj)) {
                objJson["properties"]["x"] = cmv->x();
                objJson["properties"]["y"] = cmv->y();
                objJson["properties"]["rotation"] = cmv->rotation();
                objJson["properties"]["distance"] = cmv->distance();
            }
            
            // Paint
            if (auto* solid = dynamic_cast<SolidColor*>(obj)) {
                uint32_t color = solid->colorValue();
                char colorStr[10];
                snprintf(colorStr, sizeof(colorStr), "#%06X", color & 0xFFFFFF);
                objJson["properties"]["color"] = std::string(colorStr);
            }
            if (auto* fill = dynamic_cast<Fill*>(obj)) {
                objJson["properties"]["isVisible"] = fill->isVisible();
            }
            if (auto* stroke = dynamic_cast<Stroke*>(obj)) {
                objJson["properties"]["thickness"] = stroke->thickness();
                objJson["properties"]["cap"] = stroke->cap();
                objJson["properties"]["join"] = stroke->join();
            }
            if (auto* stop = dynamic_cast<GradientStop*>(obj)) {
                objJson["properties"]["position"] = stop->position();
                uint32_t color = stop->colorValue();
                char colorStr[10];
                snprintf(colorStr, sizeof(colorStr), "#%06X", color & 0xFFFFFF);
                objJson["properties"]["color"] = std::string(colorStr);
            }
            if (auto* feather = dynamic_cast<Feather*>(obj)) {
                objJson["properties"]["strength"] = feather->strength();
                objJson["properties"]["offsetX"] = feather->offsetX();
                objJson["properties"]["offsetY"] = feather->offsetY();
                objJson["properties"]["inner"] = feather->inner();
            }
            
            // Bones
            if (auto* bone = dynamic_cast<Bone*>(obj)) {
                objJson["properties"]["length"] = bone->length();
            }
            if (auto* rootBone = dynamic_cast<RootBone*>(obj)) {
                objJson["properties"]["x"] = rootBone->x();
                objJson["properties"]["y"] = rootBone->y();
            }
            
            // DrawTarget (typeKey 48) - PR-DRAWTARGET
            if (auto* drawTarget = dynamic_cast<DrawTarget*>(obj)) {
                uint32_t runtimeDrawableId = drawTarget->drawableId();
                uint32_t localDrawableId = runtimeDrawableId;  // Default: keep runtime ID
                
                // Remap runtime Core ID back to localId (same pattern as FollowPathConstraint.targetId)
                auto it = coreIdToLocalId.find(runtimeDrawableId);
                if (it != coreIdToLocalId.end()) {
                    localDrawableId = it->second;
                    objJson["properties"]["drawableId"] = localDrawableId;  // Property 119 (localId)
                } else {
                    objJson["properties"]["drawableId"] = runtimeDrawableId;  // Fallback: keep runtime ID
                    std::cerr << "  ⚠️  DrawTarget drawableId not found in coreIdToLocalId: " << runtimeDrawableId << std::endl;
                }
                objJson["properties"]["placementValue"] = drawTarget->placementValue(); // Property 120
                
                if (auto* comp = dynamic_cast<Component*>(obj)) {
                    std::cout << "  ✅ [DrawTarget] localId=" << compToLocalId[comp]
                              << " drawableId=" << localDrawableId
                              << " placement=" << drawTarget->placementValue() << std::endl;
                }
            }
            
            // DrawRules (typeKey 49) - PR-DRAWTARGET
            if (auto* drawRules = dynamic_cast<DrawRules*>(obj)) {
                uint32_t runtimeTargetId = drawRules->drawTargetId();
                uint32_t localTargetId = runtimeTargetId;  // Default: keep runtime ID
                
                // Remap runtime Core ID back to localId
                auto it = coreIdToLocalId.find(runtimeTargetId);
                if (it != coreIdToLocalId.end()) {
                    localTargetId = it->second;
                    objJson["properties"]["drawTargetId"] = localTargetId;  // Property 121 (localId)
                } else {
                    objJson["properties"]["drawTargetId"] = runtimeTargetId;  // Fallback: keep runtime ID
                    std::cerr << "  ⚠️  DrawRules drawTargetId not found in coreIdToLocalId: " << runtimeTargetId << std::endl;
                }
                
                if (auto* comp = dynamic_cast<Component*>(obj)) {
                    std::cout << "  ✅ [DrawRules] localId=" << compToLocalId[comp]
                              << " drawTargetId=" << localTargetId << std::endl;
                }
            }
            
            // Constraints - FollowPathConstraint
            if (auto* followPath = dynamic_cast<FollowPathConstraint*>(obj)) {
                // FollowPathConstraint specific properties
                objJson["properties"]["distance"] = followPath->distance();
                objJson["properties"]["orient"] = followPath->orient();
                objJson["properties"]["offset"] = followPath->offset();
                
                // TransformSpaceConstraint base properties
                objJson["properties"]["sourceSpaceValue"] = followPath->sourceSpaceValue();
                objJson["properties"]["destSpaceValue"] = followPath->destSpaceValue();
                
                // TargetedConstraint base property (CRITICAL!)
                uint32_t runtimeTargetId = followPath->targetId();
                // Remap runtime Core ID back to localId
                auto it = coreIdToLocalId.find(runtimeTargetId);
                if (it != coreIdToLocalId.end()) {
                    objJson["properties"]["targetId"] = it->second;
                } else if (runtimeTargetId != static_cast<uint32_t>(-1)) {
                    // Only warn if not missingId (-1)
                    std::cerr << "  ⚠️  FollowPathConstraint targetId " << runtimeTargetId 
                              << " not found in localId map" << std::endl;
                }
            }
            
            // Animation Interpolators (CubicEase, CubicValue both inherit from CubicInterpolator)
            if (auto* cubicInterp = dynamic_cast<CubicInterpolator*>(obj)) {
                objJson["properties"]["x1"] = cubicInterp->x1();
                objJson["properties"]["y1"] = cubicInterp->y1();
                objJson["properties"]["x2"] = cubicInterp->x2();
                objJson["properties"]["y2"] = cubicInterp->y2();
            }
            
            artboardJson["objects"].push_back(objJson);
        }
        
        // Add LinearAnimation objects and their children (KeyedObject, KeyFrames) to objects[]
        // These are NOT in artboard->objects(), they're in separate m_Animations collection
        for (size_t i = 0; i < artboard->animationCount(); ++i) {
            auto* anim = artboard->animation(i);
            if (auto* la = dynamic_cast<LinearAnimation*>(anim)) {
                json animJson;
                animJson["typeKey"] = la->coreType();
                animJson["typeName"] = getTypeName(la->coreType());
                animJson["properties"] = json::object();
                animJson["properties"]["name"] = la->name();
                animJson["properties"]["fps"] = la->fps();
                animJson["properties"]["duration"] = la->duration();
                animJson["properties"]["loop"] = la->loop();
                
                artboardJson["objects"].push_back(animJson);
                
                // Export KeyedObject, KeyedProperty, and KeyFrame children
                for (size_t k = 0; k < la->numKeyedObjects(); ++k) {
                    auto* ko = la->getObject(k);
                    if (!ko) continue;
                    
                    json koJson;
                    koJson["typeKey"] = ko->coreType();
                    koJson["typeName"] = getTypeName(ko->coreType());
                    koJson["properties"] = json::object();
                    
                    // Remap runtime Core ID to localId for rebuild
                    uint32_t runtimeCoreId = ko->objectId();
                    auto idIt = coreIdToLocalId.find(runtimeCoreId);
                    if (idIt != coreIdToLocalId.end()) {
                        koJson["properties"]["objectId"] = idIt->second; // Use localId
                    } else {
                        std::cerr << "⚠️  WARNING: KeyedObject.objectId " << runtimeCoreId << " not in coreIdToLocalId (size=" << coreIdToLocalId.size() << ")" << std::endl;
                        koJson["properties"]["objectId"] = runtimeCoreId; // Fallback
                    }
                    
                    artboardJson["objects"].push_back(koJson);
                    
                    // Export KeyedProperty children
                    for (size_t p = 0; p < ko->numKeyedProperties(); ++p) {
                        auto* kp = ko->getProperty(p);
                        if (!kp) continue;
                        
                        json kpJson;
                        kpJson["typeKey"] = kp->coreType();
                        kpJson["typeName"] = getTypeName(kp->coreType());
                        kpJson["properties"] = json::object();
                        kpJson["properties"]["propertyKey"] = kp->propertyKey();
                        artboardJson["objects"].push_back(kpJson);
                        
                        // Export KeyFrame children
                        for (size_t f = 0; f < kp->numKeyFrames(); ++f) {
                            auto* kf = kp->getKeyFrame(f);
                            if (!kf) continue;
                            
                            json kfJson;
                            kfJson["typeKey"] = kf->coreType();
                            kfJson["typeName"] = getTypeName(kf->coreType());
                            kfJson["properties"] = json::object();
                            kfJson["properties"]["frame"] = kf->frame();
                            kfJson["properties"]["seconds"] = kf->seconds();
                            
                            // Export value based on keyframe type
                            if (auto* kfd = dynamic_cast<const KeyFrameDouble*>(kf)) {
                                kfJson["properties"]["value"] = kfd->value();
                            } else if (auto* kfc = dynamic_cast<const KeyFrameColor*>(kf)) {
                                kfJson["properties"]["value"] = kfc->value();
                            } else if (auto* kfi = dynamic_cast<const KeyFrameId*>(kf)) {
                                kfJson["properties"]["value"] = kfi->value();
                            }
                            
                            artboardJson["objects"].push_back(kfJson);
                            
                            // Export interpolator if present (follows KeyFrame in file order)
                            if (auto* ikf = dynamic_cast<const InterpolatingKeyFrame*>(kf)) {
                                if (auto* interpolator = ikf->interpolator()) {
                                    json interpJson;
                                    interpJson["typeKey"] = interpolator->coreType();
                                    interpJson["typeName"] = getTypeName(interpolator->coreType());
                                    interpJson["properties"] = json::object();
                                    
                                    // Export interpolator properties (x1, y1, x2, y2 for cubic)
                                    if (auto* cubicInterp = dynamic_cast<const CubicEaseInterpolator*>(interpolator)) {
                                        interpJson["properties"]["x1"] = cubicInterp->x1();
                                        interpJson["properties"]["y1"] = cubicInterp->y1();
                                        interpJson["properties"]["x2"] = cubicInterp->x2();
                                        interpJson["properties"]["y2"] = cubicInterp->y2();
                                    }
                                    
                                    artboardJson["objects"].push_back(interpJson);
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Add StateMachine objects and their children (Inputs, Layers, States, Transitions) to objects[]
        // These are NOT in artboard->objects(), they're in separate m_StateMachines collection
        for (size_t i = 0; i < artboard->stateMachineCount(); ++i) {
            auto* sm = artboard->stateMachine(i);
            
            json smJson;
            smJson["typeKey"] = sm->coreType();
            smJson["typeName"] = getTypeName(sm->coreType());
            smJson["properties"] = json::object();
            smJson["properties"]["name"] = sm->name();
            artboardJson["objects"].push_back(smJson);
            
            // Export Inputs
            for (size_t j = 0; j < sm->inputCount(); ++j) {
                auto* input = sm->input(j);
                json inputJson;
                inputJson["typeKey"] = input->coreType();
                inputJson["typeName"] = getTypeName(input->coreType());
                inputJson["properties"] = json::object();
                inputJson["properties"]["name"] = input->name();
                
                if (auto* boolInput = dynamic_cast<const StateMachineBool*>(input)) {
                    inputJson["properties"]["value"] = boolInput->value();
                } else if (auto* numberInput = dynamic_cast<const StateMachineNumber*>(input)) {
                    inputJson["properties"]["value"] = numberInput->value();
                }
                
                artboardJson["objects"].push_back(inputJson);
            }
            
            // Export Layers
            for (size_t j = 0; j < sm->layerCount(); ++j) {
                auto* layer = sm->layer(j);
                json layerJson;
                layerJson["typeKey"] = layer->coreType();
                layerJson["typeName"] = getTypeName(layer->coreType());
                layerJson["properties"] = json::object();
                layerJson["properties"]["name"] = layer->name();
                artboardJson["objects"].push_back(layerJson);
                
                // Export States
                for (size_t k = 0; k < layer->stateCount(); ++k) {
                    auto* state = layer->state(k);
                    json stateJson;
                    stateJson["typeKey"] = state->coreType();
                    stateJson["typeName"] = getTypeName(state->coreType());
                    stateJson["properties"] = json::object();
                    
                    if (auto* animState = dynamic_cast<const AnimationState*>(state)) {
                        stateJson["properties"]["animationId"] = animState->animationId();
                    }
                    
                    artboardJson["objects"].push_back(stateJson);
                    
                    // Export Transitions
                    for (size_t t = 0; t < state->transitionCount(); ++t) {
                        auto* trans = state->transition(t);
                        json transJson;
                        transJson["typeKey"] = trans->coreType();
                        transJson["typeName"] = getTypeName(trans->coreType());
                        transJson["properties"] = json::object();
                        transJson["properties"]["stateToId"] = trans->stateToId();
                        transJson["properties"]["flags"] = trans->flags();
                        transJson["properties"]["duration"] = trans->duration();
                        artboardJson["objects"].push_back(transJson);
                        
                        // TODO: Export transition conditions
                    }
                }
            }
        }
        
        // Keep metadata format for backward compatibility
        artboardJson["animations"] = json::array();
        for (size_t i = 0; i < artboard->animationCount(); ++i) {
            auto* anim = artboard->animation(i);
            if (auto* la = dynamic_cast<LinearAnimation*>(anim)) {
                artboardJson["animations"].push_back({
                    {"name", la->name()},
                    {"fps", la->fps()},
                    {"duration", la->duration()},
                    {"loop", la->loop()}
                });
            }
        }
        
        // Add state machines with full details (backward compat)
        artboardJson["stateMachines"] = json::array();
        for (size_t i = 0; i < artboard->stateMachineCount(); ++i) {
            auto* sm = artboard->stateMachine(i);
            json smJson;
            smJson["name"] = sm->name();
            smJson["typeKey"] = sm->coreType();
            
            // Extract inputs
            smJson["inputs"] = json::array();
            for (size_t j = 0; j < sm->inputCount(); ++j) {
                auto* input = sm->input(j);
                json inputJson;
                inputJson["name"] = input->name();
                inputJson["typeKey"] = input->coreType();
                
                // Get value based on type
                if (auto* boolInput = dynamic_cast<const StateMachineBool*>(input)) {
                    inputJson["value"] = boolInput->value();
                } else if (auto* numberInput = dynamic_cast<const StateMachineNumber*>(input)) {
                    inputJson["value"] = numberInput->value();
                } else if (dynamic_cast<const StateMachineTrigger*>(input)) {
                    inputJson["type"] = "trigger";
                }
                
                smJson["inputs"].push_back(inputJson);
            }
            
            // Extract layers
            smJson["layers"] = json::array();
            for (size_t j = 0; j < sm->layerCount(); ++j) {
                auto* layer = sm->layer(j);
                json layerJson;
                layerJson["name"] = layer->name();
                layerJson["typeKey"] = layer->coreType();
                
                // Extract states
                layerJson["states"] = json::array();
                for (size_t k = 0; k < layer->stateCount(); ++k) {
                    auto* state = layer->state(k);
                    json stateJson;
                    stateJson["typeKey"] = state->coreType();
                    
                    // If AnimationState, record animationId and get name from animation
                    if (auto* animState = dynamic_cast<const AnimationState*>(state)) {
                        uint16_t animId = animState->animationId();
                        stateJson["animationId"] = animId;
                        // Get animation name if valid index
                        if (animId < artboard->animationCount()) {
                            stateJson["name"] = artboard->animation(animId)->name();
                        } else {
                            stateJson["name"] = "Animation_" + std::to_string(animId);
                        }
                    } else {
                        stateJson["name"] = "State_" + std::to_string(k);
                    }
                    
                    // Extract transitions
                    stateJson["transitions"] = json::array();
                    for (size_t t = 0; t < state->transitionCount(); ++t) {
                        auto* trans = state->transition(t);
                        json transJson;
                        transJson["typeKey"] = trans->coreType();
                        transJson["stateToId"] = trans->stateToId();
                        transJson["flags"] = trans->flags();
                        transJson["duration"] = trans->duration();
                        // Note: TransitionConditions would need additional extraction
                        stateJson["transitions"].push_back(transJson);
                    }
                    
                    layerJson["states"].push_back(stateJson);
                }
                
                // NOTE: System states (Entry/Exit/Any) are already included in layer->state(k) loop above!
                // No need to add them separately - they're returned by layer->state()
                
                smJson["layers"].push_back(layerJson);
            }
            
            artboardJson["stateMachines"].push_back(smJson);
        }
        
        // Type summary
        artboardJson["typeSummary"] = json::object();
        for (const auto& [tk, count] : typeCounts) {
            artboardJson["typeSummary"][std::to_string(tk)] = {
                {"count", count},
                {"name", getTypeName(tk)}
            };
        }
        
        // PR-Extractor-Fix: Post-process artboard (topological sort + defaults + sanity checks)
        extractor_postprocess::DiagnosticCounters diag;
        artboardJson = extractor_postprocess::postProcessArtboard(artboardJson, diag);
        extractor_postprocess::printDiagnostics(diag);
        
        output["artboards"].push_back(artboardJson);
    }
    
    output["totalArtboards"] = file->artboardCount();
    
    // Write output
    std::ofstream outFile(argv[2]);
    outFile << output.dump(2);
    
    std::cout << "\n" << std::string(79, '=') << std::endl;
    std::cout << "✅ UNIVERSAL EXTRACTION COMPLETE!" << std::endl;
    std::cout << std::string(79, '=') << std::endl;
    std::cout << "Output: " << argv[2] << std::endl;
    std::cout << "Artboards: " << file->artboardCount() << std::endl;
    
    return 0;
}

