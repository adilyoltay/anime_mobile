// HIERARCHICAL EXTRACTOR - Extract RIV with TRUE hierarchy
// 1 Shape can have MULTIPLE Paths + Fill/Stroke
// This creates EXACT object count match

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <nlohmann/json.hpp>
#include "rive/file.hpp"
#include "rive/artboard.hpp"
#include "rive/component.hpp"
#include "rive/node.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/points_path.hpp"
#include "rive/shapes/straight_vertex.hpp"
#include "rive/shapes/cubic_detached_vertex.hpp"
#include "rive/shapes/cubic_mirrored_vertex.hpp"
#include "rive/shapes/ellipse.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/paint/linear_gradient.hpp"
#include "rive/shapes/paint/radial_gradient.hpp"
#include "rive/shapes/paint/gradient_stop.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/bones/bone.hpp"
#include "rive/bones/root_bone.hpp"
#include "utils/no_op_factory.hpp"

using json = nlohmann::json;
using namespace rive;

struct HierarchyNode {
    Core* object;
    Component* component;
    uint16_t typeKey;
    uint32_t localId;
    uint32_t parentId;
    std::vector<HierarchyNode*> children;
};

std::map<Core*, HierarchyNode*> objToNode;
std::map<uint32_t, HierarchyNode*> idToNode;
std::vector<std::unique_ptr<HierarchyNode>> allNodes;

void buildHierarchy(const std::vector<Core*>& objects) {
    std::cout << "Building complete hierarchy..." << std::endl;
    
    uint32_t nextId = 0;
    
    // Create nodes
    for (auto* obj : objects) {
        auto node = std::make_unique<HierarchyNode>();
        node->object = obj;
        node->component = dynamic_cast<Component*>(obj);
        node->typeKey = obj->coreType();
        
        if (node->component) {
            if (obj->is<Artboard>()) {
                node->localId = 0;
                node->parentId = -1;
                nextId = 1;
            } else {
                node->localId = nextId++;
                node->parentId = node->component->parentId();
            }
        } else {
            node->localId = nextId++;
            node->parentId = -1;
        }
        
        objToNode[obj] = node.get();
        if (node->component) {
            idToNode[node->localId] = node.get();
        }
        allNodes.push_back(std::move(node));
    }
    
    // Build parent-child links
    for (auto& nodePtr : allNodes) {
        if (nodePtr->parentId != static_cast<uint32_t>(-1)) {
            auto it = idToNode.find(nodePtr->parentId);
            if (it != idToNode.end()) {
                it->second->children.push_back(nodePtr.get());
            }
        }
    }
    
    std::cout << "Hierarchy: " << allNodes.size() << " nodes, " 
              << idToNode.size() << " components" << std::endl;
}

