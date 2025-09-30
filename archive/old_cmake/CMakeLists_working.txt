cmake_minimum_required(VERSION 3.16)
project(WorkingRivConverter)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include nlohmann/json (header-only library)
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# Working converter executable
add_executable(working_converter
    working_riv_converter.cpp
)

target_link_libraries(working_converter
    nlohmann_json::nlohmann_json
)

# Install targets
install(TARGETS working_converter
    RUNTIME DESTINATION bin
)

# Print configuration info
message(STATUS "Working RIV Converter - Template-based approach for valid RIV files")
