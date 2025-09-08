# AK - API Key Manager

A cross-platform C++17 command-line tool for managing API keys securely across different services.

## 🚀 Features

- **Cross-Platform**: Runs on Linux, macOS, and Windows
- **Secure Storage**: Encrypted API key storage with local vault
- **Service Integration**: Built-in support for popular APIs (OpenAI, GitHub, AWS, etc.)
- **Test Coverage**: Comprehensive test suite with coverage reporting
- **Easy Installation**: Simple build and install process
- **Shell Completions**: Bash, Zsh, and Fish shell completions

## 📦 Installation

### APT Repository (Ubuntu/Debian) - Recommended ✅

> **Note:** Use the GitHub Pages repository below. The Launchpad PPA is currently experiencing access issues.

#### Quick Install (Recommended)
```bash
curl -fsSL https://apertacodex.github.io/ak/setup-repository.sh | bash
sudo apt install ak
```

#### Manual Setup
```bash
# Add GPG key (secure method)
sudo mkdir -p /usr/share/keyrings
curl -fsSL https://apertacodex.github.io/ak/ak-repository-key.gpg | sudo tee /usr/share/keyrings/ak-archive-keyring.gpg > /dev/null

# Add GitHub Pages repository
echo "deb [signed-by=/usr/share/keyrings/ak-archive-keyring.gpg] https://apertacodex.github.io/ak/ak-apt-repo/ stable main" | sudo tee /etc/apt/sources.list.d/ak.list

# Update and install
sudo apt update && sudo apt install ak
```

#### Direct Package Download
```bash
# Download latest .deb package (check for newest version)
wget https://apertacodex.github.io/ak/ak-apt-repo/pool/main/ak_4.2.10_amd64.deb
sudo dpkg -i ak_4.2.10_amd64.deb

# Fix dependencies if needed
sudo apt-get install -f
```

#### ⚠️ PPA Alternative (Currently Unavailable)
```bash
# NOTE: Launchpad PPA currently returns 403 errors
# Use GitHub Pages repository above instead
# sudo add-apt-repository ppa:apertacodex/ak  # ← Don't use this
```

#### Verification
```bash
# Check version
ak --version

# Test functionality
ak --help

# Shell completions work after restart
ak --<TAB><TAB>
```

## 🔧 Building from Source

### Prerequisites

- **CMake 3.16+** 
- **C++17 compatible compiler** (GCC 7+, Clang 8+, MSVC 2019+)
- **Git** (for dependencies and version management)

### Platform-Specific Setup

#### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install build-essential cmake git
```

#### macOS
```bash
# Using Homebrew
brew install cmake git
# Xcode command line tools
xcode-select --install
```

#### Windows
- Install [Visual Studio 2019+](https://visualstudio.microsoft.com/) with C++ tools
- Install [CMake](https://cmake.org/download/) 
- Install [Git for Windows](https://git-scm.com/download/win)

## 🔧 Building

### Quick Build (All Platforms)

#### Linux/macOS:
```bash
./build.sh
```

#### Windows (PowerShell):
```powershell
.\build.ps1
```

### Manual CMake Build

```bash
# Configure
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --parallel

# Test
./ak_tests

# Install (Linux/macOS)
sudo cmake --build . --target install
```

### Build Options

#### Linux/macOS Build Script:
```bash
./build.sh --help                    # Show help
./build.sh                           # Release build
./build.sh --debug                   # Debug build
./build.sh --coverage test coverage  # Coverage build with report
./build.sh all                       # Full build and test cycle
./build.sh clean                     # Clean build directory
```

#### Windows PowerShell:
```powershell
.\build.ps1 -Help                     # Show help
.\build.ps1                          # Release build
.\build.ps1 -Debug                   # Debug build
.\build.ps1 -Targets test            # Run tests only
.\build.ps1 -Targets all             # Full build and test cycle
.\build.ps1 -Targets clean           # Clean build directory
```

#### CMake Targets:
```bash
# Basic targets
cmake --build . --target ak          # Build main binary
cmake --build . --target ak_tests    # Build tests
cmake --build . --target run_tests   # Build and run tests

# Coverage (Linux/macOS only)
cmake --build . --target coverage    # Generate coverage report

# Publishing (requires git repository)
cmake --build . --target bump-patch  # Bump patch version
cmake --build . --target bump-minor  # Bump minor version
cmake --build . --target bump-major  # Bump major version
cmake --build . --target publish     # Publish patch release
```

## 🧪 Testing

The project includes a comprehensive test suite with 65+ tests covering all major functionality.

### Run Tests
```bash
# Using build scripts
./build.sh test                       # Linux/macOS
.\build.ps1 -Targets test            # Windows

