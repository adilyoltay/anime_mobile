#include "serializer.hpp"
#include "core_builder.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include "rive/file.hpp"
#include "rive/core/field_types/core_color_type.hpp"
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <variant>
#include <cstdint>
#include <iostream>

using namespace rive;

namespace rive_converter
{

// Helper function to write empty asset pack chunk
void writeEmptyAssetPack(rive::VectorBinaryWriter& writer)
{
    // FileAssetContents typeKey = 105 (0x69)
    writer.writeVarUint(static_cast<uint32_t>(105));
    
    // No properties for empty asset pack
    writer.writeVarUint(uint32_t{0}); // End properties
    
    // Asset data length = 0 (no actual assets)
    writer.writeVarUint(uint32_t{0});
    
    // Chunk terminator
    writer.writeVarUint(uint32_t{0});
}

// Helper function to write artboard catalog chunk
void writeArtboardCatalog(rive::VectorBinaryWriter& writer, const CoreDocument& document)
{
    // Special typeKey 8776 (0x2248) - ArtboardListItem
    // This is not in the SDK but required by Rive Play
    writer.writeVarUint(static_cast<uint32_t>(8776));
    
    // Reference to the main artboard
    // Property 3 (id) pointing to artboard
    if (document.artboard) {
        writer.writeVarUint(static_cast<uint32_t>(3)); // id property
        writer.writeVarUint(static_cast<uint32_t>(2)); // artboard is typically ID 2
        writer.writeVarUint(uint32_t{0}); // End properties
    } else {
        writer.writeVarUint(uint32_t{0}); // No properties
    }
    
    // Chunk terminator
    writer.writeVarUint(uint32_t{0});
}

std::vector<uint8_t> serialize_minimal_riv(const Document& doc)
{
    PropertyTypeMap typeMap;
    auto document = build_core_document(doc, typeMap);

    std::vector<uint8_t> buffer;
    VectorBinaryWriter writer(&buffer);

    writer.write(reinterpret_cast<const uint8_t*>("RIVE"), 4);
    writer.writeVarUint(static_cast<uint32_t>(File::majorVersion));
    writer.writeVarUint(static_cast<uint32_t>(File::minorVersion));
    writer.writeVarUint(uint32_t{0}); // file id placeholder

    // Always add id and parentId to the header (importer expects them)
    PropertyTypeMap extendedMap = typeMap;
    extendedMap[3] = rive::CoreUintType::id; // ComponentBase::idPropertyKey
    extendedMap[5] = rive::CoreUintType::id; // ComponentBase::parentIdPropertyKey
    
    
    std::vector<uint16_t> sortedKeys;
    sortedKeys.reserve(extendedMap.size());
    for (const auto& kv : extendedMap)
    {
        sortedKeys.push_back(kv.first);
    }
    std::sort(sortedKeys.begin(), sortedKeys.end());
    for (auto key : sortedKeys)
    {
        writer.writeVarUint(static_cast<uint32_t>(key));
    }
    writer.writeVarUint(uint32_t{0});

    size_t bitmapInts = (sortedKeys.size() + 3) / 4;
    std::vector<uint32_t> bitmap(bitmapInts, 0);
    for (size_t index = 0; index < sortedKeys.size(); ++index)
    {
        auto key = sortedKeys[index];
        auto typeIt = extendedMap.find(key);
        uint8_t typeId = 0;
        if (typeIt != extendedMap.end())
        {
            typeId = typeIt->second;
            // Map CoreBoolType::id (4) to 0 for bitmap encoding
            if (typeId == 4)
            {
                typeId = 0;
            }
        }
        size_t bucket = index / 4;
        size_t shift = (index % 4) * 2;
        bitmap[bucket] |= static_cast<uint32_t>(typeId & 0x3) << shift;
    }
    for (auto value : bitmap)
    {
        writer.write(value);
    }

    for (const auto& object : document.objects)
    {
        writer.writeVarUint(static_cast<uint32_t>(object.core->coreType()));

        writer.writeVarUint(static_cast<uint32_t>(3));
        writer.writeVarUint(object.id);

        if (object.parentId > 0)
        {
            writer.writeVarUint(static_cast<uint32_t>(5)); // ComponentBase::parentIdPropertyKey
            writer.writeVarUint(object.parentId);
        }
        
        // Then write other properties
        for (const auto& prop : object.properties)
        {
            writer.writeVarUint(static_cast<uint32_t>(prop.key));
            auto typeIt = typeMap.find(prop.key);
            uint8_t typeId =
                typeIt != typeMap.end() ? typeIt->second : rive::CoreUintType::id;
            std::visit(
                [&](auto&& value) {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, float>)
                    {
                        writer.writeFloat(value);
                    }
                    else if constexpr (std::is_same_v<T, uint32_t>)
                    {
                        if (typeId == rive::CoreColorType::id)
                        {
                            writer.write(value);
                        }
                        else
                        {
                            writer.writeVarUint(value);
                        }
                    }
                    else if constexpr (std::is_same_v<T, std::string>)
                    {
                        writer.write(value);
                    }
                    else if constexpr (std::is_same_v<T, bool>)
                    {
                        writer.writeVarUint(
                            static_cast<uint32_t>(value ? 1 : 0));
                    }
                },
                prop.value);
        }
        writer.writeVarUint(uint32_t{0});
    }

    writer.writeVarUint(uint32_t{0}); // End main chunk
    
    // Write empty asset pack chunk (required by Rive Play)
    writeEmptyAssetPack(writer);
    
    // Write artboard catalog chunk (required by Rive Play)
    writeArtboardCatalog(writer, document);
    
    // Write multiple terminators (as seen in nature.riv)
    for (int i = 0; i < 4; i++) {
        writer.writeVarUint(uint32_t{0});
    }
    
    return buffer;
}

} // namespace rive_converter
