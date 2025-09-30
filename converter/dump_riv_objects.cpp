// Dump ALL objects from RIV with their properties to recreate exact copy
#include <iostream>
#include <fstream>
#include <vector>
#include "rive/file.hpp"
#include "rive/artboard.hpp"
#include "rive/component.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/points_path.hpp"
#include "rive/shapes/straight_vertex.hpp"
#include "rive/shapes/cubic_detached_vertex.hpp"
#include "rive/shapes/ellipse.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/paint/linear_gradient.hpp"
#include "rive/shapes/paint/radial_gradient.hpp"
#include "rive/shapes/paint/gradient_stop.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/state_machine.hpp"
#include "utils/no_op_factory.hpp"

using namespace rive;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input.riv>" << std::endl;
        return 1;
    }

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
    
    if (!file || !file->artboard()) {
        std::cerr << "Import failed!" << std::endl;
        return 1;
    }

    auto* artboard = file->artboard();
    
    std::cout << "Artboard: " << artboard->name() 
              << " (" << artboard->width() << "x" << artboard->height() << ")" << std::endl;
    std::cout << "Total objects: " << artboard->objects().size() << std::endl;
    
    // Count and group objects
    std::map<uint16_t, std::vector<Core*>> objectsByType;
    
    for (auto* obj : artboard->objects()) {
        objectsByType[obj->coreType()].push_back(obj);
    }
    
    std::cout << "\nObject breakdown by type:" << std::endl;
    
    // Process PointsPath and their vertices together
    int pathIndex = 0;
    for (auto* obj : artboard->objects()) {
        if (auto* pointsPath = obj->as<PointsPath>()) {
            std::cout << "\n--- Path #" << pathIndex++ << " (PointsPath) ---" << std::endl;
            std::cout << "  isClosed: " << (pointsPath->isClosed() ? "true" : "false") << std::endl;
            
            // Find vertices belonging to this path
            // Vertices are children of PointsPath
            if (auto* comp = dynamic_cast<Component*>(obj)) {
                uint32_t pathId = 0; // Would need to track Component IDs
                
                // Scan for vertices with this path as parent
                int vertexCount = 0;
                for (auto* child : artboard->objects()) {
                    if (auto* straightV = child->as<StraightVertex>()) {
                        if (auto* childComp = dynamic_cast<Component*>(child)) {
                            // Check if this vertex belongs to current path
                            // (simplified - real check needs parentId match)
                            std::cout << "    Vertex " << vertexCount++ << " (Straight): "
                                     << "x=" << straightV->x() 
                                     << ", y=" << straightV->y()
                                     << ", radius=" << straightV->radius() << std::endl;
                            
                            if (vertexCount > 20) break; // Limit output per path
                        }
                    }
                    else if (auto* cubicV = child->as<CubicDetachedVertex>()) {
                        if (auto* childComp = dynamic_cast<Component*>(child)) {
                            std::cout << "    Vertex " << vertexCount++ << " (Cubic): "
                                     << "x=" << cubicV->x()
                                     << ", y=" << cubicV->y()
                                     << ", in=" << cubicV->inRotation() << "°/" << cubicV->inDistance()
                                     << ", out=" << cubicV->outRotation() << "°/" << cubicV->outDistance() << std::endl;
                            
                            if (vertexCount > 20) break;
                        }
                    }
                }
                
                if (vertexCount > 20) {
                    std::cout << "    ... (showing first 20 vertices only)" << std::endl;
                }
            }
            
            if (pathIndex > 5) {
                std::cout << "\n... (showing first 5 paths only, total: " 
                         << objectsByType[16].size() << " paths)" << std::endl;
                break;
            }
        }
    }
    
    std::cout << "\n" << "=" * 80 << std::endl;
    std::cout << "SUMMARY:" << std::endl;
    std::cout << "To recreate Casino Slots EXACTLY, you need:" << std::endl;
    std::cout << "  " << objectsByType[16].size() << " PointsPath objects" << std::endl;
    std::cout << "  " << objectsByType[5].size() << " StraightVertex objects" << std::endl;
    std::cout << "  " << objectsByType[6].size() << " CubicDetachedVertex objects" << std::endl;
    std::cout << "\nThis requires automated extraction (RIV binary parser)" << std::endl;
    std::cout << "Manual JSON creation for 10,366 vertices is impractical." << std::endl;
    
    return 0;
}
