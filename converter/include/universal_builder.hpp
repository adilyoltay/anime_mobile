#pragma once
#include "core_builder.hpp"
#include <nlohmann/json.hpp>

namespace rive_converter
{
    // Build CoreDocument from universal JSON format (objects array with typeKey + properties)
    // Returns CoreDocument with populated typeMap
    CoreDocument build_from_universal_json(const nlohmann::json& data, PropertyTypeMap& outTypeMap);
    
} // namespace rive_converter

