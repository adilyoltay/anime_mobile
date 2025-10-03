#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include "rive/file.hpp"
#include "rive/artboard.hpp"
#include "rive/component.hpp"
#include "rive/shapes/paint/trim_path.hpp"
#include "rive/generated/component_base.hpp"
#include "utils/no_op_factory.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file.riv>" << std::endl;
        return 1;
    }
    std::ifstream in(argv[1], std::ios::binary | std::ios::ate);
    if (!in) {
        std::cerr << "Failed to open file" << std::endl;
        return 1;
    }
    std::vector<uint8_t> bytes(in.tellg());
    in.seekg(0);
    in.read(reinterpret_cast<char*>(bytes.data()), bytes.size());

    rive::NoOpFactory factory;
    rive::ImportResult result;
    auto file = rive::File::import(bytes, &factory, &result);
    if (!file || !file->artboard()) {
        std::cerr << "Import failed: " << (int)result << std::endl;
        return 1;
    }
    auto* artboard = file->artboard();
    std::cout << "Artboard objects: " << artboard->objects().size() << std::endl;

    std::unordered_map<uint32_t, rive::Component*> localIdToComponent;
    uint32_t localId = 0;
    for (auto* obj : artboard->objects()) {
        if (auto* comp = dynamic_cast<rive::Component*>(obj)) {
            localIdToComponent[localId] = comp;
        }
        localId++;
    }

    localId = 0;
    for (auto* obj : artboard->objects()) {
        if (obj && obj->coreType() == rive::TrimPath::typeKey) {
            auto* trim = obj->as<rive::TrimPath>();
            std::cout << "TrimPath localId=" << localId
                      << " start=" << trim->start()
                      << " end=" << trim->end()
                      << " offset=" << trim->offset()
                      << " mode=" << trim->modeValue() << std::endl;
            if (auto* comp = trim->as<rive::Component>()) {
                uint32_t parent = comp->parentId();
                std::cout << "  parentId=" << parent;
                if (parent != static_cast<uint32_t>(-1)) {
                    auto it = localIdToComponent.find(parent);
                    if (it != localIdToComponent.end() && it->second) {
                        std::cout << " parentTypeKey=" << it->second->coreType();
                    }
                }
                std::cout << std::endl;
            }
        }
        localId++;
    }
    return 0;
}
