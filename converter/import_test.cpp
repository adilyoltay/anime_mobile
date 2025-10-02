#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include "rive/file.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/layer_state.hpp"
#include "utils/no_op_factory.hpp"

int main(int argc, char* argv[])
{
    if (argc < 2 || argc > 3)
    {
        std::cerr << "Usage: " << argv[0] << " <riv_file> [expected_object_count]" << std::endl;
        return 1;
    }

    const char* rivPath = argv[1];
    long expectedObjectCount = -1;
    if (argc == 3)
    {
        char* endPtr = nullptr;
        expectedObjectCount = std::strtol(argv[2], &endPtr, 10);
        if (endPtr == argv[2] || *endPtr != '\0' || expectedObjectCount < 0)
        {
            std::cerr << "Invalid expected object count: " << argv[2] << std::endl;
            return 1;
        }
    }

    // Read file
    std::ifstream inFile(rivPath, std::ios::binary | std::ios::ate);
    if (!inFile)
    {
        std::cerr << "Failed to open file: " << rivPath << std::endl;
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
        
        // Accumulate NULL count across ALL artboards
        int totalNullCount = 0;
        long totalObjectCount = 0;
        
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
                
                // Check for NULL objects in THIS artboard
                int artboardNullCount = 0;
                size_t objIdx = 0;
                for (auto* obj : ab->objects()) {
                    if (obj == nullptr) {
                        std::cout << "  Object[" << objIdx << "]: NULL!" << std::endl;
                        artboardNullCount++;
                    }
                    objIdx++;
                }
                
                totalNullCount += artboardNullCount;
                totalObjectCount += ab->objects().size();
                
                if (artboardNullCount > 0) {
                    std::cout << "  ⚠️  Found " << artboardNullCount << " NULL objects in this artboard" << std::endl;
                }
                
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
        
        // CRITICAL: Check total NULL count across ALL artboards
        if (totalNullCount > 0) {
            std::cerr << "\n❌ IMPORT FAILED: Found " << totalNullCount << " NULL objects out of " 
                      << totalObjectCount << " across " << file->artboardCount() << " artboard(s) (" 
                      << (totalNullCount * 100.0 / totalObjectCount) << "%)" << std::endl;
            std::cerr << "NULL objects are genuine serialization defects that will crash Rive Play!" << std::endl;
            std::cerr << "File::readRuntimeObject() returned nullptr - runtime cannot deserialize these objects." << std::endl;
            return 1;  // Hard failure
        }

        if (expectedObjectCount >= 0 && totalObjectCount != expectedObjectCount)
        {
            std::cerr << "\n❌ IMPORT FAILED: Object count mismatch" << std::endl;
            std::cerr << "  Expected: " << expectedObjectCount << std::endl;
            std::cerr << "  Actual:   " << totalObjectCount << std::endl;
            std::cerr << "  Diff:     " << (totalObjectCount - expectedObjectCount) << std::endl;
                
            return 1;
        }
        else if (expectedObjectCount >= 0)
        {
            std::cout << "Object count matches expected total (" << totalObjectCount << ")" << std::endl;
        }
        
        if (file->artboard())
        {
            std::cout << "Artboard name: " << file->artboard()->name() << std::endl;
            std::cout << "Artboard width: " << file->artboard()->width() << std::endl;
            std::cout << "Artboard height: " << file->artboard()->height() << std::endl;

            // Check for Text objects (diagnostic info only - NULL check already done above)
            auto* artboard = file->artboard();
            std::cout << "Artboard child count: " << artboard->objects().size() << std::endl;
            
            int textCount = 0, textStyleCount = 0, textRunCount = 0;
            size_t objIdx = 0;
            for (auto* obj : artboard->objects()) {
                if (obj == nullptr) {
                    // Skip NULLs - already caught by multi-artboard check above
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
