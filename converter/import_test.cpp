#include <iostream>
#include <fstream>
#include <vector>
#include "rive/file.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/layer_state.hpp"
#include "utils/no_op_factory.hpp"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <riv_file>" << std::endl;
        return 1;
    }

    // Read file
    std::ifstream inFile(argv[1], std::ios::binary | std::ios::ate);
    if (!inFile)
    {
        std::cerr << "Failed to open file: " << argv[1] << std::endl;
        return 1;
    }
    
    size_t fileSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(fileSize);
    if (!inFile.read(reinterpret_cast<char*>(buffer.data()), fileSize))
    {
        std::cerr << "Failed to read file" << std::endl;
        return 1;
    }
    
    // Try to import
    rive::NoOpFactory factory;
    rive::ImportResult importResult;
    auto file = rive::File::import(buffer, &factory, &importResult);
    
    if (file)
    {
        std::cout << "SUCCESS: File imported successfully!" << std::endl;
        std::cout << "Artboard count: " << file->artboardCount() << std::endl;
        
        // List all artboards with their state machines
        for (size_t i = 0; i < file->artboardCount(); ++i)
        {
            auto* ab = file->artboard(i);
            if (ab)
            {
                std::cout << "\n=== Artboard #" << i << ": '" << ab->name() << "' ===" << std::endl;
                std::cout << "  Size: " << ab->width() << "x" << ab->height() << std::endl;
                std::cout << "  Objects: " << ab->objects().size() << std::endl;
                std::cout << "  State Machines: " << ab->stateMachineCount() << std::endl;
                
                // Show state machines for this artboard
                for (size_t smIdx = 0; smIdx < ab->stateMachineCount(); ++smIdx)
                {
                    auto* sm = ab->stateMachine(smIdx);
                    if (sm)
                    {
                        std::cout << "    SM #" << smIdx << ": '" << sm->name() << "'" << std::endl;
                        std::cout << "      Inputs: " << sm->inputCount() << std::endl;
                        std::cout << "      Layers: " << sm->layerCount() << std::endl;
                        
                        for (size_t layerIdx = 0; layerIdx < sm->layerCount(); ++layerIdx)
                        {
                            auto* layer = sm->layer(layerIdx);
                            if (layer)
                            {
                                std::cout << "        Layer #" << layerIdx << ": '" << layer->name() 
                                          << "' (" << layer->stateCount() << " states)" << std::endl;
                            }
                        }
                    }
                }
            }
        }
        
        if (file->artboard())
        {
            std::cout << "Artboard name: " << file->artboard()->name() << std::endl;
            std::cout << "Artboard width: " << file->artboard()->width() << std::endl;
            std::cout << "Artboard height: " << file->artboard()->height() << std::endl;

            // Check for Text objects
            auto* artboard = file->artboard();
            std::cout << "Artboard child count: " << artboard->objects().size() << std::endl;
            
            int textCount = 0, textStyleCount = 0, textRunCount = 0;
            int nullCount = 0;
            size_t objIdx = 0;
            for (auto* obj : artboard->objects()) {
                if (obj == nullptr) {
                    std::cout << "  Object[" << objIdx << "]: NULL!" << std::endl;
                    nullCount++;
                    objIdx++;
                    continue;
                }
                std::cout << "  Object[" << objIdx << "] typeKey=" << obj->coreType() << std::endl;
                if (obj->coreType() == 134) textCount++;
                if (obj->coreType() == 137) textStyleCount++;
                if (obj->coreType() == 135) textRunCount++;
                objIdx++;
            }
            std::cout << "Total: " << textCount << " Text, " << textStyleCount << " TextStylePaint, " << textRunCount << " TextValueRun" << std::endl;
            
            // CRITICAL: NULL objects indicate serialization defects
            if (nullCount > 0) {
                std::cerr << "\n❌ IMPORT FAILED: Found " << nullCount << " NULL objects out of " 
                          << artboard->objects().size() << " (" 
                          << (nullCount * 100.0 / artboard->objects().size()) << "%)" << std::endl;
                std::cerr << "NULL objects are genuine serialization defects that will crash Rive Play!" << std::endl;
                std::cerr << "File::readRuntimeObject() returned nullptr - runtime cannot deserialize these objects." << std::endl;
                return 1;  // Hard failure
            }
            
            // Check for state machines
            std::cout << "\nState Machines:" << std::endl;
            for (size_t i = 0; i < file->artboard()->stateMachineCount(); ++i)
            {
                auto* sm = file->artboard()->stateMachine(i);
                if (sm)
                {
                    std::cout << "  StateMachine #" << i << ": name='" << sm->name() << "'" << std::endl;
                    std::cout << "    Inputs: " << sm->inputCount() << std::endl;
                    std::cout << "    Layers: " << sm->layerCount() << std::endl;
                    
                    // Show layer details
                    for (size_t layerIdx = 0; layerIdx < sm->layerCount(); ++layerIdx)
                    {
                        auto* layer = sm->layer(layerIdx);
                        if (layer)
                        {
                            std::cout << "      Layer #" << layerIdx << ": name='" << layer->name() << "'" << std::endl;
                            std::cout << "        States: " << layer->stateCount() << std::endl;
                            
                            // Show state details
                            for (size_t stateIdx = 0; stateIdx < layer->stateCount(); ++stateIdx)
                            {
                                auto* state = layer->state(stateIdx);
                                if (state)
                                {
                                    std::string stateType = "Unknown";
                                    uint16_t typeKey = state->coreType();
                                    if (typeKey == 63) stateType = "EntryState";
                                    else if (typeKey == 64) stateType = "ExitState";
                                    else if (typeKey == 62) stateType = "AnyState";
                                    else if (typeKey == 61) stateType = "AnimationState";
                                    
                                    std::cout << "          State #" << stateIdx << ": " << stateType 
                                              << " (typeKey=" << typeKey << ")" << std::endl;
                                }
                            }
                        }
                    }
                }
            }
            
            auto instance = artboard->instance();
            if (instance == nullptr)
            {
                std::cout << "Artboard instance could not be created" << std::endl;
            }
            else
            {
                instance->advance(0.0f);
                std::cout << "\nArtboard instance initialized." << std::endl;
            }
        }
    }
    else
    {
        std::cout << "FAILED: Import failed - file is null" << std::endl;
        std::cout << "Import result: " << static_cast<int>(importResult) << std::endl;
        switch (importResult)
        {
            case rive::ImportResult::success:
                std::cout << "  Status: Success (but file is null?)" << std::endl;
                break;
            case rive::ImportResult::unsupportedVersion:
                std::cout << "  Status: Unsupported version" << std::endl;
                break;
            case rive::ImportResult::malformed:
                std::cout << "  Status: Malformed file" << std::endl;
                break;
            default:
                std::cout << "  Status: Unknown" << std::endl;
        }
    }
    
    return 0;
}
