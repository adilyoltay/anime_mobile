@echo off
echo 🔨 Rive JSON to RIV Converter Build Script (Windows)
echo ==================================================

setlocal EnableDelayedExpansion

REM Configuration
set BUILD_DIR=build_converter
set CMAKE_BUILD_TYPE=Release
set CLEAN_BUILD=false
set VERBOSE=false

REM Parse command line arguments
:parse_args
if "%~1"=="" goto :args_parsed
if "%~1"=="--debug" (
    set CMAKE_BUILD_TYPE=Debug
    echo Debug build enabled
    shift & goto :parse_args
)
if "%~1"=="--clean" (
    set CLEAN_BUILD=true
    echo Clean build enabled
    shift & goto :parse_args
)
if "%~1"=="--verbose" (
    set VERBOSE=true
    echo Verbose output enabled
    shift & goto :parse_args
)
if "%~1"=="-h" goto :show_help
if "%~1"=="--help" goto :show_help
echo Unknown option: %~1
goto :error

:show_help
echo Usage: %0 [OPTIONS]
echo.
echo Options:
echo   --debug     Build in debug mode
echo   --clean     Clean build directory first
echo   --verbose   Show detailed build output
echo   -h, --help  Show this help message
echo.
exit /b 0

:args_parsed

REM Check if we're in the right directory
if not exist "json_to_riv_converter.hpp" (
    echo ❌ Error: Please run this script from the rive-runtime directory
    echo Current directory: %CD%
    goto :error
)

REM Check for required dependencies
echo 🔍 Checking dependencies...

REM Check for CMake
cmake --version >nul 2>&1
if errorlevel 1 (
    echo ❌ CMake is required but not installed
    echo Please install CMake: https://cmake.org/
    goto :error
)
echo    ✓ CMake found

REM Check for Visual Studio or other C++ compiler
cl.exe >nul 2>&1
if not errorlevel 1 (
    echo    ✓ MSVC found
    set GENERATOR="Visual Studio 16 2019"
) else (
    clang++.exe --version >nul 2>&1
    if not errorlevel 1 (
        echo    ✓ Clang++ found
        set GENERATOR="MinGW Makefiles"
    ) else (
        g++.exe --version >nul 2>&1
        if not errorlevel 1 (
            echo    ✓ G++ found
            set GENERATOR="MinGW Makefiles"
        ) else (
            echo ❌ C++ compiler (MSVC, Clang++, or G++) is required
            goto :error
        )
    )
)

REM Clean build directory if requested
if "%CLEAN_BUILD%"=="true" (
    echo 🧹 Cleaning build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)

REM Create build directory
echo 📁 Setting up build directory...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM Configure with CMake
echo ⚙️  Configuring with CMake...
set CMAKE_ARGS=-DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE%

if "%VERBOSE%"=="true" (
    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_VERBOSE_MAKEFILE=ON
)

cmake .. %CMAKE_ARGS% -G %GENERATOR%
if errorlevel 1 (
    echo ❌ CMake configuration failed
    goto :error
)

REM Build the project
echo 🔨 Building project...
cmake --build . --config %CMAKE_BUILD_TYPE%
if errorlevel 1 (
    echo ❌ Build failed
    goto :error
)

echo ✅ Build completed successfully!
echo.

REM List built executables
echo 📦 Built executables:
if exist "Debug\test_converter.exe" (
    echo    ✓ Debug\test_converter.exe - Main converter with file I/O
) else if exist "Release\test_converter.exe" (
    echo    ✓ Release\test_converter.exe - Main converter with file I/O
) else if exist "test_converter.exe" (
    echo    ✓ test_converter.exe - Main converter with file I/O
)

if exist "Debug\simple_demo.exe" (
    echo    ✓ Debug\simple_demo.exe - Simple demo with embedded JSON
) else if exist "Release\simple_demo.exe" (
    echo    ✓ Release\simple_demo.exe - Simple demo with embedded JSON
) else if exist "simple_demo.exe" (
    echo    ✓ simple_demo.exe - Simple demo with embedded JSON
)

echo.

REM Run simple demo if available
set DEMO_PATH=
if exist "Debug\simple_demo.exe" set DEMO_PATH=Debug\simple_demo.exe
if exist "Release\simple_demo.exe" set DEMO_PATH=Release\simple_demo.exe
if exist "simple_demo.exe" set DEMO_PATH=simple_demo.exe

if not "%DEMO_PATH%"=="" (
    echo 🎬 Running simple demo...
    echo.
    "%DEMO_PATH%"
    echo.
)

REM Copy JSON example to build directory
if exist "..\bouncing_ball.json" (
    copy "..\bouncing_ball.json" . >nul
    echo 📋 Copied example JSON file: bouncing_ball.json
)

REM Show usage instructions
echo 🎉 Setup complete!
echo.
echo Usage examples:
if not "%DEMO_PATH%"=="" (
    echo   # Run simple demo (embedded JSON):
    echo   cd %BUILD_DIR% ^& "%DEMO_PATH%"
    echo.
)

set TEST_PATH=
if exist "Debug\test_converter.exe" set TEST_PATH=Debug\test_converter.exe
if exist "Release\test_converter.exe" set TEST_PATH=Release\test_converter.exe
if exist "test_converter.exe" set TEST_PATH=test_converter.exe

if not "%TEST_PATH%"=="" (
    echo   # Convert external JSON file:
    echo   cd %BUILD_DIR% ^& "%TEST_PATH%" bouncing_ball.json output.riv
    echo.
    echo   # Test with your own JSON:
    echo   cd %BUILD_DIR% ^& "%TEST_PATH%" C:\path\to\your.json C:\path\to\output.riv
    echo.
)

echo 📝 Next steps:
echo 1. Test the converter with the provided examples
echo 2. Create your own JSON animation descriptions
echo 3. Integrate with AI models to generate JSON
echo 4. Use the generated .riv files in Rive players/viewers
echo.

echo 🔗 Integration example:
echo   # AI generates JSON → converter → .riv file
echo   ai_model.generate('bouncing red ball') → JSON → converter → animation.riv

goto :end

:error
echo.
echo Build failed! Check the error messages above.
exit /b 1

:end
endlocal
