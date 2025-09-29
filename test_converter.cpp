#include "json_to_riv_converter.hpp"
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
    std::cout << "=== JSON to RIV Converter Test ===" << std::endl;
    
    try {
        // Default paths
        std::string jsonPath = "bouncing_ball.json";
        std::string outputPath = "bouncing_ball.riv";
        
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
        std::cout << "Initializing converter..." << std::endl;
        JsonToRivConverter converter;
        
        // Convert
        std::cout << "Converting JSON to RIV..." << std::endl;
        bool success = converter.convertJsonToRiv(jsonContent, outputPath);
        
        if (success) {
            std::cout << "✅ Conversion successful!" << std::endl;
            std::cout << "Output file: " << outputPath << std::endl;
            
            // Check file size
            std::ifstream outputFile(outputPath, std::ios::binary | std::ios::ate);
            if (outputFile.is_open()) {
                std::streamsize size = outputFile.tellg();
                outputFile.close();
                std::cout << "File size: " << size << " bytes" << std::endl;
            }
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

// Demo function to show JSON structure parsing
void demonstrateJsonParsing() {
    std::cout << "\n=== JSON Structure Demo ===" << std::endl;
    
    JsonToRivConverter converter;
    
    // Example JSON content (simplified)
    std::string jsonDemo = R"({
        "artboard": {
            "name": "DemoBall",
            "width": 400,
            "height": 600,
            "backgroundColor": "#87CEEB"
        },
        "objects": [
            {
                "type": "ellipse",
                "name": "ball",
                "id": 1,
                "x": 200,
                "y": 100,
                "width": 50,
                "height": 50,
                "fill": {
                    "type": "solid",
                    "color": "#FF4444"
                }
            }
        ],
        "animations": [
            {
                "name": "bounce",
                "duration": 2.0,
                "fps": 60,
                "loop": true,
                "keyframes": [
                    {
                        "objectId": 1,
                        "property": "y",
                        "keyframes": [
                            {"time": 0.0, "value": 100},
                            {"time": 1.0, "value": 500},
                            {"time": 2.0, "value": 100}
                        ]
                    }
                ]
            }
        ]
    })";
    
    try {
        json jsonData = json::parse(jsonDemo);
        
        // Parse artboard
        auto artboard = converter.parseArtboard(jsonData["artboard"]);
        std::cout << "Artboard: " << artboard.name << " (" << artboard.width << "x" << artboard.height << ")" << std::endl;
        
        // Parse objects
        auto objects = converter.parseObjects(jsonData["objects"]);
        std::cout << "Objects: " << objects.size() << std::endl;
        for (const auto& obj : objects) {
            std::cout << "  - " << obj.name << " (" << obj.type << ") at (" << obj.x << "," << obj.y << ")" << std::endl;
        }
        
        // Parse animations
        auto animations = converter.parseAnimations(jsonData["animations"]);
        std::cout << "Animations: " << animations.size() << std::endl;
        for (const auto& anim : animations) {
            std::cout << "  - " << anim.name << " (" << anim.duration << "s, " << anim.tracks.size() << " tracks)" << std::endl;
        }
        
    } catch (const json::exception& e) {
        std::cerr << "Demo parsing error: " << e.what() << std::endl;
    }
}
