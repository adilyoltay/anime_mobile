#include "serializer.hpp"
#include "json_loader.hpp"
#include "rive/file.hpp"
#include "utils/no_op_factory.hpp"

#include <fstream>
#include <iostream>
#include <iterator>

int main()
{
    const std::string jsonPath = "converter/test_simple.json";

    std::ifstream file(jsonPath);
    if (!file.is_open())
    {
        std::cerr << "Failed to open JSON: " << jsonPath << "\n";
        return 1;
    }

    std::string jsonContent((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());

    auto document = rive_converter::parse_json(jsonContent);
    auto bytes = rive_converter::serialize_minimal_riv(document);

    rive::NoOpFactory factory;
    rive::ImportResult result = rive::ImportResult::malformed;

    // Dump bytes for inspection
    {
        std::ofstream out("/tmp/import_check.riv", std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    auto rivFile = rive::File::import(
        rive::Span<const uint8_t>(bytes.data(), bytes.size()),
        &factory,
        &result);

    std::cout << "ImportResult enum value: "
              << static_cast<int>(result)
              << (rivFile ? " (success)" : " (failure)") << "\n";

    if (!rivFile)
    {
        std::cerr << "File import failed. Wrote /tmp/import_check.riv (" << bytes.size() << ")\n";
        return 2;
    }

    std::cout << "Artboard count: " << rivFile->artboardCount() << "\n";
    if (auto artboard = rivFile->artboard(); artboard != nullptr)
    {
        std::cout << "Default artboard name: " << artboard->name() << "\n";
    }

    return 0;
}
