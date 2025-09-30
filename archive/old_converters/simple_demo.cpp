#include <fstream>
#include <iostream>
#include <string>
#include "json_to_riv_converter.hpp"

// Simple demo without external dependencies
int main() {
    std::cout << "=== Simple Rive JSON to RIV Demo ===" << std::endl;
    std::cout << std::endl;
    
    // Create a simple bouncing ball JSON in code
    std::string demoJson = R"({
        "artboard": {
            "name": "SimpleBall",
            "width": 300,
            "height": 400,
            "backgroundColor": "#E0F6FF",
            "clip": true
        },
        "objects": [
            {
                "type": "ellipse",
                "name": "ball",
                "id": 1,
                "x": 125,
                "y": 50,
                "width": 50,
                "height": 50,
                "fill": {
                    "type": "solid",
                    "color": "#FF6B6B"
                },
                "stroke": {
                    "color": "#E55555",
                    "thickness": 3
                }
            },
            {
                "type": "rectangle",
                "name": "ground",
                "id": 2,
                "x": 0,
                "y": 350,
                "width": 300,
                "height": 50,
                "fill": {
                    "type": "solid",
                    "color": "#4ECDC4"
                }
            }
        ],
        "animations": [
            {
                "name": "simple_bounce",
                "duration": 1.5,
                "fps": 30,
                "loop": true,
                "keyframes": [
                    {
                        "objectId": 1,
                        "property": "y",
                        "keyframes": [
                            {
                                "time": 0.0,
                                "value": 50,
                                "interpolationType": "cubic",
                                "easeOut": true
                            },
                            {
                                "time": 0.75,
                                "value": 300,
                                "interpolationType": "cubic",
                                "easeIn": true
                            },
                            {
                                "time": 1.5,
                                "value": 50,
                                "interpolationType": "cubic",
                                "easeOut": true
                            }
                        ]
                    },
                    {
                        "objectId": 1,
                        "property": "scaleY",
                        "keyframes": [
                            {
                                "time": 0.0,
                                "value": 1.0,
                                "interpolationType": "linear"
                            },
                            {
                                "time": 0.7,
                                "value": 0.7,
                                "interpolationType": "cubic"
                            },
                            {
                                "time": 0.8,
                                "value": 1.3,
                                "interpolationType": "cubic"
                            },
                            {
                                "time": 1.5,
                                "value": 1.0,
                                "interpolationType": "linear"
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    
    std::cout << "📝 Generated demo JSON:" << std::endl;
    std::cout << "   - Artboard: 300x400px" << std::endl;
    std::cout << "   - Objects: Red ball + ground" << std::endl;
    std::cout << "   - Animation: 1.5s bounce with squash/stretch" << std::endl;
    std::cout << std::endl;
    
    try {
        // Initialize converter
        std::cout << "🔧 Initializing converter..." << std::endl;
        JsonToRivConverter converter;
        
        // Test JSON parsing
        std::cout << "📖 Parsing JSON structure..." << std::endl;
        
        json jsonData = json::parse(demoJson);
        
        // Parse components
        auto artboard = converter.parseArtboard(jsonData["artboard"]);
        std::cout << "   ✓ Artboard: " << artboard.name << " (" << artboard.width << "x" << artboard.height << ")" << std::endl;
        
        auto objects = converter.parseObjects(jsonData["objects"]);
        std::cout << "   ✓ Objects (" << objects.size() << "):" << std::endl;
        for (const auto& obj : objects) {
            std::cout << "     - " << obj.name << " (" << obj.type << ") at (" << obj.x << "," << obj.y << ")" << std::endl;
        }
        
        auto animations = converter.parseAnimations(jsonData["animations"]);
        std::cout << "   ✓ Animations (" << animations.size() << "):" << std::endl;
        for (const auto& anim : animations) {
            std::cout << "     - " << anim.name << ": " << anim.duration << "s, " << anim.tracks.size() << " tracks" << std::endl;
        }
        
        // Convert to RIV
        std::cout << std::endl << "🎬 Converting to RIV format..." << std::endl;
        bool success = converter.convertJsonToRiv(demoJson, "simple_demo.riv");
        
        if (success) {
            std::cout << "   ✅ Conversion successful!" << std::endl;
            std::cout << "   📁 Output: simple_demo.riv" << std::endl;
            
            // Show file info
            std::ifstream file("simple_demo.riv", std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                std::streamsize size = file.tellg();
                file.close();
                std::cout << "   📊 File size: " << size << " bytes" << std::endl;
            }
        } else {
            std::cout << "   ❌ Conversion failed!" << std::endl;
            return 1;
        }
        
        std::cout << std::endl << "🎉 Demo completed successfully!" << std::endl;
        std::cout << std::endl << "Next steps:" << std::endl;
        std::cout << "1. Examine the generated simple_demo.riv file" << std::endl;
        std::cout << "2. Test with Rive viewer/player" << std::endl;
        std::cout << "3. Modify the JSON structure and re-run" << std::endl;
        std::cout << "4. Integrate with your AI workflow" << std::endl;
        
    } catch (const json::exception& e) {
        std::cerr << "❌ JSON Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

// Utility function to create different animation presets
std::string createAnimationPreset(const std::string& type) {
    if (type == "bounce") {
        return R"({
            "name": "bounce",
            "duration": 2.0,
            "fps": 60,
            "loop": true,
            "keyframes": [
                {
                    "objectId": 1,
                    "property": "y",
                    "keyframes": [
                        {"time": 0.0, "value": 100, "interpolationType": "cubic"},
                        {"time": 1.0, "value": 400, "interpolationType": "cubic"},
                        {"time": 2.0, "value": 100, "interpolationType": "cubic"}
                    ]
                }
            ]
        })";
    } else if (type == "rotate") {
        return R"({
            "name": "spin",
            "duration": 3.0,
            "fps": 60,
            "loop": true,
            "keyframes": [
                {
                    "objectId": 1,
                    "property": "rotation",
                    "keyframes": [
                        {"time": 0.0, "value": 0, "interpolationType": "linear"},
                        {"time": 3.0, "value": 360, "interpolationType": "linear"}
                    ]
                }
            ]
        })";
    } else if (type == "pulse") {
        return R"({
            "name": "pulse",
            "duration": 1.0,
            "fps": 60,
            "loop": true,
            "keyframes": [
                {
                    "objectId": 1,
                    "property": "scaleX",
                    "keyframes": [
                        {"time": 0.0, "value": 1.0, "interpolationType": "cubic"},
                        {"time": 0.5, "value": 1.5, "interpolationType": "cubic"},
                        {"time": 1.0, "value": 1.0, "interpolationType": "cubic"}
                    ]
                },
                {
                    "objectId": 1,
                    "property": "scaleY",
                    "keyframes": [
                        {"time": 0.0, "value": 1.0, "interpolationType": "cubic"},
                        {"time": 0.5, "value": 1.5, "interpolationType": "cubic"},
                        {"time": 1.0, "value": 1.0, "interpolationType": "cubic"}
                    ]
                }
            ]
        })";
    }
    
    return "{}";
}
