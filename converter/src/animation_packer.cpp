// ANIMATION PACKER - Encode hierarchical keyframes to packed format
// Based on reverse engineering of Rive's packed animation format
//
// DISCOVERED PATTERN:
// Packed format uses property encoding similar to standard Rive:
//   Property keys: 67 (frame), 68 (value), 69 (interpolatorId)
//   Values: varints and floats as appropriate
//
// Structure (preliminary):
//   [metadata/refs] [prop_key] [value] [prop_key] [value] ...

#include "animation_packer.hpp"
#include <rive/core/binary_reader.hpp>
#include <cstring>
#include <vector>

namespace rive_converter {

// Write varuint (Rive format)
void writeVarUint(std::vector<uint8_t>& buffer, uint32_t value) {
    while (true) {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value > 0) {
            byte |= 0x80;
        }
        buffer.push_back(byte);
        if (value == 0) break;
    }
}

// Write float (little-endian)
void writeFloat(std::vector<uint8_t>& buffer, float value) {
    uint8_t bytes[4];
    std::memcpy(bytes, &value, 4);
    for (int i = 0; i < 4; i++) {
        buffer.push_back(bytes[i]);
    }
}

std::vector<uint8_t> packKeyFrames(const std::vector<KeyFrameData>& keyframes) {
    std::vector<uint8_t> packed;
    
    // For now, write in simple property format
    // Based on pattern discovered at offset 145-158:
    //   0x00 (frame varuint=0)
    //   0x0d (property key 13 or separator?)
    //   [float value]
    //   0x0e (property key 14 or next separator?)
    //   [float value]
    
    // HYPOTHESIS: This is just standard property encoding!
    // Property 67 = frame (but encoded as varint, not float!)
    // Property 68 = value (float)
    // Property 69 = interpolatorId (varint)
    
    for (const auto& kf : keyframes) {
        // Write frame (property 67) - as VARUINT not float!
        writeVarUint(packed, 67);  // Property key: frame
        writeVarUint(packed, static_cast<uint32_t>(kf.frame));  // Frame as varuint!
        
        // Write value (property 68) - as FLOAT
        writeVarUint(packed, 68);  // Property key: value
        writeFloat(packed, kf.value);
        
        // Write interpolatorId if non-zero (property 69)
        if (kf.interpolatorId != 0) {
            writeVarUint(packed, 69);  // Property key: interpolatorId
            writeVarUint(packed, kf.interpolatorId);
        }
    }
    
    return packed;
}

} // namespace rive_converter
