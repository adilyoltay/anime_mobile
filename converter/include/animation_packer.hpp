#pragma once

#include <vector>
#include <cstdint>

namespace rive_converter {

struct KeyFrameData {
    float frame;
    float value;
    uint32_t interpolatorId;
};

// Pack keyframes into Rive's packed format
std::vector<uint8_t> packKeyFrames(const std::vector<KeyFrameData>& keyframes);

} // namespace rive_converter
