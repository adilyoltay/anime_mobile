cmake_minimum_required(VERSION 3.16)
project(RealRiveConverter)

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

# Rive runtime paths (go up one directory from build_real)
set(RIVE_RUNTIME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(RIVE_INCLUDE_DIR ${RIVE_RUNTIME_DIR}/include)
set(RIVE_SRC_DIR ${RIVE_RUNTIME_DIR}/src)
set(RIVE_UTILS_DIR ${RIVE_RUNTIME_DIR}/utils)

# Include directories
include_directories(${RIVE_INCLUDE_DIR})
include_directories(${RIVE_RUNTIME_DIR})
include_directories(${RIVE_UTILS_DIR})

# Collect core Rive source files (minimal set for basic functionality)
set(RIVE_CORE_SOURCES
    # Core binary handling
    ${RIVE_SRC_DIR}/core/binary_reader.cpp
    ${RIVE_SRC_DIR}/core/binary_writer.cpp
    ${RIVE_SRC_DIR}/core/field_types/core_bool_type.cpp
    ${RIVE_SRC_DIR}/core/field_types/core_bytes_type.cpp
    ${RIVE_SRC_DIR}/core/field_types/core_color_type.cpp
    ${RIVE_SRC_DIR}/core/field_types/core_double_type.cpp
    ${RIVE_SRC_DIR}/core/field_types/core_string_type.cpp
    ${RIVE_SRC_DIR}/core/field_types/core_uint_type.cpp
    
    # Basic components
    ${RIVE_SRC_DIR}/component.cpp
    ${RIVE_SRC_DIR}/container_component.cpp
    ${RIVE_SRC_DIR}/transform_component.cpp
    ${RIVE_SRC_DIR}/node.cpp
    
    # Artboard
    ${RIVE_SRC_DIR}/artboard.cpp
    ${RIVE_SRC_DIR}/file.cpp
    ${RIVE_SRC_DIR}/factory.cpp
    
    # Shapes
    ${RIVE_SRC_DIR}/shapes/shape.cpp
    ${RIVE_SRC_DIR}/shapes/rectangle.cpp
    ${RIVE_SRC_DIR}/shapes/ellipse.cpp
    ${RIVE_SRC_DIR}/shapes/path.cpp
    ${RIVE_SRC_DIR}/shapes/parametric_path.cpp
    
    # Paint
    ${RIVE_SRC_DIR}/shapes/paint/shape_paint.cpp
    ${RIVE_SRC_DIR}/shapes/paint/fill.cpp
    ${RIVE_SRC_DIR}/shapes/paint/stroke.cpp
    ${RIVE_SRC_DIR}/shapes/paint/solid_color.cpp
    ${RIVE_SRC_DIR}/shapes/paint/color.cpp
    
    # Animation basics
    ${RIVE_SRC_DIR}/animation/linear_animation.cpp
    ${RIVE_SRC_DIR}/animation/keyframe.cpp
    ${RIVE_SRC_DIR}/animation/keyframe_double.cpp
    ${RIVE_SRC_DIR}/animation/keyed_object.cpp
    ${RIVE_SRC_DIR}/animation/keyed_property.cpp
    
    # Generated base classes (minimal set)
    ${RIVE_SRC_DIR}/generated/artboard_base.cpp
    ${RIVE_SRC_DIR}/generated/node_base.cpp
    ${RIVE_SRC_DIR}/generated/shapes/rectangle_base.cpp
    ${RIVE_SRC_DIR}/generated/shapes/ellipse_base.cpp
    ${RIVE_SRC_DIR}/generated/shapes/shape_base.cpp
    ${RIVE_SRC_DIR}/generated/shapes/paint/fill_base.cpp
    ${RIVE_SRC_DIR}/generated/shapes/paint/stroke_base.cpp
    ${RIVE_SRC_DIR}/generated/shapes/paint/solid_color_base.cpp
    ${RIVE_SRC_DIR}/generated/animation/linear_animation_base.cpp
    ${RIVE_SRC_DIR}/generated/animation/keyframe_double_base.cpp
    
    # Utils
    ${RIVE_UTILS_DIR}/no_op_factory.cpp
)

# Create Rive core library
add_library(rive_core STATIC ${RIVE_CORE_SOURCES})
target_include_directories(rive_core PUBLIC ${RIVE_INCLUDE_DIR})

# Compiler definitions
target_compile_definitions(rive_core PRIVATE
    _RIVE_INTERNAL_
    WITH_RIVE_TOOLS
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(rive_core PRIVATE 
        _USE_MATH_DEFINES
        NOMINMAX
    )
    target_compile_options(rive_core PRIVATE 
        /wd4244 /wd4267 /wd4305 /wd4996
    )
elseif(APPLE)
    target_compile_definitions(rive_core PRIVATE RIVE_MACOSX)
    target_compile_options(rive_core PRIVATE 
        -Wno-unused-parameter 
        -Wno-sign-compare
        -Wno-unused-variable
    )
elseif(UNIX)
    target_compile_options(rive_core PRIVATE 
        -Wno-unused-parameter 
        -Wno-sign-compare
        -Wno-unused-variable
    )
endif()

# Real JSON to RIV Converter library
add_library(real_json_to_riv_converter STATIC
    real_json_to_riv_converter.hpp
    real_json_to_riv_converter.cpp
)

target_link_libraries(real_json_to_riv_converter 
    rive_core
    nlohmann_json::nlohmann_json
)

# Real converter executable
add_executable(real_converter
    real_converter_main.cpp
)

target_link_libraries(real_converter
    real_json_to_riv_converter
    rive_core
    nlohmann_json::nlohmann_json
)

# Install targets
install(TARGETS real_converter
    RUNTIME DESTINATION bin
)

install(FILES 
    bouncing_ball.json
    DESTINATION share/examples
)

# Print configuration info
message(STATUS "Real Rive Converter - Using Rive Runtime API")
message(STATUS "Rive Runtime Directory: ${RIVE_RUNTIME_DIR}")
message(STATUS "Rive Include Directory: ${RIVE_INCLUDE_DIR}")
message(STATUS "Core sources: ${RIVE_CORE_SOURCES}")
