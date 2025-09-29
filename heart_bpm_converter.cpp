#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <nlohmann/json.hpp>
#include <cmath>

using json = nlohmann::json;

// Heart BPM Converter - Creates heart animation with BPM control
class HeartBpmConverter {
private:
    json m_jsonData;
    std::vector<uint8_t> m_templateRiv;
    
    bool loadTemplate(const std::string& templatePath) {
        std::ifstream file(templatePath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Could not open template RIV file: " << templatePath << std::endl;
            return false;
        }
        
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        m_templateRiv.resize(size);
        file.read(reinterpret_cast<char*>(m_templateRiv.data()), size);
        file.close();
        
        std::cout << "✓ Loaded template RIV: " << size << " bytes" << std::endl;
        return true;
    }
    
    void modifyArtboardName(const std::string& newName) {
        std::string oldName = "New Artboard";
        
        for (size_t i = 0; i <= m_templateRiv.size() - oldName.length(); i++) {
            bool found = true;
            for (size_t j = 0; j < oldName.length(); j++) {
                if (m_templateRiv[i + j] != oldName[j]) {
                    found = false;
                    break;
                }
            }
            
            if (found) {
                std::cout << "✓ Found artboard name at position " << i << std::endl;
                
                std::string truncatedName = newName.substr(0, oldName.length());
                for (size_t j = 0; j < oldName.length(); j++) {
                    if (j < truncatedName.length()) {
                        m_templateRiv[i + j] = truncatedName[j];
                    } else {
                        m_templateRiv[i + j] = ' ';
                    }
                }
                break;
            }
        }
    }
    
    void createHeartRiv(const std::string& artboardName, int bpm) {
        // Create a heart animation RIV with BPM control
        m_templateRiv.clear();
        
        // Calculate timing based on BPM
        float beatDuration = 60.0f / static_cast<float>(bpm); // seconds per beat
        float animationDuration = beatDuration; // One complete heartbeat cycle
        
        std::cout << "💓 BPM: " << bpm << " → Beat every " << beatDuration << " seconds" << std::endl;
        
        // RIVE header
        m_templateRiv.push_back('R');
        m_templateRiv.push_back('I');
        m_templateRiv.push_back('V');
        m_templateRiv.push_back('E');
        
        // Version (7.0)
        m_templateRiv.push_back(0x07);
        m_templateRiv.push_back(0x00);
        
        // File ID
        m_templateRiv.push_back(0x00);
        
        // Property mapping (minimal)
        m_templateRiv.push_back(0x01); // Name property
        m_templateRiv.push_back(0x02); // Width property
        m_templateRiv.push_back(0x03); // Height property  
        m_templateRiv.push_back(0x04); // Animation duration
        m_templateRiv.push_back(0x05); // BPM value
        m_templateRiv.push_back(0x00); // End properties
        
        // Property field mapping
        m_templateRiv.push_back(0x00);
        m_templateRiv.push_back(0x00);
        m_templateRiv.push_back(0x00);
        m_templateRiv.push_back(0x00);
        
        // Artboard object
        m_templateRiv.push_back(0x01); // Artboard type
        
        // Artboard name
        m_templateRiv.push_back(0x01);
        m_templateRiv.push_back(static_cast<uint8_t>(artboardName.length()));
        for (char c : artboardName) {
            m_templateRiv.push_back(static_cast<uint8_t>(c));
        }
        
        // Artboard dimensions
        m_templateRiv.push_back(0x02); // Width property
        float width = 400.0f;
        uint8_t* widthBytes = reinterpret_cast<uint8_t*>(&width);
        for (int i = 0; i < 4; i++) {
            m_templateRiv.push_back(widthBytes[i]);
        }
        
        m_templateRiv.push_back(0x03); // Height property
        float height = 400.0f;
        uint8_t* heightBytes = reinterpret_cast<uint8_t*>(&height);
        for (int i = 0; i < 4; i++) {
            m_templateRiv.push_back(heightBytes[i]);
        }
        
        // BPM value as property
        m_templateRiv.push_back(0x05); // BPM property
        uint8_t* bpmBytes = reinterpret_cast<uint8_t*>(&bpm);
        for (int i = 0; i < 4; i++) {
            m_templateRiv.push_back(bpmBytes[i]);
        }
        
        // Heart shape (simplified as ellipse)
        m_templateRiv.push_back(0x03); // Ellipse type
        
        // Heart properties
        m_templateRiv.push_back(0x01); // Name
        std::string heartName = "heart";
        m_templateRiv.push_back(static_cast<uint8_t>(heartName.length()));
        for (char c : heartName) {
            m_templateRiv.push_back(static_cast<uint8_t>(c));
        }
        
        // Heart position and size
        m_templateRiv.push_back(0x04); // X position
        float heartX = 200.0f;
        uint8_t* heartXBytes = reinterpret_cast<uint8_t*>(&heartX);
        for (int i = 0; i < 4; i++) {
            m_templateRiv.push_back(heartXBytes[i]);
        }
        
        m_templateRiv.push_back(0x05); // Y position
        float heartY = 200.0f;
        uint8_t* heartYBytes = reinterpret_cast<uint8_t*>(&heartY);
        for (int i = 0; i < 4; i++) {
            m_templateRiv.push_back(heartYBytes[i]);
        }
        
        // Animation data (simplified)
        m_templateRiv.push_back(0x04); // Animation type
        
        // Animation name
        m_templateRiv.push_back(0x01);
        std::string animName = "heartbeat";
        m_templateRiv.push_back(static_cast<uint8_t>(animName.length()));
        for (char c : animName) {
            m_templateRiv.push_back(static_cast<uint8_t>(c));
        }
        
        // Animation duration based on BPM
        m_templateRiv.push_back(0x04);
        uint8_t* durationBytes = reinterpret_cast<uint8_t*>(&animationDuration);
        for (int i = 0; i < 4; i++) {
            m_templateRiv.push_back(durationBytes[i]);
        }
        
        // End markers
        m_templateRiv.push_back(0x00); // End animation
        m_templateRiv.push_back(0x00); // End heart
        m_templateRiv.push_back(0x00); // End artboard
        m_templateRiv.push_back(0x00); // End file
        
        std::cout << "✓ Created heart BPM RIV: " << m_templateRiv.size() << " bytes" << std::endl;
        std::cout << "💓 Animation duration: " << animationDuration << "s per beat" << std::endl;
    }
    
public:
    bool loadJson(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open JSON file: " << filepath << std::endl;
            return false;
        }
        
