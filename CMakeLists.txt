cmake_minimum_required(VERSION 3.16)
project(RiveJsonConverter)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)

# Include nlohmann/json (header-only library)
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# Text rendering dependencies (HarfBuzz + SheenBidi)
FetchContent_Declare(
    harfbuzz
    GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
    GIT_TAG 10.1.0
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(HB_BUILD_UTILS OFF CACHE BOOL "" FORCE)
set(HB_BUILD_SUBSET OFF CACHE BOOL "" FORCE)
set(HB_HAVE_CAIRO OFF CACHE BOOL "" FORCE)
set(HB_HAVE_FREETYPE OFF CACHE BOOL "" FORCE)
set(HB_HAVE_GRAPHITE2 OFF CACHE BOOL "" FORCE)
set(HB_HAVE_GLIB OFF CACHE BOOL "" FORCE)
set(HB_HAVE_ICU OFF CACHE BOOL "" FORCE)
set(HB_HAVE_GOBJECT OFF CACHE BOOL "" FORCE)
set(HB_HAVE_INTROSPECTION OFF CACHE BOOL "" FORCE)
set(HB_HAVE_CORETEXT OFF CACHE BOOL "" FORCE)
set(HB_HAVE_UNISCRIBE OFF CACHE BOOL "" FORCE)
set(HB_HAVE_GDI OFF CACHE BOOL "" FORCE)
set(HB_HAVE_DIRECTWRITE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(harfbuzz)

FetchContent_Declare(
    sheenbidi
    GIT_REPOSITORY https://github.com/Tehreer/SheenBidi.git
    GIT_TAG v2.6
)
FetchContent_MakeAvailable(sheenbidi)

add_library(rive_sheenbidi STATIC ${sheenbidi_SOURCE_DIR}/Source/SheenBidi.c)
target_include_directories(rive_sheenbidi PUBLIC ${sheenbidi_SOURCE_DIR}/Headers)
target_compile_definitions(rive_sheenbidi PRIVATE SB_CONFIG_UNITY)
set_target_properties(rive_sheenbidi PROPERTIES C_STANDARD 99)

if(TARGET harfbuzz)
    set(RIVE_HARFBUZZ_TARGET harfbuzz)
elseif(TARGET harfbuzz::harfbuzz)
    set(RIVE_HARFBUZZ_TARGET harfbuzz::harfbuzz)
else()
    message(FATAL_ERROR "HarfBuzz target not found after FetchContent")
endif()

# Rive runtime paths
set(RIVE_RUNTIME_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(RIVE_INCLUDE_DIR ${RIVE_RUNTIME_DIR}/include)
set(RIVE_SRC_DIR ${RIVE_RUNTIME_DIR}/src)

# Include directories
include_directories(${RIVE_INCLUDE_DIR})
include_directories(${RIVE_RUNTIME_DIR})

# Collect all Rive source files
file(GLOB_RECURSE RIVE_SOURCES 
    ${RIVE_SRC_DIR}/*.cpp
    ${RIVE_SRC_DIR}/*.c
)

# Exclude test files and examples
list(FILTER RIVE_SOURCES EXCLUDE REGEX ".*/tests?/.*")
list(FILTER RIVE_SOURCES EXCLUDE REGEX ".*/examples?/.*")
list(FILTER RIVE_SOURCES EXCLUDE REGEX ".*/bench/.*")

# Add NoOpFactory implementation
list(APPEND RIVE_SOURCES ${RIVE_RUNTIME_DIR}/utils/no_op_factory.cpp)

# Create Rive static library
add_library(rive_runtime STATIC ${RIVE_SOURCES})
target_include_directories(rive_runtime PUBLIC ${RIVE_INCLUDE_DIR})

# Compiler definitions
target_compile_definitions(rive_runtime PRIVATE
    _RIVE_INTERNAL_
    WITH_RIVE_TOOLS
    WITH_RIVE_TEXT
)

target_link_libraries(rive_runtime
    PRIVATE
        ${RIVE_HARFBUZZ_TARGET}
        rive_sheenbidi
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(rive_runtime PRIVATE 
        _USE_MATH_DEFINES
        NOMINMAX
    )
    target_compile_options(rive_runtime PRIVATE 
        /wd4244 /wd4267 /wd4305 /wd4996
    )
elseif(APPLE)
    target_compile_definitions(rive_runtime PRIVATE RIVE_MACOSX)
    target_compile_options(rive_runtime PRIVATE 
        -Wno-unused-parameter 
        -Wno-sign-compare
    )
elseif(UNIX)
    target_compile_options(rive_runtime PRIVATE 
        -Wno-unused-parameter 
        -Wno-sign-compare
    )
endif()

# JSON to RIV Converter library (archived - now in converter/)
# add_library(json_to_riv_converter STATIC
#     json_to_riv_converter.hpp
#     json_to_riv_converter.cpp
# )
# 
# target_link_libraries(json_to_riv_converter 
#     rive_runtime
#     nlohmann_json::nlohmann_json
# )

# Test executable (archived)
# add_executable(test_converter
#     test_converter.cpp
# )
# 
# target_link_libraries(test_converter
#     json_to_riv_converter
#     rive_runtime
#     nlohmann_json::nlohmann_json
# )

# Simple demo executable (archived)
# add_executable(simple_demo
#     simple_demo.cpp
# )
# 
# target_link_libraries(simple_demo
#     json_to_riv_converter
# )

# Install targets (commented - old converters archived)
# install(TARGETS test_converter simple_demo
#     RUNTIME DESTINATION bin
# )

install(FILES 
    bouncing_ball.json
    DESTINATION share/examples
)

# Print configuration info
message(STATUS "Rive Runtime Directory: ${RIVE_RUNTIME_DIR}")
message(STATUS "Rive Include Directory: ${RIVE_INCLUDE_DIR}")
message(STATUS "Found ${list_length} Rive source files")

add_subdirectory(converter)
