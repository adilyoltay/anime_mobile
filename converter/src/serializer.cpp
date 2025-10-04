#include "serializer.hpp"
#include "core_builder.hpp"
#include "serializer_diagnostics.hpp"
#include <iostream>
#include <cctype>
#include <cstring>
#include <nlohmann/json.hpp>
#include <stdexcept>

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
std::vector<uint8_t> raw_decode_base64(const std::string& input)
{
    auto decode_char = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };

    std::vector<uint8_t> out;
    int val = 0;
    int bits = -8;
    for (unsigned char ch : input)
    {
        if (std::isspace(ch))
        {
            continue;
        }
        if (ch == '=')
        {
            break;
        }
        int decoded = decode_char(static_cast<char>(ch));
        if (decoded < 0)
        {
            continue;
        }
        val = (val << 6) | decoded;
        bits += 6;
        if (bits >= 0)
        {
            out.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

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
            if (auto p = std::get_if<std::string>(&property.value))
            {
                writer.write(*p);
            }
            else if (auto p = std::get_if<std::vector<uint8_t>>(&property.value))
            {
                writer.writeVarUint(static_cast<uint32_t>(p->size()));
                if (!p->empty())
                {
                    writer.write(p->data(), p->size());
                }
            }
            else
            {
                writer.write(std::string());
            }
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
        if (object.isComponent)
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
    
    // PR1 Extended: Add bytes key (212) and assetId key (204) for asset placeholder/font
    // Since we write these in the serializer (not builder), must add to header manually
    headerSet.insert(kFileAssetBytesKey); // 212
    typeMap[kFileAssetBytesKey] = rive::CoreStringType::id; // bytes type (same as string)
    headerSet.insert(kFileAssetIdKey);   // 204
    typeMap[kFileAssetIdKey] = rive::CoreUintType::id;

    std::vector<uint16_t> headerKeys(headerSet.begin(), headerSet.end());
    std::sort(headerKeys.begin(), headerKeys.end());

    std::vector<uint8_t> buffer;
    buffer.reserve(16384); // Reserve 16KB for large files
    VectorBinaryWriter writer(&buffer);

    SerializerDiagnostics diag;
    if (diag.isEnabled())
    {
        diag.info("Starting RIV serialization (serialize_minimal_riv)");
        diag.logCount("Objects", document.objects.size());
        diag.logCount("Header keys", headerKeys.size());
    }

    // File header
    diag.beginChunk("HEADER", buffer.size());
    writer.write(reinterpret_cast<const uint8_t*>("RIVE"), 4);
    writer.writeVarUint(static_cast<uint32_t>(File::majorVersion));
    writer.writeVarUint(static_cast<uint32_t>(File::minorVersion));
    writer.writeVarUint(uint32_t{0}); // file id placeholder
    diag.endChunk("HEADER", buffer.size());

    // Table of Contents
    diag.beginChunk("TOC", buffer.size(), headerKeys.size() * 2);  // ~2 bytes per key
    for (auto key : headerKeys)
    {
        writer.writeVarUint(static_cast<uint32_t>(key));
    }
    writer.writeVarUint(uint32_t{0});
    diag.endChunk("TOC", buffer.size());

    // NOTE: No padding between ToC and bitmap
    // RuntimeHeader::read() expects bitmap to start immediately after 0 terminator
    // See include/rive/runtime_header.hpp:87-93 - readUint32() is called right after ToC loop

    // Bitmap
    diag.beginChunk("BITMAP", buffer.size());
    diag.checkAlignment("Bitmap", buffer.size(), 4);
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
    diag.endChunk("BITMAP", buffer.size());

    // Objects
    diag.beginChunk("OBJECTS", buffer.size());
    diag.logCount("Object count", document.objects.size());

    bool placeholderEmitted = false;  // PR1: Separate flags
    bool fontBytesEmitted = false;
    std::unordered_map<uint32_t, uint32_t> localComponentIndex;
    uint32_t nextLocalIndex = 0;
    // PR2c: track all property keys written to the stream
    std::unordered_set<uint16_t> streamPropKeys;
    
    size_t objIndex = 0;
    for (const auto& object : document.objects)
    {
        writer.writeVarUint(static_cast<uint32_t>(object.typeKey));

        if (object.isArtboard)
        {
            localComponentIndex.clear();
            localComponentIndex.emplace(object.id, 0);
            nextLocalIndex = 1;
        }

        if (object.isComponent)
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

        // PR2b: Track remap misses for diagnostic
        static std::map<uint16_t, int> remapMissCount;
        static bool firstRun = true;
        
        for (const auto& property : properties)
        {
            // PR2c: HEADER_MISS check
            if (headerSet.find(property.key) == headerSet.end())
            {
                std::cerr << "HEADER_MISS key=" << property.key
                          << " typeKey=" << object.typeKey << std::endl;
                // Skip writing this property in debug diagnostics
                continue;
            }

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
                    else
                    {
                        // PR2b: Mapping not found - SKIP this property entirely (don't write raw ID)
                        // This prevents out-of-range index errors in importer
                        remapMissCount[property.key]++;
                        if (remapMissCount[property.key] <= 10)
                        {
                            std::cerr << "⚠️  PR2b remap-miss: key=" << property.key 
                                      << " globalId=" << globalId << " — skipping property" << std::endl;
                        }
                        continue; // CRITICAL: Skip writing this property
                    }
                }
            }
            
            int fieldId = fieldIdForKey(property.key, typeMap);

            // PR2c: TYPE_MISMATCH check
            uint8_t headerCode = static_cast<uint8_t>(headerTypeCodeFor(fieldId) & 0x3);
            // Note: Color/Bytes handled via fieldId mapping; we only sanity check primitive mismatches
            // Log if primitive mismatch (approximate)
            if ((headerCode == 0 && (std::holds_alternative<float>(property.value) || std::holds_alternative<std::string>(property.value))) ||
                (headerCode == 1 && !std::holds_alternative<float>(property.value)) ||
                (headerCode == 2 && !std::holds_alternative<std::string>(property.value)))
            {
                std::cerr << "TYPE_MISMATCH key=" << property.key
                          << " code=" << int(headerCode)
                          << " actual=" << (std::holds_alternative<float>(property.value) ? "Double" : std::holds_alternative<std::string>(property.value) ? "String" : "Uint/Bool")
                          << " typeKey=" << object.typeKey << std::endl;
                // Continue to write to keep behavior but log
            }

            writeProperty(writer, property.key, property, fieldId);
            streamPropKeys.insert(property.key);
        }
        
        // PR2b: Print summary at end of first artboard
        if (firstRun && !remapMissCount.empty())
        {
            std::cerr << "\n=== PR2b REMAP MISS SUMMARY ===" << std::endl;
            for (const auto& [key, count] : remapMissCount)
            {
                std::cerr << "  key " << key << ": " << count << " misses" << std::endl;
            }
            std::cerr << "================================\n" << std::endl;
            firstRun = false;
        }

        writer.writeVarUint(uint32_t{0}); // Property terminator
        
        // PR1: Asset placeholder AFTER Backboard properties complete (if no font)
        if (objIndex == 0 && !fontBytesEmitted && !placeholderEmitted && document.fontData.empty())
        {
            placeholderEmitted = true;
            // Write ImageAsset (105) placeholder object
            writer.writeVarUint(static_cast<uint32_t>(rive::ImageAssetBase::typeKey)); // 105
            writer.writeVarUint(static_cast<uint32_t>(kFileAssetIdKey)); // 204
            writer.writeVarUint(static_cast<uint32_t>(0));   // assetId = 0
            writer.writeVarUint(static_cast<uint32_t>(0));   // property terminator

            // Write FileAssetContents (106) with empty bytes (212) as independent object
            writer.writeVarUint(static_cast<uint32_t>(rive::FileAssetContentsBase::typeKey)); // 106
            writer.writeVarUint(static_cast<uint32_t>(kFileAssetBytesKey)); // 212
            writer.writeVarUint(static_cast<uint32_t>(0));   // length = 0 (empty)
            writer.writeVarUint(static_cast<uint32_t>(0));   // property terminator
            
            std::cout << "  ℹ️  Asset placeholder after Backboard (no font embedded)" << std::endl;
        }
        
        // PR1: Real font bytes AFTER FontAsset (141) properties complete
        if (!fontBytesEmitted && object.typeKey == 141) // FontAsset
        {
            if (!document.fontData.empty())
            {
                fontBytesEmitted = true;
                
                // Write FileAssetContents (106) as independent object
                writer.writeVarUint(static_cast<uint32_t>(rive::FileAssetContentsBase::typeKey)); // 106
                writer.writeVarUint(static_cast<uint32_t>(kFileAssetBytesKey)); // 212
                writer.writeVarUint(static_cast<uint32_t>(document.fontData.size()));
                writer.write(document.fontData.data(), document.fontData.size());
                writer.writeVarUint(uint32_t{0}); // property terminator
                
                std::cout << "  ℹ️  Font bytes written after FontAsset (" << document.fontData.size() << " bytes)" << std::endl;
            }
        }
        
        objIndex++;
    }

    // PR2c: Print header/stream diff
    if (true) {
        std::unordered_set<uint16_t> missingInHeader;
        for (auto k : streamPropKeys) if (headerSet.find(k) == headerSet.end()) missingInHeader.insert(k);
        std::unordered_set<uint16_t> extraInHeader;
        for (auto k : headerSet) if (streamPropKeys.find(k) == streamPropKeys.end()) extraInHeader.insert(k);
        if (!missingInHeader.empty() || !extraInHeader.empty()) {
            std::cerr << "\n=== PR2c HEADER/STREAM DIFF (minimal) ===" << std::endl;
            if (!missingInHeader.empty()) {
                std::cerr << "MissingInHeader:";
                for (auto k : missingInHeader) std::cerr << " " << k;
                std::cerr << std::endl;
            }
            if (!extraInHeader.empty()) {
                std::cerr << "ExtraInHeader:";
                for (auto k : extraInHeader) std::cerr << " " << k;
                std::cerr << std::endl;
            }
            std::cerr << "=======================================\n" << std::endl;
        }
    }

    // End object stream with terminator
    writer.writeVarUint(static_cast<uint32_t>(0)); // Object stream terminator
    diag.endChunk("OBJECTS", buffer.size());
    
    diag.printSummary();
    
    // PR-RivePlay-Catalog: Write Artboard Catalog chunk for proper artboard selection
    // This must come AFTER object stream terminator, as a separate chunk
    std::cout << "\n  ℹ️  Writing Artboard Catalog chunk (minimal serializer)..." << std::endl;
    
    // Collect artboard IDs from document
    // Note: 0x0 artboards are already filtered by universal_builder, so all artboards here are valid
    std::vector<uint32_t> artboardIds;
    for (const auto& object : document.objects) {
        if (object.isArtboard) {
            artboardIds.push_back(object.id);
            std::cout << "    - Artboard id: " << object.id << std::endl;
        }
    }
    
    // DISABLED: ArtboardCatalog causes "Unknown property key" warnings in Rive Play
    // The catalog chunk uses type keys 8726/8776 which are not in the ToC,
    // causing Rive Play to stop creating drawable objects → grey screen
    // TODO: Either add 8726/8776 to ToC or use official runtime encoding
    /*
    if (!artboardIds.empty()) {
        // Write ArtboardList (8726) wrapper
        writer.writeVarUint(static_cast<uint32_t>(8726)); // ArtboardList typeKey
        writer.writeVarUint(static_cast<uint32_t>(0));    // No properties, terminate immediately
        
        // Write ArtboardListItem (8776) for each non-empty artboard
        for (uint32_t artboardId : artboardIds) {
            writer.writeVarUint(static_cast<uint32_t>(8776)); // ArtboardListItem typeKey
            writer.writeVarUint(static_cast<uint32_t>(3));    // id property key (component id)
            writer.writeVarUint(artboardId);                   // artboard's ID
            writer.writeVarUint(static_cast<uint32_t>(0));    // Property terminator
        }
    }
    */
    
    std::cout << "  ✅ Artboard Catalog written (" << artboardIds.size() << " artboards)" << std::endl;
    
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
        if (object.isComponent)
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

    // Ensure bytes (212) and assetId (204) keys declared for serializer-emitted chunks
    headerSet.insert(kFileAssetBytesKey);
    typeMap[kFileAssetBytesKey] = rive::CoreStringType::id; // bytes/share id with string
    headerSet.insert(kFileAssetIdKey);
    typeMap[kFileAssetIdKey] = rive::CoreUintType::id;

    std::vector<uint16_t> headerKeys(headerSet.begin(), headerSet.end());
    std::sort(headerKeys.begin(), headerKeys.end());

    std::vector<uint8_t> buffer;
    buffer.reserve(16384); // Reserve 16KB for large files
    VectorBinaryWriter writer(&buffer);

    SerializerDiagnostics diag;
    if (diag.isEnabled())
    {
        diag.info("Starting RIV serialization (serialize_core_document)");
        diag.logCount("Objects", document.objects.size());
        diag.logCount("Header keys", headerKeys.size());
    }

    // File header
    diag.beginChunk("HEADER", buffer.size());
    writer.write(reinterpret_cast<const uint8_t*>("RIVE"), 4);
    writer.writeVarUint(static_cast<uint32_t>(rive::File::majorVersion));
    writer.writeVarUint(static_cast<uint32_t>(rive::File::minorVersion));
    writer.writeVarUint(uint32_t{0}); // file id
    diag.endChunk("HEADER", buffer.size());

    // Table of Contents
    diag.beginChunk("TOC", buffer.size(), headerKeys.size() * 2);
    for (auto key : headerKeys)
    {
        writer.writeVarUint(static_cast<uint32_t>(key));
    }
    writer.writeVarUint(uint32_t{0});
    diag.endChunk("TOC", buffer.size());

    // NOTE: No padding between ToC and bitmap
    // RuntimeHeader::read() expects bitmap to start immediately after 0 terminator
    // See include/rive/runtime_header.hpp:87-93 - readUint32() is called right after ToC loop

    // Bitmap
    diag.beginChunk("BITMAP", buffer.size());
    diag.checkAlignment("Bitmap", buffer.size(), 4);
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
    diag.endChunk("BITMAP", buffer.size());

    // Objects
    diag.beginChunk("OBJECTS", buffer.size());
    diag.logCount("Object count", document.objects.size());

    bool placeholderEmitted = false;  // PR1: Separate flags
    bool fontBytesEmitted = false;
    std::unordered_map<uint32_t, uint32_t> localComponentIndex;
    uint32_t nextLocalIndex = 0;

    // PR2c: Collect stream property keys and enable diagnostics
    std::unordered_set<uint16_t> streamPropKeys;

    for (size_t objIndex = 0; objIndex < document.objects.size(); ++objIndex)
    {
        const auto& object = document.objects[objIndex];
        writer.writeVarUint(static_cast<uint32_t>(object.typeKey));

        if (object.isArtboard)
        {
            localComponentIndex.clear();
            localComponentIndex.emplace(object.id, 0);
            nextLocalIndex = 1;
        }

        if (object.isComponent)
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

        // PR2b: Track remap misses for diagnostic (core_document path)
        static std::map<uint16_t, int> remapMissCountCore;
        static bool firstRunCore = true;
        
        for (const auto& property : properties)
        {
            // PR2c: HEADER_MISS check
            if (headerSet.find(property.key) == headerSet.end())
            {
                std::cerr << "HEADER_MISS key=" << property.key
                          << " typeKey=" << object.typeKey << std::endl;
                continue;
            }
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
                    else
                    {
                        // PR2b: Mapping not found - SKIP this property entirely
                        remapMissCountCore[property.key]++;
                        if (remapMissCountCore[property.key] <= 10)
                        {
                            std::cerr << "⚠️  PR2b remap-miss (core): key=" << property.key 
                                      << " globalId=" << globalId << " — skipping property" << std::endl;
                        }
                        continue; // CRITICAL: Skip writing this property
                    }
                }
            }
            
            int fieldId = fieldIdForKey(property.key, typeMap);
            writeProperty(writer, property.key, property, fieldId);
        }
        
        // PR2b: Print summary at end
        if (firstRunCore && !remapMissCountCore.empty())
        {
            std::cerr << "\n=== PR2b REMAP MISS SUMMARY (CORE) ===" << std::endl;
            for (const auto& [key, count] : remapMissCountCore)
            {
                std::cerr << "  key " << key << ": " << count << " misses" << std::endl;
            }
            std::cerr << "=======================================\n" << std::endl;
            firstRunCore = false;
        }

        writer.writeVarUint(uint32_t{0}); // Property terminator
        
        // PR1: Asset placeholder AFTER Backboard properties complete (if no font)
        if (objIndex == 0 && !fontBytesEmitted && !placeholderEmitted && document.fontData.empty())
        {
            placeholderEmitted = true;
            // Write ImageAsset (105) placeholder
            writer.writeVarUint(static_cast<uint32_t>(rive::ImageAssetBase::typeKey)); // 105
            writer.writeVarUint(static_cast<uint32_t>(kFileAssetIdKey)); // 204
            writer.writeVarUint(static_cast<uint32_t>(0));   // assetId = 0
            writer.writeVarUint(static_cast<uint32_t>(0));   // property terminator

            // Write FileAssetContents (106) with empty bytes (212) as independent object
            writer.writeVarUint(static_cast<uint32_t>(rive::FileAssetContentsBase::typeKey)); // 106
            writer.writeVarUint(static_cast<uint32_t>(kFileAssetBytesKey)); // 212
            writer.writeVarUint(static_cast<uint32_t>(0));   // length = 0 (empty)
            writer.writeVarUint(static_cast<uint32_t>(0));   // Property terminator
            
            std::cout << "  ℹ️  Asset placeholder after Backboard (no font embedded)" << std::endl;
        }
        
        // PR1: Real font bytes AFTER FontAsset (141) properties complete
        if (!fontBytesEmitted && object.typeKey == 141)
        {
            if (!document.fontData.empty())
            {
                fontBytesEmitted = true;
                
                // Write FileAssetContents (106) as independent object
                writer.writeVarUint(static_cast<uint32_t>(rive::FileAssetContentsBase::typeKey));
                writer.writeVarUint(static_cast<uint32_t>(kFileAssetBytesKey)); // 212
                writer.writeVarUint(static_cast<uint32_t>(document.fontData.size()));
                writer.write(document.fontData.data(), document.fontData.size());
                writer.writeVarUint(uint32_t{0}); // Property terminator
                
                std::cout << "  ℹ️  Font bytes written after FontAsset (" << document.fontData.size() << " bytes)" << std::endl;
            }
        }
    }
    
    // End object stream with terminator
    writer.writeVarUint(static_cast<uint32_t>(0)); // Object stream terminator
    diag.endChunk("OBJECTS", buffer.size());
    
    diag.printSummary();

    // PR-RivePlay-Catalog: Write Artboard Catalog chunk for proper artboard selection
    // This must come AFTER object stream terminator, as a separate chunk
    std::cout << "\n  ℹ️  Writing Artboard Catalog chunk..." << std::endl;
    
    // Collect artboard IDs from document
    // Note: 0x0 artboards are already filtered by universal_builder, so all artboards here are valid
    std::vector<uint32_t> artboardIds;
    for (const auto& object : document.objects) {
        if (object.isArtboard) {
            artboardIds.push_back(object.id);
            std::cout << "    - Artboard id: " << object.id << std::endl;
        }
    }
    
    // DISABLED: ArtboardCatalog causes "Unknown property key" warnings in Rive Play  
    // The catalog chunk uses type keys 8726/8776 which are not in the ToC,
    // causing Rive Play to stop creating drawable objects → grey screen
    // TODO: Either add 8726/8776 to ToC or use official runtime encoding
    /*
    if (!artboardIds.empty()) {
        // Write ArtboardList (8726) wrapper
        writer.writeVarUint(static_cast<uint32_t>(8726)); // ArtboardList typeKey
        writer.writeVarUint(static_cast<uint32_t>(0));    // No properties, terminate immediately
        
        // Write ArtboardListItem (8776) for each non-empty artboard
        for (uint32_t artboardId : artboardIds) {
            writer.writeVarUint(static_cast<uint32_t>(8776)); // ArtboardListItem typeKey
            writer.writeVarUint(static_cast<uint32_t>(3));    // id property key (component id)
            writer.writeVarUint(artboardId);                   // artboard's builder ID
            writer.writeVarUint(static_cast<uint32_t>(0));    // Property terminator
        }
    }
    */
    
    // Final chunk terminator
    writer.writeVarUint(static_cast<uint32_t>(0));
    
    std::cout << "  ✅ Artboard Catalog written (" << artboardIds.size() << " artboards)" << std::endl;
    
    return buffer;
}
std::vector<uint8_t> serialize_exact_riv_json(const nlohmann::json& data)
{
    std::vector<uint8_t> buffer;
    buffer.reserve(4096);
    rive::VectorBinaryWriter writer(&buffer);

    writer.write(reinterpret_cast<const uint8_t*>("RIVE"), 4);

    const uint64_t major = data.value("majorVersion", 7);
    const uint64_t minor = data.value("minorVersion", 0);
    const uint64_t fileId = data.value("fileId", static_cast<uint64_t>(0));

    writer.writeVarUint(major);
    writer.writeVarUint(minor);
    writer.writeVarUint(fileId);

    const auto& headerKeys = data.at("headerKeys");
    if (!headerKeys.is_array())
    {
        throw std::runtime_error("headerKeys must be an array in exact RIV JSON payload");
    }
    for (const auto& entry : headerKeys)
    {
        const uint64_t key = entry.is_object() ? entry.at("key").get<uint64_t>() : entry.get<uint64_t>();
        writer.writeVarUint(key);
    }
    writer.writeVarUint(uint32_t{0});

    const auto& bitmaps = data.at("bitmaps");
    if (!bitmaps.is_array())
    {
        throw std::runtime_error("bitmaps must be an array in exact RIV JSON payload");
    }
    const auto expectedBitmapWords = (headerKeys.size() + 3) / 4;
    if (bitmaps.size() != expectedBitmapWords)
    {
        throw std::runtime_error("bitmaps array length does not match header key count");
    }
    for (const auto& value : bitmaps)
    {
        writer.write(static_cast<uint32_t>(value.get<uint64_t>()));
    }

    const auto& objects = data.at("objects");
    if (!objects.is_array())
    {
        throw std::runtime_error("objects must be an array in exact RIV JSON payload");
    }
    for (const auto& obj : objects)
    {
        const uint64_t typeKey = obj.at("typeKey").get<uint64_t>();
        writer.writeVarUint(typeKey);

        const auto& properties = obj.at("properties");
        for (const auto& prop : properties)
        {
            const uint64_t key = prop.at("key").get<uint64_t>();
            writer.writeVarUint(key);

            const std::string category = prop.value("category", std::string("uint"));
            const auto& value = prop.at("value");

            if (category == "string")
            {
                writer.write(value.get<std::string>());
            }
            else if (category == "double")
            {
                float f = static_cast<float>(value.get<double>());
                writer.writeFloat(f);
            }
            else if (category == "color")
            {
                writer.write(static_cast<uint32_t>(value.get<uint64_t>()));
            }
            else if (category == "bytes")
            {
                std::string encoded = value.get<std::string>();
                std::vector<uint8_t> bytes = raw_decode_base64(encoded);
                writer.writeVarUint(static_cast<uint64_t>(bytes.size()));
                if (!bytes.empty())
                {
                    writer.write(bytes.data(), bytes.size());
                }
            }
            else if (category == "bool")
            {
                uint64_t v = value.is_boolean() ? (value.get<bool>() ? 1u : 0u)
                                                : value.get<uint64_t>();
                writer.writeVarUint(v ? uint64_t{1} : uint64_t{0});
            }
            else
            {
                if (value.is_number_unsigned())
                {
                    writer.writeVarUint(value.get<uint64_t>());
                }
                else if (value.is_number_integer())
                {
                    int64_t v = value.get<int64_t>();
                    writer.writeVarUint(static_cast<uint64_t>(v));
                }
                else if (value.is_boolean())
                {
                    writer.writeVarUint(value.get<bool>() ? uint64_t{1} : uint64_t{0});
                }
                else if (value.is_string())
                {
                    writer.write(value.get<std::string>());
                }
                else
                {
                    writer.writeVarUint(uint32_t{0});
                }
            }
        }

        writer.writeVarUint(uint32_t{0});
    }

    if (data.contains("objectTerminator"))
    {
        const auto& terminatorJson = data.at("objectTerminator");
        const std::string terminatorEncoded = terminatorJson.is_string() ? terminatorJson.get<std::string>()
                                                                        : std::string();
        if (!terminatorEncoded.empty())
        {
            std::vector<uint8_t> terminatorBytes = raw_decode_base64(terminatorEncoded);
            if (terminatorBytes.empty())
            {
                throw std::runtime_error("objectTerminator must decode to at least one byte");
            }
            writer.write(terminatorBytes.data(), terminatorBytes.size());
        }
    }
    else
    {
        writer.writeVarUint(uint32_t{0});
    }

    std::string tailEncoded = data.value("tail", std::string());
    if (!tailEncoded.empty())
    {
        std::vector<uint8_t> tailBytes = raw_decode_base64(tailEncoded);
        if (!tailBytes.empty())
        {
            writer.write(tailBytes.data(), tailBytes.size());
        }
    }

    return buffer;
}

} // namespace rive_converter
