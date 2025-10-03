#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

static std::string run_command_capture(const std::string& command)
{
    std::array<char, 4096> buffer{};
    std::string result;

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe)
    {
        throw std::runtime_error("Failed to execute command: " + command);
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
    {
        result.append(buffer.data());
    }

    int rc = pclose(pipe);
    if (rc != 0)
    {
        throw std::runtime_error("Command exited with non-zero status: " + command);
    }

    return result;
}

static fs::path resolve_script_path(const fs::path& exePath)
{
    fs::path exeDir = exePath.parent_path();
    fs::path rootDir = exeDir;
    for (int i = 0; i < 2; ++i)
    {
        rootDir = rootDir.parent_path();
    }
    fs::path scriptPath = rootDir / "converter" / "analyze_riv.py";
    if (!fs::exists(scriptPath))
    {
        throw std::runtime_error("Unable to locate analyze_riv.py at " + scriptPath.string());
    }
    return scriptPath;
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <input.riv> <output.json>" << std::endl;
        return 1;
    }

    try
    {
        fs::path inputPath = fs::absolute(argv[1]);
        fs::path outputPath = fs::absolute(argv[2]);
        if (!fs::exists(inputPath))
        {
            throw std::runtime_error("Input file does not exist: " + inputPath.string());
        }

        fs::path exePath = fs::canonical(argv[0]);
        fs::path scriptPath = resolve_script_path(exePath);

        std::string command = "python3 \"" + scriptPath.string() + "\" \"" + inputPath.string() + "\" --json";
        std::string rawOutput = run_command_capture(command);

        auto jsonStart = rawOutput.find('{');
        if (jsonStart == std::string::npos)
        {
            throw std::runtime_error("analyze_riv.py did not return JSON output");
        }
        auto jsonPayload = rawOutput.substr(jsonStart);

        nlohmann::json analysis = nlohmann::json::parse(jsonPayload);
        analysis["__riv_exact__"] = true;
        analysis["source"] = "analyze_riv";
        analysis["inputPath"] = inputPath.string();

        std::ofstream outFile(outputPath);
        if (!outFile.is_open())
        {
            throw std::runtime_error("Failed to open output file: " + outputPath.string());
        }
        outFile << analysis.dump(2) << std::endl;

        std::cout << "Extracted raw RIV stream to " << outputPath << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}
