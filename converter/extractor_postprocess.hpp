#pragma once

#include <nlohmann/json.hpp>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <iostream>

using json = nlohmann::json;

namespace extractor_postprocess
{

struct DiagnosticCounters
{
    int missingParents = 0;
    int droppedObjects = 0;
    std::map<uint16_t, int> defaultsInjected; // typeKey -> count
    int reorderedObjects = 0;
    int skippedTrimPath = 0; // PR-Extractor-SkipTrimPath
};

// Inject required default properties for effect types
inline void injectRequiredDefaults(json& objJson, DiagnosticCounters& diag)
{
    if (!objJson.contains("typeKey")) return;
    
    uint16_t typeKey = objJson["typeKey"];
    auto& props = objJson["properties"];
    
    bool injected = false;
    
    switch (typeKey) {
        case 47: // TrimPath (PR-TrimPath-Compat: normalized range 0-1)
            if (!props.contains("start")) { props["start"] = 0.0; injected = true; }
            if (!props.contains("end")) { props["end"] = 1.0; injected = true; } // Was 0.0, now 1.0 (normalized)
            if (!props.contains("offset")) { props["offset"] = 0.0; injected = true; }
            if (!props.contains("modeValue")) { props["modeValue"] = 0; injected = true; }
            break;
            
        case 49: // Feather (typeKey 533 is old, 49 is correct)
        case 533: // Feather (legacy)
            if (!props.contains("strength")) { props["strength"] = 0.0; injected = true; }
            if (!props.contains("offsetX")) { props["offsetX"] = 0.0; injected = true; }
            if (!props.contains("offsetY")) { props["offsetY"] = 0.0; injected = true; }
            if (!props.contains("inner")) { props["inner"] = false; injected = true; }
            break;
            
        case 48: // Dash
            if (!props.contains("length")) { props["length"] = 0.0; injected = true; }
            if (!props.contains("lengthIsPercentage")) { props["lengthIsPercentage"] = false; injected = true; }
            break;
            
        case 46: // DashPath
            if (!props.contains("offset")) { props["offset"] = 0.0; injected = true; }
            if (!props.contains("offsetIsPercentage")) { props["offsetIsPercentage"] = false; injected = true; }
            break;
            
        case 19: // GradientStop
            if (!props.contains("colorValue")) { props["colorValue"] = 4278190080u; injected = true; } // 0xFF000000
            if (!props.contains("position")) { props["position"] = 0.0; injected = true; }
            break;
    }
    
    if (injected) {
        diag.defaultsInjected[typeKey]++;
    }
}

// Topological sort: parents before children
inline std::vector<json> topologicalSort(const std::vector<json>& objects, DiagnosticCounters& diag)
{
    // Build parent-child relationships
    std::unordered_map<uint32_t, std::vector<uint32_t>> parentToChildren;
    std::unordered_map<uint32_t, uint32_t> childToParent;
    std::unordered_map<uint32_t, size_t> localIdToIndex;
    std::unordered_set<uint32_t> allLocalIds;
    
    for (size_t i = 0; i < objects.size(); i++) {
        if (!objects[i].contains("localId")) continue;
        uint32_t localId = objects[i]["localId"];
        allLocalIds.insert(localId);
        localIdToIndex[localId] = i;
        
        if (objects[i].contains("parentId")) {
            uint32_t parentId = objects[i]["parentId"];
            childToParent[localId] = parentId;
            parentToChildren[parentId].push_back(localId);
        }
    }
    
    // Check for missing parents and count forward references
    int forwardRefCount = 0;
    for (const auto& [child, parent] : childToParent) {
        if (allLocalIds.find(parent) == allLocalIds.end()) {
            diag.missingParents++;
            std::cerr << "⚠️  Missing parent: object " << child << " → parent " << parent << " (not found)" << std::endl;
        }
    }
    
    // Kahn's algorithm for topological sort
    std::vector<json> sorted;
    std::unordered_map<uint32_t, int> inDegree;
    
    // Calculate in-degrees (number of parents)
    for (uint32_t id : allLocalIds) {
        inDegree[id] = (childToParent.count(id) > 0) ? 1 : 0;
    }
    
    // Queue of nodes with no incoming edges (roots)
    std::vector<uint32_t> queue;
    for (uint32_t id : allLocalIds) {
        if (inDegree[id] == 0) {
            queue.push_back(id);
        }
    }
    
    // Process queue
    std::unordered_set<uint32_t> visited;
    while (!queue.empty()) {
        uint32_t current = queue.back();
        queue.pop_back();
        
        if (visited.count(current)) continue;
        visited.insert(current);
        
        // Add to sorted output
        size_t idx = localIdToIndex[current];
        sorted.push_back(objects[idx]);
        
        // Process children
        if (parentToChildren.count(current)) {
            for (uint32_t child : parentToChildren[current]) {
                inDegree[child]--;
                if (inDegree[child] == 0) {
                    queue.push_back(child);
                }
            }
        }
    }
    
    // Add any remaining objects (those without localId or non-components)
    for (const auto& obj : objects) {
        if (!obj.contains("localId")) {
            sorted.push_back(obj);
        }
    }
    
    // Check if all objects with localId were processed
    if (sorted.size() < objects.size()) {
        diag.droppedObjects = objects.size() - sorted.size();
        std::cerr << "⚠️  Dropped " << diag.droppedObjects << " objects (possibly due to cycles or missing parents)" << std::endl;
    }
    
    diag.reorderedObjects = sorted.size();
    return sorted;
}

// Parent sanity check: Drop objects with missing parents or invalid parent types
inline void checkParentSanity(std::vector<json>& objects, DiagnosticCounters& diag)
{
    // Build localId -> typeKey map
    std::unordered_map<uint32_t, uint16_t> localIdToType;
    std::unordered_set<uint32_t> allLocalIds;
    for (const auto& obj : objects) {
        if (obj.contains("localId") && obj.contains("typeKey")) {
            uint32_t localId = obj["localId"];
            allLocalIds.insert(localId);
            localIdToType[localId] = obj["typeKey"];
        }
    }
    
    // Filter objects with valid parents
    std::vector<json> filtered;
    for (auto& obj : objects) {
        if (!obj.contains("typeKey")) {
            filtered.push_back(obj);
            continue;
        }
        
        uint16_t typeKey = obj["typeKey"];
        bool shouldDrop = false;
        
        // PR-TrimPath-Compat: Attempted end=1.0 but still MALFORMED
        // Keeping TrimPath skip until deeper investigation
        if (typeKey == 47) { // TrimPath
            diag.skippedTrimPath++;
            diag.droppedObjects++;
            continue; // Skip - needs deeper runtime investigation
        }
        
        // Ensure ClippingShape objects are preserved. Previous debug-only logic
        // skipped typeKey 42 and caused malformed masking and grey screen.
        
        // Check if parent exists (for ALL objects, not just TrimPath)
        if (obj.contains("parentId")) {
            uint32_t parentId = obj["parentId"];
            
            // Special case: parentId=0 is always valid (Artboard)
            if (parentId != 0 && allLocalIds.find(parentId) == allLocalIds.end()) {
                std::cerr << "⚠️  Dropping object typeKey=" << typeKey 
                          << " localId=" << obj.value("localId", 0u)
                          << " (missing parent " << parentId << ")" << std::endl;
                diag.droppedObjects++;
                shouldDrop = true;
            }
        }
        
        if (!shouldDrop) {
            filtered.push_back(obj);
        }
    }
    
    objects = filtered;
}

// Main post-processing function
inline json postProcessArtboard(const json& artboardJson, DiagnosticCounters& diag)
{
    json processed = artboardJson;
    
    if (!processed.contains("objects") || !processed["objects"].is_array()) {
        return processed;
    }
    
    // PR-RivePlay-Fix: Filter out 0x0 artboards (causes grey screen in Play)
    if (processed.contains("width") && processed.contains("height")) {
        double width = processed.value("width", 0.0);
        double height = processed.value("height", 0.0);
        
        if (width == 0.0 && height == 0.0) {
            std::cerr << "⚠️  Filtering 0x0 artboard '" << processed.value("name", "unnamed") 
                      << "' (causes selection issues in Rive Play)" << std::endl;
            processed["objects"] = json::array(); // Empty - will be filtered by caller
            return processed;
        }
    }
    
    // Convert objects array to vector for processing
    std::vector<json> objects;
    for (const auto& obj : processed["objects"]) {
        objects.push_back(obj);
    }
    
    std::cout << "  Post-processing: " << objects.size() << " objects..." << std::endl;
    
    // Step 1: Inject required defaults
    for (auto& obj : objects) {
        injectRequiredDefaults(obj, diag);
    }
    
    // Step 2: Topological sort (parents before children)
    objects = topologicalSort(objects, diag);
    
    // Step 3: Parent sanity checks (drop invalid objects)
    checkParentSanity(objects, diag);
    
    // Convert back to JSON array
    processed["objects"] = json::array();
    for (const auto& obj : objects) {
        processed["objects"].push_back(obj);
    }
    
    std::cout << "  Post-processing complete: " << processed["objects"].size() << " objects" << std::endl;
    
    return processed;
}

// Print diagnostic summary
inline void printDiagnostics(const DiagnosticCounters& diag)
{
    if (diag.missingParents > 0 || diag.droppedObjects > 0 || !diag.defaultsInjected.empty()) {
        std::cout << "\n📊 Extraction Diagnostics:" << std::endl;
        
        if (diag.missingParents > 0) {
            std::cout << "  ⚠️  Missing parent references: " << diag.missingParents << std::endl;
        }
        
        if (!diag.defaultsInjected.empty()) {
            std::cout << "  ℹ️  Defaults injected:" << std::endl;
            for (const auto& [typeKey, count] : diag.defaultsInjected) {
                std::string typeName;
                switch (typeKey) {
                    case 47: typeName = "TrimPath"; break;
                    case 49: typeName = "Feather"; break;
                    case 533: typeName = "Feather"; break;
                    case 48: typeName = "Dash"; break;
                    case 46: typeName = "DashPath"; break;
                    case 19: typeName = "GradientStop"; break;
                    default: typeName = "Type" + std::to_string(typeKey); break;
                }
                std::cout << "    - " << typeName << " (" << typeKey << "): " << count << " objects" << std::endl;
            }
        }
        
        if (diag.skippedTrimPath > 0) {
            std::cout << "  🚫 TrimPath skipped: " << diag.skippedTrimPath << " (runtime compatibility - needs investigation)" << std::endl;
        }
        
        if (diag.droppedObjects > diag.skippedTrimPath) {
            std::cout << "  🧹 Other objects dropped: " << (diag.droppedObjects - diag.skippedTrimPath) << " (invalid parents/sanity checks)" << std::endl;
        }
        
        std::cout << "  ✅ Objects reordered: " << diag.reorderedObjects << " (topological sort)" << std::endl;
    } else {
        std::cout << "\n✅ No post-processing issues detected" << std::endl;
    }
}

} // namespace extractor_postprocess
