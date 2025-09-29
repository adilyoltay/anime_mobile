#include "json_loader.hpp"
#include "serializer.hpp"
#include <fstream>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: rive_convert <input.json> <output.riv>" << std::endl;
        return 1;
    }

    const std::string inputPath = argv[1];
    const std::string outputPath = argv[2];

    std::ifstream inputFile(inputPath);
    if (!inputFile.is_open())
    {
        std::cerr << "Failed to open input file: " << inputPath << std::endl;
        return 1;
    }

    std::string jsonContent((std::istreambuf_iterator<char>(inputFile)),
                            std::istreambuf_iterator<char>());

    try
    {
        auto document = rive_converter::parse_json(jsonContent);
        auto bytes = rive_converter::serialize_minimal_riv(document);

        std::ofstream outputFile(outputPath, std::ios::binary);
        outputFile.write(reinterpret_cast<const char*>(bytes.data()),
                         bytes.size());
        std::cout << "Wrote RIV file: " << outputPath << " (" << bytes.size()
                  << " bytes)" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