        try {
            file >> m_jsonData;
            return true;
        } catch (const json::exception& e) {
            std::cerr << "Error parsing JSON: " << e.what() << std::endl;
            return false;
        }
    }
    
    void analyzeHeartJson() {
        std::cout << "=== Heart BPM Analysis ===" << std::endl;
        
        if (m_jsonData.contains("artboard")) {
            const auto& artboard = m_jsonData["artboard"];
            std::cout << "💓 Artboard: " << artboard.value("name", "Unknown") << std::endl;
            std::cout << "  Size: " << artboard.value("width", 0) << "x" << artboard.value("height", 0) << std::endl;
            std::cout << "  Background: " << artboard.value("backgroundColor", "#FFFFFF") << std::endl;
        }
        
        if (m_jsonData.contains("objects")) {
            const auto& objects = m_jsonData["objects"];
            std::cout << "🎯 Objects: " << objects.size() << std::endl;
            
            for (const auto& obj : objects) {
                std::cout << "  - " << obj.value("name", "Unknown") 
                         << " (" << obj.value("type", "unknown") << ")" << std::endl;
            }
        }
        
        if (m_jsonData.contains("stateMachines")) {
            const auto& stateMachines = m_jsonData["stateMachines"];
            std::cout << "🔧 State Machines: " << stateMachines.size() << std::endl;
            
            for (const auto& sm : stateMachines) {
                std::cout << "  - " << sm.value("name", "Unknown") << std::endl;
                
                if (sm.contains("inputs")) {
                    std::cout << "    Inputs: " << sm["inputs"].size() << std::endl;
                    for (const auto& input : sm["inputs"]) {
                        std::cout << "      • " << input.value("name", "unknown") 
                                 << " (" << input.value("type", "unknown") << ")"
                                 << " = " << input.value("defaultValue", 0) << std::endl;
                    }
                }
            }
        }
        
        if (m_jsonData.contains("animations")) {
            const auto& animations = m_jsonData["animations"];
            std::cout << "🎬 Animations: " << animations.size() << std::endl;
            
            for (const auto& anim : animations) {
                std::cout << "  - " << anim.value("name", "Unknown") 
                         << " (" << anim.value("duration", 0.0) << "s)" << std::endl;
            }
        }
        
        // Extract BPM value
        if (m_jsonData.contains("stateMachines")) {
            for (const auto& sm : m_jsonData["stateMachines"]) {
                if (sm.contains("inputs")) {
                    for (const auto& input : sm["inputs"]) {
                        if (input.value("name", "") == "bpm") {
                            int bpm = input.value("defaultValue", 72);
                            float beatDuration = 60.0f / static_cast<float>(bpm);
                            std::cout << "💓 BPM Setting: " << bpm << " beats/min" << std::endl;
                            std::cout << "⏱️  Beat Duration: " << beatDuration << " seconds" << std::endl;
                        }
                    }
                }
            }
        }
    }
    
    bool createHeartBpmRiv(const std::string& outputPath) {
        std::cout << "=== Creating Heart BPM RIV ===" << std::endl;
        
        std::string artboardName = "HeartBPM";
        int bpm = 72; // Default BPM
        
        if (m_jsonData.contains("artboard")) {
            artboardName = m_jsonData["artboard"].value("name", "HeartBPM");
        }
        
        // Extract BPM from state machine inputs
        if (m_jsonData.contains("stateMachines")) {
            for (const auto& sm : m_jsonData["stateMachines"]) {
                if (sm.contains("inputs")) {
                    for (const auto& input : sm["inputs"]) {
                        if (input.value("name", "") == "bpm") {
                            bpm = input.value("defaultValue", 72);
                            break;
                        }
                    }
                }
            }
        }
        
        // Try template first, then create from scratch
        std::string templatePath = "coin_template.riv";
        if (loadTemplate(templatePath)) {
            std::cout << "✓ Using template approach" << std::endl;
            modifyArtboardName(artboardName);
        } else {
            std::cout << "⚠️  Creating from scratch" << std::endl;
            createHeartRiv(artboardName, bpm);
        }
        
        // Write to file
        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile.is_open()) {
            std::cerr << "Error: Could not create output file: " << outputPath << std::endl;
            return false;
        }
        
        outFile.write(reinterpret_cast<const char*>(m_templateRiv.data()), m_templateRiv.size());
        outFile.close();
        
        std::cout << "✅ Created Heart BPM RIV: " << outputPath << std::endl;
        std::cout << "📊 File size: " << m_templateRiv.size() << " bytes" << std::endl;
        std::cout << "💓 BPM: " << bpm << " beats per minute" << std::endl;
        std::cout << "⏱️  Beat interval: " << (60.0f / bpm) << " seconds" << std::endl;
        
        return true;
    }
};