# Using CMake directly
cd build
cmake --build . --target run_tests   # Build and run
./ak_tests                           # Run tests directly (Linux/macOS)
ak_tests.exe                         # Run tests directly (Windows)
```

### Coverage Report (Linux/macOS)
```bash
./build.sh --coverage test coverage
# Report generated in build/coverage_html/index.html
```

## 📋 Usage Examples

```bash
# Basic usage
ak --version
ak --help

# Key management
ak add mykey "sk-..." 
ak list
ak get mykey
ak remove mykey

# Service testing
ak test openai
ak test github
ak test --all

# Shell completions
ak completion bash > /etc/bash_completion.d/ak  # Bash
ak completion zsh > /usr/share/zsh/site-functions/_ak  # Zsh
ak completion fish > ~/.config/fish/completions/ak.fish  # Fish
```

## 🏗️ Development

### Project Structure
```
ak/
├── include/           # Header files
│   ├── cli/          # CLI interface
│   ├── core/         # Core configuration
│   ├── crypto/       # Cryptographic functions
│   ├── services/     # Service definitions
│   └── ...
├── src/              # Source files
│   ├── cli/
│   ├── core/
│   ├── crypto/
│   └── ...
├── tests/            # Test files
├── cmake/            # CMake helper scripts
├── CMakeLists.txt    # Main CMake configuration
├── build.sh          # Unix build script
├── build.ps1         # Windows build script
└── Makefile          # Legacy Make support
```

### Development Workflow

1. **Setup Development Environment**:
   ```bash
   git clone <repository>
   cd ak
   ./build.sh --debug test  # Build in debug mode with tests
   ```

2. **Make Changes**: Edit source files in `src/` and headers in `include/`

3. **Run Tests**: 
   ```bash
   ./build.sh test
   ```

4. **Check Coverage** (Linux/macOS):
   ```bash
   ./build.sh --coverage test coverage
   ```

5. **Create Release**:
   ```bash
   cmake --build build --target publish        # Patch release
   cmake --build build --target publish-minor  # Minor release
   cmake --build build --target publish-major  # Major release
   ```

### Adding New Tests

Tests use Google Test framework. Add test files in the appropriate `tests/` subdirectory:

```cpp
#include <gtest/gtest.h>
#include "your/module.hpp"

TEST(ModuleTest, BasicFunctionality) {
    // Your test code
    EXPECT_EQ(expected, actual);
}
```

## 🎯 Supported Platforms

| Platform | Architecture | Compiler | Status |
|----------|-------------|----------|--------|
| Linux    | x86_64      | GCC 7+   | ✅ Fully Supported |
| Linux    | x86_64      | Clang 8+ | ✅ Fully Supported |
| macOS    | x86_64      | Clang    | ✅ Fully Supported |
| macOS    | ARM64       | Clang    | ✅ Fully Supported |
| Windows  | x86_64      | MSVC 2019+ | ✅ Fully Supported |
| Windows  | x86_64      | MinGW    | ⚠️ Basic Support |

## 📊 Current Status

- **Version**: 4.2.10
- **Test Coverage**: ~60% line coverage, ~67% function coverage
- **Tests**: 65 tests across 21 test suites
- **Build System**: CMake 3.16+ with cross-platform support
- **APT Repository**: Available for Ubuntu/Debian systems

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests: `./build.sh test`
5. Submit a pull request

## 📄 License

This project is open source and licensed under the MIT License with Attribution Requirement.

**Attribution Required:** When integrating AK into other systems or creating derivative works, please mention ApertaCodex as the original creator.

See the [LICENSE](LICENSE:1) file for full details.

## 🔧 Troubleshooting

### Common Issues

#### CMake Version Too Old
```bash
# Ubuntu/Debian - install newer CMake
sudo apt remove cmake
pip3 install cmake

# macOS - update via Homebrew  
brew upgrade cmake
```

#### Missing C++17 Support
```bash
# Ubuntu/Debian - install GCC 7+
sudo apt install gcc-7 g++-7

# Update alternatives
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-7 60
```

#### Windows Build Issues
- Ensure Visual Studio 2019+ is installed with C++ tools
- Run build from "Developer Command Prompt for VS"
- Use PowerShell (not Command Prompt) for build scripts

### Build Logs
- Linux/macOS: Check console output
- Windows: Check `build/CMakeFiles/CMakeError.log` for detailed errors

## 📞 Support

- Issues: [GitHub Issues](https://github.com/apertacodex/ak/issues)
- Documentation: This README and inline code comments
- Build Problems: Check troubleshooting section above