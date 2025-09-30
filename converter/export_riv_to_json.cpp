#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include "rive/file.hpp"
#include "rive/artboard.hpp"
#include "rive/component.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/ellipse.hpp"
#include "rive/shapes/points_path.hpp"
#include "rive/shapes/straight_vertex.hpp"
#include "rive/shapes/cubic_detached_vertex.hpp"
#include "rive/shapes/path_vertex.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/paint/linear_gradient.hpp"
#include "rive/shapes/paint/radial_gradient.hpp"
#include "rive/shapes/paint/gradient_stop.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/text/text.hpp"
#include "utils/no_op_factory.hpp"

using json = nlohmann::json;
using namespace rive;

json export_artboard(Artboard* artboard) {
    json ab;
    ab["name"] = artboard->name();
    ab["width"] = artboard->width();
    ab["height"] = artboard->height();
    
    // Count objects by type for statistics
    std::map<uint16_t, int> typeCounts;
    json shapes = json::array();
    json customPaths = json::array();
    json texts = json::array();
    json animations = json::array();
    json stateMachines = json::array();
    
    // Iterate through all objects
    std::cout << "Scanning " << artboard->objects().size() << " objects..." << std::endl;
    
    std::map<uint32_t, json> pathMap;  // parentId -> path data
    
    for (auto* obj : artboard->objects()) {
        uint16_t typeKey = obj->coreType();
        typeCounts[typeKey]++;
        
        // Handle PointsPath containers
        if (auto* pointsPath = obj->as<PointsPath>()) {
            if (auto* comp = dynamic_cast<Component*>(obj)) {
                json pathData;
                pathData["id"] = comp->parentId();  // Track for vertices
                pathData["isClosed"] = pointsPath->isClosed();
                pathData["vertices"] = json::array();
                pathData["fillEnabled"] = false;
                pathData["strokeEnabled"] = false;
                pathMap[reinterpret_cast<uintptr_t>(obj)] = pathData;
            }
        }
        // Handle vertices
        else if (auto* straightVertex = obj->as<StraightVertex>()) {
            json vertex;
            vertex["type"] = "straight";
            vertex["x"] = straightVertex->x();
            vertex["y"] = straightVertex->y();
            vertex["radius"] = straightVertex->radius();
            
            // Find parent path
            if (auto* comp = dynamic_cast<Component*>(obj)) {
                // Add to parent path (simplified - would need parent lookup)
                std::cout << "  StraightVertex: x=" << straightVertex->x() 
                         << " y=" << straightVertex->y() << std::endl;
            }
        }
        else if (auto* cubicVertex = obj->as<CubicDetachedVertex>()) {
            json vertex;
            vertex["type"] = "cubic";
            vertex["x"] = cubicVertex->x();
            vertex["y"] = cubicVertex->y();
            vertex["inRotation"] = cubicVertex->inRotation();
            vertex["inDistance"] = cubicVertex->inDistance();
            vertex["outRotation"] = cubicVertex->outRotation();
            vertex["outDistance"] = cubicVertex->outDistance();
            
            std::cout << "  CubicVertex: x=" << cubicVertex->x() 
                     << " y=" << cubicVertex->y() << std::endl;
        }
    }
    
    // Export animations
    for (size_t i = 0; i < artboard->animationCount(); ++i) {
        auto* anim = artboard->animation(i);
        if (auto* linearAnim = dynamic_cast<LinearAnimation*>(anim)) {
            json animData;
            animData["name"] = linearAnim->name();
            animData["fps"] = linearAnim->fps();
            animData["duration"] = linearAnim->duration();
            animData["loop"] = linearAnim->loop();
            animData["scaleKeyframes"] = json::array();
            animations.push_back(animData);
        }
    }
    
    // Export state machines
    for (size_t i = 0; i < artboard->stateMachineCount(); ++i) {
        auto* sm = artboard->stateMachine(i);
        json smData;
        smData["name"] = sm->name();
        
        json inputs = json::array();
        json layers = json::array();
        
        for (size_t layerIdx = 0; layerIdx < sm->layerCount(); ++layerIdx) {
            auto* layer = sm->layer(layerIdx);
            json layerData;
            layerData["name"] = layer->name();
            layerData["states"] = json::array();
            layerData["transitions"] = json::array();
            layers.push_back(layerData);
        }
        
        smData["inputs"] = inputs;
        smData["layers"] = layers;
        stateMachines.push_back(smData);
    }
    
    ab["shapes"] = shapes;
    ab["customPaths"] = customPaths;
    ab["texts"] = texts;
    ab["animations"] = animations;
    ab["stateMachines"] = stateMachines;
    
    // Print statistics
    std::cout << "\nObject Type Statistics:" << std::endl;
    for (const auto& [typeKey, count] : typeCounts) {
        if (count > 0) {
            std::cout << "  typeKey " << typeKey << ": " << count << " objects" << std::endl;
        }
    }
    
    return ab;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.riv> <output.json>" << std::endl;
        std::cerr << "Example: " << argv[0] << " casino.riv casino_exported.json" << std::endl;
        return 1;
    }

    std::cout << "Reading RIV file: " << argv[1] << std::endl;
    
    std::ifstream inFile(argv[1], std::ios::binary | std::ios::ate);
    if (!inFile) {
        std::cerr << "Failed to open: " << argv[1] << std::endl;
        return 1;
    }
    
    size_t fileSize = inFile.tellg();
    inFile.seekg(0);
    std::vector<uint8_t> buffer(fileSize);
    inFile.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    inFile.close();
    
    std::cout << "File size: " << fileSize << " bytes" << std::endl;
    
    NoOpFactory factory;
    ImportResult importResult;
    auto file = File::import(buffer, &factory, &importResult);
    
    if (!file) {
        std::cerr << "Import failed!" << std::endl;
        return 1;
    }
    
    std::cout << "Import successful! Artboards: " << file->artboardCount() << std::endl;
    
    json output;
    output["_meta"]["source"] = argv[1];
    output["_meta"]["originalSize"] = fileSize;
    output["_meta"]["exportDate"] = "2024-09-30";
    
    json artboards = json::array();
    
    for (size_t i = 0; i < file->artboardCount(); ++i) {
        auto* artboard = file->artboard(i);
        if (artboard) {
            std::cout << "\nExporting Artboard #" << i << ": " << artboard->name() << std::endl;
            artboards.push_back(export_artboard(artboard));
        }
    }
    
    if (file->artboardCount() == 1) {
        // Single artboard - use legacy format
        output["artboard"] = artboards[0];
    } else {
        output["artboards"] = artboards;
    }
    
    std::cout << "\nWriting JSON to: " << argv[2] << std::endl;
    std::ofstream outFile(argv[2]);
    outFile << output.dump(2);
    outFile.close();
    
    std::cout << "Export complete!" << std::endl;
    
    return 0;
}
