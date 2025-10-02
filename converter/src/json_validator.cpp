#include "json_validator.hpp"
#include <iostream>
#include <unordered_set>
#include <unordered_map>

namespace rive_converter
{

ValidationResult JSONValidator::validate(const nlohmann::json& data)
{
    ValidationResult result;
    
    // Check if artboards array exists
    if (!data.contains("artboards") || !data["artboards"].is_array() || data["artboards"].empty()) {
        std::cerr << "❌ ERROR: No artboards array found" << std::endl;
        return result;
    }
    
    auto& artboard = data["artboards"][0];
    if (!artboard.contains("objects") || !artboard["objects"].is_array()) {
        std::cerr << "❌ ERROR: No objects array in artboard" << std::endl;
        return result;
    }
    
    result.totalObjects = artboard["objects"].size();
    
    // Run all checks
    checkParentReferences(data, result);
    checkCycles(data, result);
    checkRequiredProperties(data, result);
    
    result.validObjects = result.totalObjects - result.missingParents - result.missingRequiredProps.size();
    
    return result;
}

void JSONValidator::checkParentReferences(const nlohmann::json& data, ValidationResult& result)
{
    auto& objects = data["artboards"][0]["objects"];
    
    // Build set of all localIds
    std::unordered_set<uint32_t> localIds;
    for (auto& obj : objects) {
        if (obj.contains("localId")) {
            localIds.insert(obj["localId"].get<uint32_t>());
        }
    }
    
    // Check each parent reference
    for (auto& obj : objects) {
        if (obj.contains("parentId")) {
            uint32_t parentId = obj["parentId"].get<uint32_t>();
            if (localIds.find(parentId) == localIds.end()) {
                result.missingParents++;
                uint32_t childId = obj.contains("localId") ? obj["localId"].get<uint32_t>() : 0;
                result.missingParentPairs.push_back({childId, parentId});
            }
        }
    }
}

void JSONValidator::checkCycles(const nlohmann::json& data, ValidationResult& result)
{
    auto& objects = data["artboards"][0]["objects"];
    
    // Build parent map
    std::map<uint32_t, uint32_t> childToParent;
    std::unordered_set<uint32_t> allNodes;
    
    for (auto& obj : objects) {
        if (obj.contains("localId")) {
            uint32_t localId = obj["localId"].get<uint32_t>();
            allNodes.insert(localId);
            
            if (obj.contains("parentId")) {
                uint32_t parentId = obj["parentId"].get<uint32_t>();
                childToParent[localId] = parentId;
            }
        }
    }
    
    // Check for cycles from each node
    for (uint32_t node : allNodes) {
        std::vector<uint32_t> cycle;
        if (detectCycleFrom(node, childToParent, cycle)) {
            result.hasCycles = true;
            result.cycleNodes = cycle;
            break; // Found one cycle, that's enough
        }
    }
}

bool JSONValidator::detectCycleFrom(uint32_t start, 
                                    const std::map<uint32_t, uint32_t>& childToParent,
                                    std::vector<uint32_t>& cycle)
{
    std::unordered_set<uint32_t> visited;
    std::vector<uint32_t> path;
    uint32_t cur = start;
    
    while (true) {
        if (visited.count(cur)) {
            // Cycle detected - extract cycle nodes
            auto it = std::find(path.begin(), path.end(), cur);
            if (it != path.end()) {
                cycle.assign(it, path.end());
                cycle.push_back(cur);
            }
            return true;
        }
        
        visited.insert(cur);
        path.push_back(cur);
        
        auto parentIt = childToParent.find(cur);
        if (parentIt == childToParent.end()) {
            break; // No parent, end of chain
        }
        
        cur = parentIt->second;
    }
    
    return false;
}

void JSONValidator::checkRequiredProperties(const nlohmann::json& data, ValidationResult& result)
{
    auto& objects = data["artboards"][0]["objects"];
    
    for (auto& obj : objects) {
        if (!obj.contains("typeKey")) continue;
        
        uint16_t typeKey = obj["typeKey"].get<uint16_t>();
        auto requiredProps = getRequiredProperties(typeKey);
        
        if (requiredProps.empty()) continue; // No required properties for this type
        
        auto props = obj.value("properties", nlohmann::json::object());
        bool hasMissing = false;
        
        for (auto& propName : requiredProps) {
            if (!props.contains(propName)) {
                hasMissing = true;
                break;
            }
        }
        
        if (hasMissing) {
            uint32_t localId = obj.contains("localId") ? obj["localId"].get<uint32_t>() : 0;
            result.missingRequiredProps[typeKey].push_back(localId);
        }
    }
}

std::vector<std::string> JSONValidator::getRequiredProperties(uint16_t typeKey)
{
    switch (typeKey) {
        case 47: // TrimPath
            return {"start", "end", "offset", "modeValue"};
        
        case 49: // Feather
            return {"strength", "offsetX", "offsetY", "inner"};
        
        case 507: // Dash (DashBase::typeKey)
            return {"length", "lengthIsPercentage"};
        
        case 506: // DashPath (DashPathBase::typeKey, NOT 46 which is CubicWeight)
            return {"offset", "offsetIsPercentage"};
        
        case 19: // GradientStop
            return {"colorValue", "position"};
        
        default:
            return {}; // No required properties or not tracked
    }
}

// PR-TrimPath-Compat: Validate value ranges for TrimPath
bool validateTrimPathRanges(const nlohmann::json& props, uint32_t localId)
{
    if (!props.contains("start") || !props.contains("end")) {
        return true; // Missing properties caught by checkRequiredProperties
    }
    
    double start = props.value("start", 0.0);
    double end = props.value("end", 1.0);
    
    // Check normalized range: 0 <= start <= end <= 1
    if (start < 0.0 || start > 1.0) {
        std::cerr << "⚠️  TrimPath localId=" << localId 
                  << " has invalid start=" << start << " (must be 0-1)" << std::endl;
        return false;
    }
    
    if (end < 0.0 || end > 1.0) {
        std::cerr << "⚠️  TrimPath localId=" << localId 
                  << " has invalid end=" << end << " (must be 0-1)" << std::endl;
        return false;
    }
    
    if (start > end) {
        std::cerr << "⚠️  TrimPath localId=" << localId 
                  << " has start=" << start << " > end=" << end << " (invalid range)" << std::endl;
        return false;
    }
    
    return true;
}

void JSONValidator::printResults(const ValidationResult& result, bool verbose)
{
    std::cout << "\n=== JSON VALIDATION RESULTS ===" << std::endl;
    std::cout << "Total objects: " << result.totalObjects << std::endl;
    
    if (result.isValid()) {
        std::cout << "✅ VALIDATION PASSED - JSON is clean" << std::endl;
        std::cout << "  - All parent references valid" << std::endl;
        std::cout << "  - No cycles detected" << std::endl;
        std::cout << "  - All required properties present" << std::endl;
    } else {
        std::cout << "❌ VALIDATION FAILED" << std::endl;
        
        if (result.missingParents > 0) {
            std::cout << "\n❌ Missing Parents: " << result.missingParents << std::endl;
            if (verbose || result.missingParentPairs.size() <= 10) {
                for (auto& [child, parent] : result.missingParentPairs) {
                    std::cout << "  - Object " << child << " → parent " << parent << " (not found)" << std::endl;
                }
            } else {
                std::cout << "  (Showing first 10)" << std::endl;
                for (size_t i = 0; i < 10; i++) {
                    auto& [child, parent] = result.missingParentPairs[i];
                    std::cout << "  - Object " << child << " → parent " << parent << " (not found)" << std::endl;
                }
                std::cout << "  ... and " << (result.missingParentPairs.size() - 10) << " more" << std::endl;
            }
        }
        
        if (result.hasCycles) {
            std::cout << "\n❌ Cycle Detected in Parent Graph" << std::endl;
            std::cout << "  Cycle: ";
            for (size_t i = 0; i < result.cycleNodes.size(); i++) {
                std::cout << result.cycleNodes[i];
                if (i < result.cycleNodes.size() - 1) std::cout << " → ";
            }
            std::cout << std::endl;
        }
        
        if (!result.missingRequiredProps.empty()) {
            std::cout << "\n❌ Missing Required Properties" << std::endl;
            for (auto& [typeKey, ids] : result.missingRequiredProps) {
                std::string typeName;
                switch (typeKey) {
                    case 47: typeName = "TrimPath"; break;
                    case 533: typeName = "Feather"; break;
                    case 507: typeName = "Dash"; break;
                    case 506: typeName = "DashPath"; break;
                    case 19: typeName = "GradientStop"; break;
                    default: typeName = "Type" + std::to_string(typeKey); break;
                }
                
                std::cout << "  - " << typeName << " (typeKey " << typeKey << "): " 
                          << ids.size() << " objects" << std::endl;
                
                if (verbose || ids.size() <= 5) {
                    for (uint32_t id : ids) {
                        std::cout << "    • localId " << id << std::endl;
                    }
                } else {
                    for (size_t i = 0; i < 5; i++) {
                        std::cout << "    • localId " << ids[i] << std::endl;
                    }
                    std::cout << "    ... and " << (ids.size() - 5) << " more" << std::endl;
                }
            }
        }
    }
    
    std::cout << "===============================" << std::endl;
}

} // namespace rive_converter
