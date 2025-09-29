#include <iostream>
#include <fstream>
#include <vector>
#include "rive/file.hpp"
#include "utils/no_op_factory.hpp"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <riv_file>" << std::endl;
        return 1;
    }

    // Read file
    std::ifstream inFile(argv[1], std::ios::binary | std::ios::ate);
    if (!inFile)
    {
        std::cerr << "Failed to open file: " << argv[1] << std::endl;
        return 1;
    }
    
    size_t fileSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(fileSize);
    if (!inFile.read(reinterpret_cast<char*>(buffer.data()), fileSize))
    {
        std::cerr << "Failed to read file" << std::endl;
        return 1;
    }
    
    // Try to import
    rive::NoOpFactory factory;
    rive::ImportResult importResult;
    auto file = rive::File::import(buffer, &factory, &importResult);
    
    if (file)
    {
        std::cout << "SUCCESS: File imported successfully!" << std::endl;
        std::cout << "Artboard count: " << file->artboardCount() << std::endl;
        if (file->artboard())
        {
            std::cout << "Artboard name: " << file->artboard()->name() << std::endl;
            std::cout << "Artboard width: " << file->artboard()->width() << std::endl;
            std::cout << "Artboard height: " << file->artboard()->height() << std::endl;

            auto instance = file->artboard()->instance();
            if (instance == nullptr)
            {
                std::cout << "Artboard instance could not be created" << std::endl;
            }
            else
            {
                instance->advance(0.0f);
                std::cout << "Artboard instance initialized." << std::endl;
            }
        }
    }
    else
    {
        std::cout << "FAILED: Import failed - file is null" << std::endl;
        std::cout << "Import result: " << static_cast<int>(importResult) << std::endl;
        switch (importResult)
        {
            case rive::ImportResult::success:
                std::cout << "  Status: Success (but file is null?)" << std::endl;
                break;
            case rive::ImportResult::unsupportedVersion:
                std::cout << "  Status: Unsupported version" << std::endl;
                break;
            case rive::ImportResult::malformed:
                std::cout << "  Status: Malformed file" << std::endl;
                break;
            default:
                std::cout << "  Status: Unknown" << std::endl;
        }
    }
    
    return 0;
}
