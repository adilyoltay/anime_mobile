#!/bin/bash

echo "🔨 Rive JSON to RIV Converter Build Script"
echo "=========================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="build_converter"
CMAKE_BUILD_TYPE="Release"
CLEAN_BUILD=false
VERBOSE=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            CMAKE_BUILD_TYPE="Debug"
            echo -e "${YELLOW}Debug build enabled${NC}"
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            echo -e "${YELLOW}Clean build enabled${NC}"
            shift
            ;;
        --verbose)
            VERBOSE=true
            echo -e "${YELLOW}Verbose output enabled${NC}"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --debug     Build in debug mode"
            echo "  --clean     Clean build directory first"
            echo "  --verbose   Show detailed build output"
            echo "  -h, --help  Show this help message"
            echo ""
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Check if we're in the right directory
if [ ! -f "json_to_riv_converter.hpp" ]; then
    echo -e "${RED}❌ Error: Please run this script from the rive-runtime directory${NC}"
    echo "Current directory: $(pwd)"
    exit 1
fi

# Check for required dependencies
echo -e "${BLUE}🔍 Checking dependencies...${NC}"

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}❌ CMake is required but not installed${NC}"
    echo "Please install CMake: https://cmake.org/"
    exit 1
fi
echo "   ✓ CMake found: $(cmake --version | head -n1)"

# Check for C++ compiler
if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    echo -e "${RED}❌ C++ compiler (g++ or clang++) is required${NC}"
    exit 1
fi

if command -v clang++ &> /dev/null; then
    echo "   ✓ Clang++ found: $(clang++ --version | head -n1)"
elif command -v g++ &> /dev/null; then
    echo "   ✓ G++ found: $(g++ --version | head -n1)"
fi

# Clean build directory if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}🧹 Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
echo -e "${BLUE}📁 Setting up build directory...${NC}"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo -e "${BLUE}⚙️  Configuring with CMake...${NC}"
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE"

if [ "$VERBOSE" = true ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_VERBOSE_MAKEFILE=ON"
fi

if ! cmake .. $CMAKE_ARGS; then
    echo -e "${RED}❌ CMake configuration failed${NC}"
    exit 1
fi

# Build the project
echo -e "${BLUE}🔨 Building project...${NC}"
MAKE_ARGS="-j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

if [ "$VERBOSE" = true ]; then
    MAKE_ARGS="$MAKE_ARGS VERBOSE=1"
fi

if ! cmake --build . --config $CMAKE_BUILD_TYPE -- $MAKE_ARGS; then
    echo -e "${RED}❌ Build failed${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Build completed successfully!${NC}"
echo ""

# List built executables
echo -e "${BLUE}📦 Built executables:${NC}"
if [ -f "test_converter" ] || [ -f "test_converter.exe" ]; then
    echo "   ✓ test_converter - Main converter with file I/O"
fi
if [ -f "simple_demo" ] || [ -f "simple_demo.exe" ]; then
    echo "   ✓ simple_demo - Simple demo with embedded JSON"
fi

echo ""

# Run simple demo if available
if [ -f "simple_demo" ] || [ -f "simple_demo.exe" ]; then
    echo -e "${GREEN}🎬 Running simple demo...${NC}"
    echo ""
    
    if [ -f "simple_demo" ]; then
        ./simple_demo
    else
        ./simple_demo.exe
    fi
    
    echo ""
fi

# Copy JSON example to build directory
if [ -f "../bouncing_ball.json" ]; then
    cp "../bouncing_ball.json" .
    echo -e "${BLUE}📋 Copied example JSON file: bouncing_ball.json${NC}"
fi

# Show usage instructions
echo -e "${GREEN}🎉 Setup complete!${NC}"
echo ""
echo -e "${BLUE}Usage examples:${NC}"
echo "  # Run simple demo (embedded JSON):"
echo "  cd $BUILD_DIR && ./simple_demo"
echo ""
echo "  # Convert external JSON file:"
echo "  cd $BUILD_DIR && ./test_converter bouncing_ball.json output.riv"
echo ""
echo "  # Test with your own JSON:"
echo "  cd $BUILD_DIR && ./test_converter /path/to/your.json /path/to/output.riv"
echo ""

echo -e "${YELLOW}📝 Next steps:${NC}"
echo "1. Test the converter with the provided examples"
echo "2. Create your own JSON animation descriptions"
echo "3. Integrate with AI models to generate JSON"
echo "4. Use the generated .riv files in Rive players/viewers"
echo ""

echo -e "${BLUE}🔗 Integration example:${NC}"
echo "  # AI generates JSON → converter → .riv file"
echo "  ai_model.generate('bouncing red ball') → JSON → converter → animation.riv"
