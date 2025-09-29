cmake_minimum_required(VERSION 3.16)
project(rive_json_converter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

add_library(rive_convert
    src/json_loader.cpp
    src/core_builder.cpp
    src/serializer.cpp
)

target_include_directories(rive_convert
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(rive_convert
    PUBLIC
        nlohmann_json::nlohmann_json
        rive_runtime
)

add_executable(rive_convert_cli src/main.cpp)

target_link_libraries(rive_convert_cli
    PRIVATE
        rive_convert
)

# Add import test executable
add_executable(import_test import_test.cpp)
target_link_libraries(import_test
    PRIVATE
        rive_runtime
)

# Helper importer that exercises the freshly built converter/runtime
add_executable(import_check
    import_check.cpp
    ../utils/no_op_factory.cpp)

target_link_libraries(import_check
    PRIVATE
        rive_convert
        rive_runtime)

target_include_directories(import_check
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../utils)

