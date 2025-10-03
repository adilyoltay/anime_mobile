#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "json_loader.hpp"
#include "core_builder.hpp"
#include <nlohmann/json.hpp>

namespace rive_converter
{
std::vector<uint8_t> serialize_minimal_riv(const Document& document);
std::vector<uint8_t> serialize_core_document(const CoreDocument& document, PropertyTypeMap& typeMap);
std::vector<uint8_t> serialize_exact_riv_json(const nlohmann::json& data);
}
