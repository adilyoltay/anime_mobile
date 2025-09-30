#pragma once
#include <string>
#include <vector>
#include <fstream>

namespace rive_converter
{

// Load a font file (TTF/OTF) as binary data
inline std::vector<uint8_t> load_font_file(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        return {};
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
    {
        return {};
    }
    
    return buffer;
}

// Get default system font path
inline std::string get_system_font_path(const std::string& fontName = "Arial")
{
    #ifdef __APPLE__
    return "/System/Library/Fonts/Supplemental/" + fontName + ".ttf";
    #elif _WIN32
    return "C:\\Windows\\Fonts\\" + fontName + ".ttf";
    #else
    return "/usr/share/fonts/truetype/" + fontName + ".ttf";
    #endif
}

} // namespace rive_converter
