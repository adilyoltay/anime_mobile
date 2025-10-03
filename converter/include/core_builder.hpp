#pragma once
#include <vector>
#include <unordered_map>
#include <variant>
#include <memory>
#include "json_loader.hpp"
#include "rive/core.hpp"

namespace rive
{
class Core;
class Artboard;
}

namespace rive_converter
{
struct Property
{
    uint16_t key = 0;
    std::variant<float, uint32_t, std::string, bool, std::vector<uint8_t>> value;
};

struct CoreObject
{
    std::unique_ptr<rive::Core> core;
    uint16_t typeKey = 0;
    bool isComponent = false;
    bool isArtboard = false;
    bool isDrawable = false;
    uint32_t id = 0;
    uint32_t parentId = 0;
    std::vector<Property> properties;
};

struct CoreDocument
{
    std::vector<CoreObject> objects;
    std::vector<uint8_t> fontData; // Embedded font binary (TTF/OTF)
};

using PropertyTypeMap = std::unordered_map<uint16_t, uint8_t>;

class CoreBuilder
{
public:
    CoreBuilder();

    CoreObject& addCore(rive::Core* core);
    void setParent(CoreObject& object, uint32_t parentId);
    void set(CoreObject& object, uint16_t key, float value);
    void set(CoreObject& object, uint16_t key, uint32_t value);
    void set(CoreObject& object, uint16_t key, const std::string& value);
    void set(CoreObject& object, uint16_t key, bool value);
    void set(CoreObject& object, uint16_t key, const std::vector<uint8_t>& value);

    CoreDocument build(PropertyTypeMap& typeMap);

private:
    std::deque<CoreObject> m_objects;
    uint32_t m_nextId = 1;
};

CoreDocument build_core_document(const Document& document,
                                PropertyTypeMap& typeMap);
}
