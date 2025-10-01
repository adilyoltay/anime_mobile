#include "json_validator.hpp"
#include <iostream>
#include <fstream>

using namespace rive_converter;

static nlohmann::json loadJSON(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }
    
    nlohmann::json data;
    file >> data;
    return data;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: json_validator <input.json> [--verbose]" << std::endl;
        std::cerr << "\nValidates JSON input before RIV conversion." << std::endl;
        std::cerr << "\nChecks:" << std::endl;
        std::cerr << "  - Parent references (all parentId values must exist)" << std::endl;
        std::cerr << "  - Cycles in parent graph" << std::endl;
        std::cerr << "  - Required properties per object type" << std::endl;
        std::cerr << "\nExit codes:" << std::endl;
        std::cerr << "  0 - Validation passed (JSON is clean)" << std::endl;
        std::cerr << "  1 - Validation failed (issues found)" << std::endl;
        std::cerr << "  2 - Error (file not found, parse error, etc.)" << std::endl;
        return 2;
    }
    
    std::string filepath = argv[1];
    bool verbose = (argc > 2 && std::string(argv[2]) == "--verbose");
    
    try {
        // Load JSON
        std::cout << "Loading JSON: " << filepath << std::endl;
        auto data = loadJSON(filepath);
        
        // Validate
        JSONValidator validator;
        auto result = validator.validate(data);
        
        // Print results
        validator.printResults(result, verbose);
        
        // Exit code
        if (result.isValid()) {
            return 0; // Success
        } else {
            return 1; // Validation failed
        }
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ ERROR: " << e.what() << std::endl;
        return 2; // Error
    }
}
