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
    src/hierarchical_parser.cpp
    src/universal_builder.cpp
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

# RIV Extractor - Extract complete object tree to JSON
add_executable(riv_extractor
    riv_extractor.cpp
    ../utils/no_op_factory.cpp
)
target_link_libraries(riv_extractor
    PRIVATE
        rive_runtime
        nlohmann_json::nlohmann_json
)
target_include_directories(riv_extractor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../utils
)

# Full Extractor - Better vertex grouping
add_executable(full_extractor
    full_extractor.cpp
    ../utils/no_op_factory.cpp
)
target_link_libraries(full_extractor
    PRIVATE
        rive_runtime
        nlohmann_json::nlohmann_json
)
target_include_directories(full_extractor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../utils
)

# Complete Hierarchy Extractor - TRUE exact copy with component tracking
add_executable(complete_extractor
    complete_hierarchy_extractor.cpp
    ../utils/no_op_factory.cpp
)
target_link_libraries(complete_extractor
    PRIVATE
        rive_runtime
        nlohmann_json::nlohmann_json
)
target_include_directories(complete_extractor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../utils
)

# Hierarchical Extractor - TRUE Shape grouping
add_executable(hierarchical_extractor
    hierarchical_extractor.cpp
    ../utils/no_op_factory.cpp
)
target_link_libraries(hierarchical_extractor
    PRIVATE
        rive_runtime
        nlohmann_json::nlohmann_json
)
target_include_directories(hierarchical_extractor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../utils
)

# Universal Extractor - Extract EVERYTHING (all types, all properties)
add_executable(universal_extractor
    universal_extractor.cpp
    ../utils/no_op_factory.cpp
)
target_link_libraries(universal_extractor
    PRIVATE
        rive_runtime
        nlohmann_json::nlohmann_json
)
target_include_directories(universal_extractor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../utils
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

# JSON Validator - Validate JSON input before conversion
add_executable(json_validator
    json_validator_main.cpp
    src/json_validator.cpp
)
target_link_libraries(json_validator
    PRIVATE
        nlohmann_json::nlohmann_json
)
target_include_directories(json_validator
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

