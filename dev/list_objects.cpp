#include <iostream>
#include <fstream>
#include <vector>
#include "rive/file.hpp"
#include "rive/artboard.hpp"
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
        std::cerr << "Import failed" << std::endl;
        return 1;
    }
    auto* artboard = file->artboard();
    size_t idx = 0;
    for (auto* obj : artboard->objects()) {
        std::cout << idx++ << ": typeKey=" << obj->coreType() << std::endl;
    }
    return 0;
}
