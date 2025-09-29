#include "real_json_to_riv_converter.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

std::string readJsonFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open JSON file: " + filepath);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return buffer.str();
}

int main(int argc, char* argv[]) {
    std::cout << "=== Real JSON to RIV Converter ===" << std::endl;
    std::cout << "Using actual Rive runtime API for RIV generation" << std::endl;
    std::cout << std::endl;
    
    try {
        // Default paths
        std::string jsonPath = "bouncing_ball.json";
        std::string outputPath = "bouncing_ball_real.riv";
        
        // Parse command line arguments
        if (argc >= 2) {
            jsonPath = argv[1];
        }
        if (argc >= 3) {
            outputPath = argv[2];
        }
        
        std::cout << "Input JSON: " << jsonPath << std::endl;
        std::cout << "Output RIV: " << outputPath << std::endl;
        std::cout << std::endl;
        
        // Read JSON file
        std::cout << "Reading JSON file..." << std::endl;
        std::string jsonContent = readJsonFile(jsonPath);
        
        // Create converter
        std::cout << "Initializing real converter..." << std::endl;
        RealJsonToRivConverter converter;
        
        // Convert
        std::cout << "Converting JSON to real RIV format..." << std::endl;
        bool success = converter.convertJsonToRiv(jsonContent, outputPath);
        
        if (success) {
            std::cout << std::endl;
            std::cout << "🎉 Conversion successful!" << std::endl;
            std::cout << "Output file: " << outputPath << std::endl;
            
            // Check file size
            std::ifstream outputFile(outputPath, std::ios::binary | std::ios::ate);
            if (outputFile.is_open()) {
                std::streamsize size = outputFile.tellg();
                outputFile.close();
                std::cout << "File size: " << size << " bytes" << std::endl;
            }
            
            std::cout << std::endl;
            std::cout << "Note: This RIV file uses proper Rive binary format" << std::endl;
            std::cout << "and should be compatible with Rive players/viewers." << std::endl;
            
        } else {
            std::cout << "❌ Conversion failed!" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