json extractHierarchicalShape(HierarchyNode* shapeNode) {
    json shape;
    auto* shapeObj = shapeNode->object;
    auto* node = dynamic_cast<Node*>(shapeObj);
    
    // Get position
    if (node) {
        shape["x"] = node->x();
        shape["y"] = node->y();
    }
    
    // Determine shape type
    if (shapeNode->typeKey == 2) { // Node (empty container)
        shape["type"] = "node";
        shape["paths"] = json::array(); // No paths
    }
    else if (shapeObj->is<Rectangle>()) {
        shape["type"] = "rectangle";
        auto* rect = shapeObj->as<Rectangle>();
        shape["width"] = rect->width();
        shape["height"] = rect->height();
        shape["paths"] = json::array(); // No custom paths
    }
    else if (shapeObj->is<Ellipse>()) {
        shape["type"] = "ellipse";
        auto* ellipse = shapeObj->as<Ellipse>();
        shape["width"] = ellipse->width();
        shape["height"] = ellipse->height();
        shape["paths"] = json::array();
    }
    else {
        // Custom shape with PointsPath children
        shape["type"] = "custom";
        shape["paths"] = json::array();
        
        // Extract ALL PointsPath children
        for (auto* child : shapeNode->children) {
            if (child->typeKey == 16) { // PointsPath
                auto* path = child->object->as<PointsPath>();
                json pathData;
                pathData["isClosed"] = path->isClosed();
                pathData["vertices"] = json::array();
                
                // Extract vertices
                for (auto* vertexChild : child->children) {
                    if (vertexChild->typeKey == 5) { // StraightVertex
                        auto* v = vertexChild->object->as<StraightVertex>();
                        pathData["vertices"].push_back({
                            {"type", "straight"},
                            {"x", v->x()},
                            {"y", v->y()},
                            {"radius", v->radius()}
                        });
                    }
                    else if (vertexChild->typeKey == 6) { // CubicDetachedVertex
                        auto* v = vertexChild->object->as<CubicDetachedVertex>();
                        pathData["vertices"].push_back({
                            {"type", "cubic"},
                            {"x", v->x()},
                            {"y", v->y()},
                            {"inRotation", v->inRotation()},
                            {"inDistance", v->inDistance()},
                            {"outRotation", v->outRotation()},
                            {"outDistance", v->outDistance()}
                        });
                    }
                    else if (vertexChild->typeKey == 35) { // CubicMirroredVertex
                        auto* v = vertexChild->object->as<CubicMirroredVertex>();
                        pathData["vertices"].push_back({
                            {"type", "cubicMirrored"},
                            {"x", v->x()},
                            {"y", v->y()},
                            {"rotation", v->rotation()},
                            {"distance", v->distance()}
                        });
                    }
                }
                
                shape["paths"].push_back(pathData);
            }
        }
    }
    
    // Extract Fill
    shape["hasFill"] = false;
    for (auto* child : shapeNode->children) {
        if (child->typeKey == 20) { // Fill
            shape["hasFill"] = true;
            json fill;
            fill["hasGradient"] = false;
            
            // Check Fill's children
            for (auto* fillChild : child->children) {
                if (fillChild->typeKey == 22 || fillChild->typeKey == 17) { // Gradient
                    fill["hasGradient"] = true;
                    json gradient;
                    gradient["type"] = (fillChild->typeKey == 22) ? "linear" : "radial";
                    gradient["stops"] = json::array();
                    
                    // Get GradientStops
                    for (auto* stopChild : fillChild->children) {
                        if (stopChild->typeKey == 19) { // GradientStop
                            auto* stop = stopChild->object->as<GradientStop>();
                            uint32_t c = stop->colorValue();
                            char cs[10];
                            snprintf(cs, sizeof(cs), "#%06X", c & 0xFFFFFF);
                            
                            gradient["stops"].push_back({
                                {"position", stop->position()},
                                {"color", std::string(cs)}
                            });
                        }
                    }
                    
                    fill["gradient"] = gradient;
                }
                else if (fillChild->typeKey == 18) { // SolidColor
                    auto* solid = fillChild->object->as<SolidColor>();
                    uint32_t c = solid->colorValue();
                    char cs[10];
                    snprintf(cs, sizeof(cs), "#%06X", c & 0xFFFFFF);
                    fill["solidColor"] = std::string(cs);
                }
                else if (fillChild->typeKey == 533) { // Feather
                    auto* feather = fillChild->object->as<Feather>();
                    fill["hasFeather"] = true;
                    fill["feather"] = {
                        {"strength", feather->strength()},
                        {"offsetX", feather->offsetX()},
                        {"offsetY", feather->offsetY()},
                        {"inner", feather->inner()}
                    };
                }
            }
            
            shape["fill"] = fill;
            break; // Only one Fill per Shape
        }
    }
    
    // Extract Stroke
    shape["hasStroke"] = false;
    for (auto* child : shapeNode->children) {
        if (child->typeKey == 24) { // Stroke
            auto* stroke = child->object->as<Stroke>();
            shape["hasStroke"] = true;
            json strokeData;
            strokeData["thickness"] = stroke->thickness();
            
            // Get SolidColor
            for (auto* strokeChild : child->children) {
                if (strokeChild->typeKey == 18) { // SolidColor
                    auto* solid = strokeChild->object->as<SolidColor>();
                    uint32_t c = solid->colorValue();
                    char cs[10];
                    snprintf(cs, sizeof(cs), "#%06X", c & 0xFFFFFF);
                    strokeData["color"] = std::string(cs);
                }
            }
            
            shape["stroke"] = strokeData;
            break;
        }
    }
    
    return shape;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.riv> <output.json>" << std::endl;
        return 1;
    }

    std::cout << "HIERARCHICAL EXTRACTOR - TRUE %100 Match" << std::endl;
    std::cout << std::string(79, '=') << std::endl;
    
    std::ifstream inFile(argv[1], std::ios::binary | std::ios::ate);
    if (!inFile) return 1;
    
    size_t fileSize = inFile.tellg();
    inFile.seekg(0);
    std::vector<uint8_t> buffer(fileSize);
    inFile.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    
    NoOpFactory factory;
    ImportResult importResult;
    auto file = File::import(buffer, &factory, &importResult);
    
    if (!file) return 1;
    
    // Process ALL artboards
    json output;
    output["artboards"] = json::array();
    
    for (size_t artboardIdx = 0; artboardIdx < file->artboardCount(); ++artboardIdx) {
        auto* artboard = file->artboard(artboardIdx);
        if (!artboard) continue;
        
        std::cout << "\n=== Artboard #" << artboardIdx << ": " << artboard->name() << " ===" << std::endl;
        std::cout << "Size: " << artboard->width() << "x" << artboard->height() << std::endl;
        std::cout << "Objects: " << artboard->objects().size() << std::endl;
        
        // Clear global maps for each artboard
        objToNode.clear();
        idToNode.clear();
        allNodes.clear();
        
        std::vector<Core*> allObjects(artboard->objects().begin(), artboard->objects().end());
        buildHierarchy(allObjects);
    
    // Extract SHAPES with all their children
    json hierarchicalShapes = json::array();
    int shapeCount = 0;
    int totalPaths = 0;
    int totalVertices = 0;
    
    std::cout << "\nExtracting Shapes with full hierarchy..." << std::endl;
    
    // Also extract Bones separately
    json bones = json::array();
    for (auto& nodePtr : allNodes) {
        if (nodePtr->typeKey == 40 || nodePtr->typeKey == 41 || nodePtr->typeKey == 42) { // Bone, RootBone
            json bone;
            bone["type"] = (nodePtr->typeKey == 41) ? "root" : "bone";
            
            if (auto* b = dynamic_cast<Bone*>(nodePtr->object)) {
                bone["length"] = b->length();
            }
            if (auto* rb = dynamic_cast<RootBone*>(nodePtr->object)) {
                bone["x"] = rb->x();
                bone["y"] = rb->y();
            }
            bones.push_back(bone);
        }
    }
    
    for (auto& nodePtr : allNodes) {
        // Extract ALL shape types: Node (2), Shape (3), Ellipse (4), Rectangle (7)
        if (nodePtr->typeKey == 2 || nodePtr->typeKey == 3 || nodePtr->typeKey == 4 || nodePtr->typeKey == 7) {
            json shape = extractHierarchicalShape(nodePtr.get());
            
            // Count paths in this shape
            int pathsInShape = shape.value("paths", json::array()).size();
            totalPaths += pathsInShape;
            
            for (const auto& path : shape.value("paths", json::array())) {
                totalVertices += path.value("vertices", json::array()).size();
            }
            
            hierarchicalShapes.push_back(shape);
            shapeCount++;
            
            if (shapeCount % 100 == 0) {
                std::cout << "  " << shapeCount << " shapes..." << std::endl;
            }
        }
    }
    
    std::cout << "\n✅ Extraction complete!" << std::endl;
    std::cout << "Shapes: " << shapeCount << std::endl;
    std::cout << "Paths: " << totalPaths << std::endl;
    std::cout << "Vertices: " << totalVertices << std::endl;
    
    // Build output
    json output;
    output["artboard"] = {
        {"name", artboard->name()},
        {"width", artboard->width()},
        {"height", artboard->height()}
    };
    
    output["hierarchicalShapes"] = hierarchicalShapes;
    output["bones"] = bones;
    output["animations"] = json::array();
    output["stateMachines"] = json::array();
    
    // Animations & State Machines (same as before)
    for (size_t i = 0; i < artboard->animationCount(); ++i) {
        auto* anim = artboard->animation(i);
        if (auto* la = dynamic_cast<LinearAnimation*>(anim)) {
            output["animations"].push_back({
                {"name", la->name()},
                {"fps", la->fps()},
                {"duration", la->duration()},
                {"loop", la->loop()}
            });
        }
    }
    
    for (size_t i = 0; i < artboard->stateMachineCount(); ++i) {
        auto* sm = artboard->stateMachine(i);
        json smData;
        smData["name"] = sm->name();
        smData["layers"] = json::array();
        
        std::vector<std::string> animNames;
        for (size_t j = 0; j < artboard->animationCount(); ++j) {
            if (auto* la = dynamic_cast<LinearAnimation*>(artboard->animation(j))) {
                animNames.push_back(la->name());
            }
        }
        
        for (size_t layerIdx = 0; layerIdx < sm->layerCount(); ++layerIdx) {
            auto* layer = sm->layer(layerIdx);
            json layerData;
            layerData["name"] = layer->name();
            layerData["states"] = json::array();
            
            for (size_t stateIdx = 0; stateIdx < layer->stateCount(); ++stateIdx) {
                auto* state = layer->state(stateIdx);
                if (auto* animState = dynamic_cast<AnimationState*>(state)) {
                    int animIdx = animState->animationId() - 1 - artboard->stateMachineCount();
                    std::string animName = (animIdx >= 0 && animIdx < (int)animNames.size()) 
                        ? animNames[animIdx] : "Unknown";
                    
                    layerData["states"].push_back({
                        {"name", std::string("S") + std::to_string(stateIdx)},
                        {"type", "animation"},
                        {"animationName", animName}
                    });
                }
            }
            
            layerData["transitions"] = json::array();
            smData["layers"].push_back(layerData);
        }
        
        smData["inputs"] = json::array();
        output["stateMachines"].push_back(smData);
    }
    
    // Write
    std::ofstream outFile(argv[2]);
    outFile << output.dump(2);
    
    std::cout << "\n" << std::string(79, '=') << std::endl;
    std::cout << "SUCCESS - Hierarchical extraction complete!" << std::endl;
    std::cout << std::string(79, '=') << std::endl;
    std::cout << "Output: " << argv[2] << std::endl;
    std::cout << "Shapes: " << shapeCount << " (should match original!)" << std::endl;
    std::cout << "Paths: " << totalPaths << std::endl;
    std::cout << "Vertices: " << totalVertices << std::endl;
    
    return 0;
}
