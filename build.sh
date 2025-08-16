#!/bin/bash
# Cross-platform build script for AK (API Key Manager)
# Works on Linux, macOS, and Windows (via Git Bash/WSL)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Platform detection
PLATFORM="Unknown"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="Linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macOS"
elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
    PLATFORM="Windows"
fi

print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  AK (API Key Manager) Build Script${NC}"
    echo -e "${BLUE}  Platform: $PLATFORM${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo
}

print_step() {
    echo -e "${YELLOW}🔧 $1${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
    exit 1
}

check_requirements() {
    print_step "Checking build requirements..."
    
    # Check for CMake
    if ! command -v cmake &> /dev/null; then
        print_error "CMake is required but not installed. Please install CMake 3.16 or higher."
    fi
    
    # Check CMake version
    CMAKE_VERSION=$(cmake --version | head -n1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')
    CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d. -f1)
    CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d. -f2)
    
    if [ "$CMAKE_MAJOR" -lt 3 ] || ([ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -lt 16 ]); then
        print_error "CMake 3.16 or higher is required. Found version: $CMAKE_VERSION"
    fi
    
    # Check for C++ compiler
    CXX_COMPILER=""
    if command -v g++ &> /dev/null; then
        CXX_COMPILER="g++"
    elif command -v clang++ &> /dev/null; then
        CXX_COMPILER="clang++"
    elif command -v cl &> /dev/null && [[ "$PLATFORM" == "Windows" ]]; then
        CXX_COMPILER="MSVC"
    else
        print_error "No C++ compiler found. Please install g++, clang++, or MSVC."
    fi
    
    print_success "Requirements check passed. Using $CXX_COMPILER compiler."
}

configure_build() {
    print_step "Configuring build system..."
    
    # Create build directory
    if [ -d "build" ]; then
        rm -rf build
    fi
    mkdir -p build
    cd build
    
    # Configure with CMake
    local CMAKE_ARGS=""
    if [[ "$PLATFORM" == "Windows" ]] && command -v cl &> /dev/null; then
        # Use Visual Studio generator on Windows if available
        CMAKE_ARGS="-G \"Visual Studio 16 2019\" -A x64"
    fi
    
    if [[ "$1" == "Debug" ]]; then
        CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=Debug"
    elif [[ "$1" == "Coverage" ]]; then
        CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=Coverage"
    else
        CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release"
    fi
    
    eval cmake .. $CMAKE_ARGS || print_error "CMake configuration failed"
    
    cd ..
    print_success "Build system configured successfully"
}

build_project() {
    print_step "Building project..."
    
    cd build
    
    # Build with appropriate number of cores
    local CORES=1
    if [[ "$PLATFORM" == "Linux" ]] || [[ "$PLATFORM" == "macOS" ]]; then
        CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)
    elif [[ "$PLATFORM" == "Windows" ]]; then
        CORES=$NUMBER_OF_PROCESSORS
    fi
    
    cmake --build . --parallel $CORES || print_error "Build failed"
    
    cd ..
    print_success "Project built successfully"
}

run_tests() {
    print_step "Running tests..."
    
    cd build
    
    # Run tests
    if [[ "$PLATFORM" == "Windows" ]]; then
        ./Debug/ak_tests.exe || ./Release/ak_tests.exe || print_error "Tests failed"
    else
        ./ak_tests || print_error "Tests failed"
    fi
    
    cd ..
    print_success "All tests passed"
}

generate_coverage() {
    if [[ "$PLATFORM" == "Windows" ]]; then
        echo -e "${YELLOW}⚠️  Coverage reporting is not supported on Windows${NC}"
        return
    fi
    
    print_step "Generating coverage report..."
    
    cd build
    cmake --build . --target coverage || print_error "Coverage generation failed"
    cd ..
    
    print_success "Coverage report generated in build/coverage_html/index.html"
}

install_project() {
    print_step "Installing project..."
    
    cd build
    
    if [[ "$PLATFORM" == "Windows" ]]; then
        cmake --build . --target INSTALL || print_error "Installation failed"
    else
        sudo cmake --build . --target install || print_error "Installation failed"
    fi
    
    cd ..
    print_success "Project installed successfully"
}

show_help() {
    echo "Usage: $0 [OPTIONS] [TARGETS]"
    echo
    echo "OPTIONS:"
    echo "  --debug         Build in debug mode"
    echo "  --coverage      Build with coverage instrumentation (Linux/macOS only)"
    echo "  --help, -h      Show this help message"
    echo
    echo "TARGETS:"
    echo "  configure       Configure build system only"
    echo "  build           Build the project (default)"
    echo "  test            Run tests"
    echo "  coverage        Generate coverage report (Linux/macOS only)"
    echo "  install         Install the project"
    echo "  clean           Clean build directory"
    echo "  all             Configure, build, and test"
    echo
    echo "EXAMPLES:"
    echo "  $0                    # Build in release mode"
    echo "  $0 --debug           # Build in debug mode"
    echo "  $0 --coverage test coverage # Build with coverage and generate report"
    echo "  $0 all               # Full build and test cycle"
}

main() {
    # Parse arguments
    local BUILD_TYPE="Release"
    local TARGETS=()
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --debug)
                BUILD_TYPE="Debug"
                shift
                ;;
            --coverage)
                BUILD_TYPE="Coverage"
                shift
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            clean)
                if [ -d "build" ]; then
                    rm -rf build
                    print_success "Build directory cleaned"
                else
                    echo "Build directory doesn't exist"
                fi
                exit 0
                ;;
            configure|build|test|coverage|install|all)
                TARGETS+=("$1")
                shift
                ;;
            *)
                echo "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    # Default to build if no targets specified
    if [ ${#TARGETS[@]} -eq 0 ]; then
        TARGETS=("build")
    fi
    
    print_header
    check_requirements
    
    # Execute targets
    for target in "${TARGETS[@]}"; do
        case $target in
            configure)
                configure_build "$BUILD_TYPE"
                ;;
            build)
                configure_build "$BUILD_TYPE"
                build_project
                ;;
            test)
                if [ ! -d "build" ]; then
                    configure_build "$BUILD_TYPE"
                    build_project
                fi
                run_tests
                ;;
            coverage)
                generate_coverage
                ;;
            install)
                if [ ! -d "build" ]; then
                    configure_build "$BUILD_TYPE"
                    build_project
                fi
                install_project
                ;;
            all)
                configure_build "$BUILD_TYPE"
                build_project
                run_tests
                ;;
        esac
    done
    
    echo
    print_success "Build script completed successfully!"
    
    # Show binary location
    if [ -f "build/ak" ] || [ -f "build/ak.exe" ] || [ -f "build/Release/ak.exe" ] || [ -f "build/Debug/ak.exe" ]; then
        echo
        echo -e "${BLUE}Binary location:${NC}"
        if [[ "$PLATFORM" == "Windows" ]]; then
            if [ -f "build/Release/ak.exe" ]; then
                echo "  build/Release/ak.exe"
            elif [ -f "build/Debug/ak.exe" ]; then
                echo "  build/Debug/ak.exe"
            else
                echo "  build/ak.exe"
            fi
        else
            echo "  build/ak"
        fi
    fi
}

# Run main function with all arguments
main "$@"