#include "core_builder.hpp"
#include "rive/assets/file_asset_contents.hpp"
#include <cstring>

namespace rive_converter
{

void addEmptyAssetPack(CoreBuilder& builder)
{
    // Add an empty FileAssetContents (typeKey 105)
    // This satisfies Rive Play's requirement for an asset pack
    
    auto& assetPack = builder.addCore(new rive::FileAssetContents());
    
    // Empty asset data - just a placeholder
    // In serializer, we'll write:
    // - typeKey: 105
    // - property 388 (cdnUuid): empty string
    // - property 389 (cdnBaseUrl): empty string  
    // - asset bytes length: 0
}

void addArtboardCatalog(CoreBuilder& builder, uint32_t artboardId)
{
    // These special typeKeys (8726, 8776) are not in the SDK
    // We'll need to write them manually in the serializer
    // For now, just document what should be written:
    
    // typeKey 8726 (0x2216) - Artboard List/Catalog header
    // typeKey 8776 (0x2248) - Artboard List Item
    //   - Links to artboard by ID
    //   - Provides metadata for UI display
}

void addDrawableChain(CoreBuilder& builder, const std::vector<uint32_t>& drawableIds)
{
    // Add DrawRules (typeKey 49/420) to define draw order
    // This tells the renderer which shapes to draw and in what order
    
    // Note: DrawRules (49) vs LayoutComponentStyle (420) 
    // Need to investigate which is correct for our use case
}

} // namespace rive_converter