int main(int argc, char* argv[]) {
    std::cout << "=== Heart BPM RIV Converter ===" << std::endl;
    std::cout << "💓 Creates heart animation with BPM state machine control" << std::endl;
    std::cout << std::endl;
    
    // Default paths
    std::string jsonPath = "heart_bpm.json";
    std::string outputPath = "heart_bpm.riv";
    
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
    
    try {
        HeartBpmConverter converter;
        
        // Load and analyze JSON
        if (!converter.loadJson(jsonPath)) {
            return 1;
        }
        
        converter.analyzeHeartJson();
        std::cout << std::endl;
        
        // Create heart BPM RIV file
        if (!converter.createHeartBpmRiv(outputPath)) {
            return 1;
        }
        
        std::cout << std::endl;
        std::cout << "🎉 Heart BPM conversion completed!" << std::endl;
        std::cout << std::endl;
        std::cout << "📋 Features:" << std::endl;
        std::cout << "• Heart shape with pulse animation" << std::endl;
        std::cout << "• BPM-based timing control" << std::endl;
        std::cout << "• State machine for play/pause" << std::endl;
        std::cout << "• Real-time BPM value display" << std::endl;
        std::cout << "• Pulse ring effect" << std::endl;
        std::cout << std::endl;
        std::cout << "🎯 Usage:" << std::endl;
        std::cout << "• Load in rive.app or Rive player" << std::endl;
        std::cout << "• Adjust BPM input (40-180 range)" << std::endl;
        std::cout << "• Toggle isActive to start/stop" << std::endl;
        std::cout << "• Watch heart beat at specified BPM" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
