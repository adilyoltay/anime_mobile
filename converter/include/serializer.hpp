#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "json_loader.hpp"
#include "core_builder.hpp"

namespace rive_converter
{
std::vector<uint8_t> serialize_minimal_riv(const Document& document);
std::vector<uint8_t> serialize_core_document(const CoreDocument& document, PropertyTypeMap& typeMap);
}

