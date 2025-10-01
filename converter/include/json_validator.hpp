#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include "nlohmann/json.hpp"

namespace rive_converter
{

struct ValidationResult
{
    // Parent reference validation
    int missingParents = 0;
    std::vector<std::pair<uint32_t, uint32_t>> missingParentPairs; // (childId, parentId)
    
    // Cycle detection
    bool hasCycles = false;
    std::vector<uint32_t> cycleNodes;
    
    // Required properties validation
    std::map<uint16_t, std::vector<uint32_t>> missingRequiredProps; // typeKey -> [localIds]
    
    // Summary
    int totalObjects = 0;
    int validObjects = 0;
    
    bool isValid() const {
        return missingParents == 0 && !hasCycles && missingRequiredProps.empty();
    }
};

class JSONValidator
{
public:
    JSONValidator() = default;
    
    // Main validation function
    ValidationResult validate(const nlohmann::json& data);
    
    // Individual checks
    void checkParentReferences(const nlohmann::json& data, ValidationResult& result);
    void checkCycles(const nlohmann::json& data, ValidationResult& result);
    void checkRequiredProperties(const nlohmann::json& data, ValidationResult& result);
    
    // Print formatted results
    void printResults(const ValidationResult& result, bool verbose = false);
    
private:
    // Helper: get required properties for a given typeKey
    std::vector<std::string> getRequiredProperties(uint16_t typeKey);
    
    // Helper: DFS cycle detection
    bool detectCycleFrom(uint32_t start, 
                        const std::map<uint32_t, uint32_t>& childToParent,
                        std::vector<uint32_t>& cycle);
};

} // namespace rive_converter
