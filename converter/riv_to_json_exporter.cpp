// RIV to JSON Exporter - Extract complete object tree from Casino Slots
#include <iostream>
#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>
#include "rive/file.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/points_path.hpp"
#include "rive/shapes/straight_vertex.hpp"
#include "rive/shapes/cubic_detached_vertex.hpp"
#include "rive/shapes/path_vertex.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/linear_animation.hpp"
#include "utils/no_op_factory.hpp"

using json = nlohmann::json;

void exportObjectTree(rive::Core* obj, json& out, int depth = 0) {
    if (!obj) return;
    
    std::string indent(depth * 2, ' ');
    uint16_t typeKey = obj->coreType();
    
    out["typeKey"] = typeKey;
    
    // Type-specific extraction
    if (auto* vertex = dynamic_cast<rive::StraightVertex*>(obj)) {
        out["type"] = "StraightVertex";
        out["x"] = vertex->x();
        out["y"] = vertex->y();
        out["radius"] = vertex->radius();
    }
    else if (auto* vertex = dynamic_cast<rive::CubicDetachedVertex*>(obj)) {
        out["type"] = "CubicDetachedVertex";
        out["x"] = vertex->x();
        out["y"] = vertex->y();
        out["inRotation"] = vertex->inRotation();
        out["inDistance"] = vertex->inDistance();
        out["outRotation"] = vertex->outRotation();
        out["outDistance"] = vertex->outDistance();
    }
    else if (auto* path = dynamic_cast<rive::PointsPath*>(obj)) {
        out["type"] = "PointsPath";
        out["isClosed"] = path->isClosed();
    }
    else if (auto* shape = dynamic_cast<rive::Shape*>(obj)) {
        out["type"] = "Shape";
        out["x"] = shape->x();
        out["y"] = shape->y();
    }
    else if (auto* sm = dynamic_cast<rive::StateMachine*>(obj)) {
        out["type"] = "StateMachine";
        out["name"] = sm->name();
    }
    else if (auto* layer = dynamic_cast<rive::StateMachineLayer*>(obj)) {
        out["type"] = "StateMachineLayer";
        out["name"] = layer->name();
    }
    else if (auto* anim = dynamic_cast<rive::LinearAnimation*>(obj)) {
        out["type"] = "LinearAnimation";
        out["name"] = anim->name();
        out["fps"] = anim->fps();
        out["duration"] = anim->duration();
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.riv> <output.json>" << std::endl;
        return 1;
    }

    // Read RIV file
    std::ifstream inFile(argv[1], std::ios::binary | std::ios::ate);
    if (!inFile) {
        std::cerr << "Failed to open: " << argv[1] << std::endl;
        return 1;
    }
    
    size_t fileSize = inFile.tellg();
    inFile.seekg(0);
    std::vector<uint8_t> buffer(fileSize);
    inFile.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    
    // Import
    rive::NoOpFactory factory;
    rive::ImportResult importResult;
    auto file = rive::File::import(buffer, &factory, &importResult);
    
    if (!file) {
        std::cerr << "Import failed!" << std::endl;
        return 1;
    }

    json output;
    output["fileInfo"]["version"] = "Exported from RIV";
    output["fileInfo"]["originalSize"] = fileSize;
    
    json artboards = json::array();
    
    for (size_t i = 0; i < file->artboardCount(); ++i) {
        auto* artboard = file->artboard(i);
        if (!artboard) continue;
        
        json ab;
        ab["name"] = artboard->name();
        ab["width"] = artboard->width();
        ab["height"] = artboard->height();
        ab["objectCount"] = artboard->objects().size();
        
        // Extract shapes, paths, etc.
        json objects = json::array();
        for (auto* obj : artboard->objects()) {
            json objData;
            exportObjectTree(obj, objData);
            objects.push_back(objData);
        }
        ab["objects"] = objects;
        
        // Extract state machines
        json stateMachines = json::array();
        for (size_t smIdx = 0; smIdx < artboard->stateMachineCount(); ++smIdx) {
            auto* sm = artboard->stateMachine(smIdx);
            if (sm) {
                json smData;
                smData["name"] = sm->name();
                smData["inputCount"] = sm->inputCount();
                smData["layerCount"] = sm->layerCount();
                stateMachines.push_back(smData);
            }
        }
        ab["stateMachines"] = stateMachines;
        
        artboards.push_back(ab);
    }
    
    output["artboards"] = artboards;
    
    // Write JSON
    std::ofstream outFile(argv[2]);
    outFile << output.dump(2);
    
    std::cout << "Exported to: " << argv[2] << std::endl;
    std::cout << "Artboards: " << artboards.size() << std::endl;
    
    return 0;
}
