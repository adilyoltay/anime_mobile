#include "serializer.hpp"
#include "core_builder.hpp"
#include <iostream>

#include "rive/component.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include "rive/file.hpp"
#include "rive/generated/component_base.hpp"
#include "rive/generated/assets/image_asset_base.hpp"
#include "rive/generated/assets/file_asset_base.hpp"
#include "rive/generated/assets/file_asset_contents_base.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_bytes_type.hpp"
#include "rive/core/field_types/core_color_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

using namespace rive;

namespace rive_converter
{
namespace
{
constexpr uint16_t kParentIdKey = rive::ComponentBase::parentIdPropertyKey;
constexpr uint16_t kComponentIdKey = 3;
constexpr uint16_t kFileAssetIdKey = 204;
constexpr uint16_t kFileAssetBytesKey = 212;
constexpr uint16_t kArtboardListTypeKey = 8726;
constexpr uint16_t kArtboardListItemTypeKey = 8776;

int fieldIdForKey(uint16_t key, const PropertyTypeMap& fallback)
{
    auto it = fallback.find(key);
    if (it != fallback.end())
    {
        return it->second;
    }
    return CoreUintType::id;
}

uint8_t headerTypeCodeFor(int fieldId)
{
    switch (fieldId)
    {
        case CoreStringType::id: // Also handles CoreBytesType (same id = 1)
            return 1; // string/bytes (length + data)
        case CoreDoubleType::id:
            return 2; // double/float
        case CoreColorType::id:
            return 3; // color (32-bit)
        case CoreBoolType::id:
            return 0; // encode as uint (varuint 0/1)
        default:
            return 0; // uint / fallback
    }
}

void writeProperty(VectorBinaryWriter& writer,
                   uint16_t key,
                   const Property& property,
                   int fieldId)
{
    writer.writeVarUint(static_cast<uint32_t>(key));

    switch (fieldId)
    {
        case CoreDoubleType::id:
        {
            float value = 0.0f;
            if (auto p = std::get_if<float>(&property.value))
            {
                value = *p;
            }
            else if (auto p = std::get_if<uint32_t>(&property.value))
            {
                value = static_cast<float>(*p);
            }
            writer.writeFloat(value);
            break;
        }
        case CoreColorType::id:
        {
            uint32_t value = 0;
            if (auto p = std::get_if<uint32_t>(&property.value))
            {
                value = *p;
            }
            writer.write(value);
            break;
        }
        case CoreStringType::id:
        {
            std::string value;
            if (auto p = std::get_if<std::string>(&property.value))
            {
                value = *p;
            }
            writer.write(std::move(value));
            break;
        }
        case CoreBoolType::id:
        {
            bool value = false;
            if (auto p = std::get_if<bool>(&property.value))
            {
                value = *p;
            }
            writer.writeVarUint(static_cast<uint32_t>(value ? 1u : 0u));
            break;
        }
        // Note: CoreBytesType has same id as CoreStringType (id=1), handled above
        default:
        {
            if (auto p = std::get_if<uint32_t>(&property.value))
            {
                writer.writeVarUint(*p);
            }
            else if (auto p = std::get_if<bool>(&property.value))
            {
                writer.writeVarUint(static_cast<uint32_t>(*p ? 1u : 0u));
            }
            else if (auto p = std::get_if<float>(&property.value))
            {
                writer.writeFloat(*p);
            }
            else if (auto p = std::get_if<std::string>(&property.value))
            {
                writer.write(*p);
            }
            else if (auto p = std::get_if<std::vector<uint8_t>>(&property.value))
            {
                // Write bytes: length + raw data (same as string)
                writer.writeVarUint(static_cast<uint32_t>(p->size()));
                if (!p->empty())
                {
                    writer.write(p->data(), p->size());
                }
            }
            else
            {
                writer.writeVarUint(0u);
            }
            break;
        }
    }
}
} // namespace

std::vector<uint8_t> serialize_minimal_riv(const Document& doc)
{
    PropertyTypeMap typeMap;
    auto document = build_core_document(doc, typeMap);

    std::unordered_set<uint16_t> headerSet;
    for (const auto& object : document.objects)
    {
        for (const auto& property : object.properties)
        {
            headerSet.insert(property.key);
        }
    }
    
    // Key 212 (FileAssetContents::bytes) is now added as normal object property,
    // so it will be in headerSet automatically

    bool needsParentKey = false;
    bool needsIdKey = false;
    for (const auto& object : document.objects)
    {
        if (object.core->is<rive::Component>())
        {
            needsIdKey = true;
            if (object.parentId != 0)
            {
                needsParentKey = true;
            }
        }
    }
    if (needsIdKey)
    {
        headerSet.insert(kComponentIdKey);
        typeMap[kComponentIdKey] = rive::CoreUintType::id;
    }
    if (needsParentKey)
    {
        headerSet.insert(kParentIdKey);
        typeMap[kParentIdKey] = rive::CoreUintType::id;
    }

    // Key 212 (bytes) already in typeMap from core_builder if we have font data
    // No need to manually add to headerSet anymore

    std::vector<uint16_t> headerKeys(headerSet.begin(), headerSet.end());
    std::sort(headerKeys.begin(), headerKeys.end());

    std::vector<uint8_t> buffer;
    buffer.reserve(16384); // Reserve 16KB for large files
    VectorBinaryWriter writer(&buffer);

    // File header
    writer.write(reinterpret_cast<const uint8_t*>("RIVE"), 4);
    writer.writeVarUint(static_cast<uint32_t>(File::majorVersion));
    writer.writeVarUint(static_cast<uint32_t>(File::minorVersion));
    writer.writeVarUint(uint32_t{0}); // file id placeholder

    for (auto key : headerKeys)
    {
        writer.writeVarUint(static_cast<uint32_t>(key));
    }
    writer.writeVarUint(uint32_t{0});

    const size_t bitmapCount = (headerKeys.size() + 3) / 4;
    std::vector<uint32_t> bitmap(bitmapCount, 0u);
    for (size_t index = 0; index < headerKeys.size(); ++index)
    {
        int fieldId = fieldIdForKey(headerKeys[index], typeMap);
        uint8_t code = static_cast<uint8_t>(headerTypeCodeFor(fieldId) & 0x3);
        size_t bucket = index / 4;
        size_t shift = (index % 4) * 2;
        bitmap[bucket] |= static_cast<uint32_t>(code) << shift;
    }
    for (auto value : bitmap)
    {
        writer.write(value);
    }

    bool assetPreludeWritten = false; // Will be set to true after FontAsset
    std::unordered_map<uint32_t, uint32_t> localComponentIndex;
    uint32_t nextLocalIndex = 0;

    for (const auto& object : document.objects)
    {
        writer.writeVarUint(static_cast<uint32_t>(object.core->coreType()));

        if (object.core->is<rive::Artboard>())
        {
            localComponentIndex.clear();
            localComponentIndex.emplace(object.id, 0);
            nextLocalIndex = 1;
        }

        if (object.core->is<rive::Component>())
        {
            uint32_t localId = 0;
            auto selfIt = localComponentIndex.find(object.id);
            if (selfIt == localComponentIndex.end())
            {
                localId = nextLocalIndex++;
                localComponentIndex.emplace(object.id, localId);
            }
            else
            {
                localId = selfIt->second;
            }

            writer.writeVarUint(static_cast<uint32_t>(kComponentIdKey));
            writer.writeVarUint(localId);

            if (object.parentId != 0)
            {
                auto parentIt = localComponentIndex.find(object.parentId);
                if (parentIt == localComponentIndex.end())
                {
                    parentIt = localComponentIndex
                                   .emplace(object.parentId, nextLocalIndex++)
                                   .first;
                }
                writer.writeVarUint(static_cast<uint32_t>(kParentIdKey));
                writer.writeVarUint(parentIt->second);
            }
        }

        std::vector<Property> properties = object.properties;
        std::sort(properties.begin(), properties.end(),
                  [](const Property& a, const Property& b) {
                      return a.key < b.key;
                  });

        for (const auto& property : properties)
        {
            // Special handling for component reference properties - remap to artboard-local indices
            if (property.key == 51 ||   // KeyedObject::objectId (animation references)
                property.key == 92 ||   // ClippingShape::sourceId (clipping references)
                property.key == 272)    // TextValueRun::styleId (text style references)
            {
                if (auto p = std::get_if<uint32_t>(&property.value))
                {
                    uint32_t globalId = *p;
                    auto localIt = localComponentIndex.find(globalId);
                    if (localIt != localComponentIndex.end())
                    {
                        writer.writeVarUint(static_cast<uint32_t>(property.key)); // key
                        writer.writeVarUint(localIt->second); // artboard-local index
                        continue; // Skip normal writeProperty
                    }
                }
            }
            
            int fieldId = fieldIdForKey(property.key, typeMap);
            writeProperty(writer, property.key, property, fieldId);
        }

        writer.writeVarUint(uint32_t{0});
        
        // Write FileAssetContents immediately after FontAsset (typeKey 141)
        if (!assetPreludeWritten && object.core->coreType() == 141) // FontAsset
        {
            if (!document.fontData.empty())
            {
                writer.writeVarUint(static_cast<uint32_t>(rive::FileAssetContentsBase::typeKey)); // 106
                writer.writeVarUint(static_cast<uint32_t>(kFileAssetBytesKey)); // 212
                writer.writeVarUint(static_cast<uint32_t>(document.fontData.size()));
                writer.write(document.fontData.data(), document.fontData.size());
                writer.writeVarUint(uint32_t{0}); // property terminator
                assetPreludeWritten = true;
            }
        }
    }

    // NOTE: NO end-of-stream marker! Runtime expects next chunk immediately.
    // Writing a 0 here causes "Malformed file" error (invalid type key).
    
    return buffer;
}

// Serialize CoreDocument directly (for universal builder)
std::vector<uint8_t> serialize_core_document(const CoreDocument& document, PropertyTypeMap& typeMap)
{
    std::unordered_set<uint16_t> headerSet;
    for (const auto& object : document.objects)
    {
        for (const auto& property : object.properties)
        {
            headerSet.insert(property.key);
        }
    }

    bool needsParentKey = false;
    bool needsIdKey = false;
    for (const auto& object : document.objects)
    {
        if (object.core->is<rive::Component>())
        {
            needsIdKey = true;
            if (object.parentId != 0)
            {
                needsParentKey = true;
            }
        }
    }
    if (needsIdKey)
    {
        headerSet.insert(kComponentIdKey);
        typeMap[kComponentIdKey] = rive::CoreUintType::id;
    }
    if (needsParentKey)
    {
        headerSet.insert(kParentIdKey);
        typeMap[kParentIdKey] = rive::CoreUintType::id;
    }

    std::vector<uint16_t> headerKeys(headerSet.begin(), headerSet.end());
    std::sort(headerKeys.begin(), headerKeys.end());

    std::vector<uint8_t> buffer;
    buffer.reserve(16384); // Reserve 16KB for large files
    VectorBinaryWriter writer(&buffer);

    // File header
    writer.write(reinterpret_cast<const uint8_t*>("RIVE"), 4);
    writer.writeVarUint(static_cast<uint32_t>(rive::File::majorVersion));
    writer.writeVarUint(static_cast<uint32_t>(rive::File::minorVersion));
    writer.writeVarUint(uint32_t{0}); // file id

    for (auto key : headerKeys)
    {
        writer.writeVarUint(static_cast<uint32_t>(key));
    }
    writer.writeVarUint(uint32_t{0});

    const size_t bitmapCount = (headerKeys.size() + 3) / 4;
    std::vector<uint32_t> bitmap(bitmapCount, 0u);
    for (size_t index = 0; index < headerKeys.size(); ++index)
    {
        int fieldId = fieldIdForKey(headerKeys[index], typeMap);
        uint8_t code = static_cast<uint8_t>(headerTypeCodeFor(fieldId) & 0x3);
        size_t bucket = index / 4;
        size_t shift = (index % 4) * 2;
        bitmap[bucket] |= static_cast<uint32_t>(code) << shift;
    }
    for (auto value : bitmap)
    {
        writer.write(value);
    }

    bool assetPreludeWritten = false;
    std::unordered_map<uint32_t, uint32_t> localComponentIndex;
    uint32_t nextLocalIndex = 0;

    for (const auto& object : document.objects)
    {
        writer.writeVarUint(static_cast<uint32_t>(object.core->coreType()));

        if (object.core->is<rive::Artboard>())
        {
            localComponentIndex.clear();
            localComponentIndex.emplace(object.id, 0);
            nextLocalIndex = 1;
        }

        if (object.core->is<rive::Component>())
        {
            uint32_t localId = 0;
            auto selfIt = localComponentIndex.find(object.id);
            if (selfIt == localComponentIndex.end())
            {
                localId = nextLocalIndex++;
                localComponentIndex.emplace(object.id, localId);
            }
            else
            {
                localId = selfIt->second;
            }

            writer.writeVarUint(static_cast<uint32_t>(kComponentIdKey));
            writer.writeVarUint(localId);

            if (object.parentId != 0)
            {
                auto parentIt = localComponentIndex.find(object.parentId);
                if (parentIt == localComponentIndex.end())
                {
                    parentIt = localComponentIndex
                                   .emplace(object.parentId, nextLocalIndex++)
                                   .first;
                }
                writer.writeVarUint(static_cast<uint32_t>(kParentIdKey));
                writer.writeVarUint(parentIt->second);
            }
        }

        std::vector<Property> properties = object.properties;
        std::sort(properties.begin(), properties.end(),
                  [](const Property& a, const Property& b) {
                      return a.key < b.key;
                  });

        for (const auto& property : properties)
        {
            if (property.key == 51 || property.key == 92 || property.key == 272)
            {
                if (auto p = std::get_if<uint32_t>(&property.value))
                {
                    uint32_t globalId = *p;
                    auto localIt = localComponentIndex.find(globalId);
                    if (localIt != localComponentIndex.end())
                    {
                        writer.writeVarUint(static_cast<uint32_t>(property.key));
                        writer.writeVarUint(localIt->second);
                        continue;
                    }
                }
            }
            
            int fieldId = fieldIdForKey(property.key, typeMap);
            writeProperty(writer, property.key, property, fieldId);
        }

        writer.writeVarUint(uint32_t{0});
        
        if (!assetPreludeWritten && object.core->coreType() == 141)
        {
            if (!document.fontData.empty())
            {
                writer.writeVarUint(static_cast<uint32_t>(rive::FileAssetContentsBase::typeKey));
                writer.writeVarUint(static_cast<uint32_t>(kFileAssetBytesKey));
                writer.writeVarUint(static_cast<uint32_t>(document.fontData.size()));
                writer.write(document.fontData.data(), document.fontData.size());
                writer.writeVarUint(uint32_t{0});
                assetPreludeWritten = true;
            }
        }
    }

    // NOTE: NO end-of-stream marker! Runtime expects next chunk immediately.
    // Writing a 0 here causes "Malformed file" error (invalid type key).
    
    return buffer;
}

} // namespace rive_converter
