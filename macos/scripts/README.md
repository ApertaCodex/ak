# macOS Packaging Scripts

This directory contains the core automation scripts for creating professional macOS packages for AK.

## Script Overview

### Core Packaging Scripts
- **`create-app-bundle.sh`** - Creates native macOS application bundles (.app)
- **`create-pkg.sh`** - Creates PKG installer packages (.pkg)
- **`create-dmg.sh`** - Creates DMG disk images (.dmg)

### Utility Scripts
- **`package-all.sh`** - Master script to create all package types
- **`validate-packages.sh`** - Validates created packages for compliance and common issues

## Quick Start

### Create All Packages
```bash
# Create all package types with default settings
./scripts/package-all.sh

# Create specific package types
./scripts/package-all.sh app dmg

# Create signed and notarized packages
./scripts/package-all.sh --sign "Developer ID Application" --notarize
```

### Individual Package Creation
```bash
# App bundle
./scripts/create-app-bundle.sh --version 3.3.0

# PKG installer
./scripts/create-pkg.sh --version 3.3.0 --sign "Developer ID Installer"

# DMG disk image
./scripts/create-dmg.sh --version 3.3.0
```

### Package Validation
```bash
# Validate all packages in output directory
./scripts/validate-packages.sh

# Validate packages in custom directory
./scripts/validate-packages.sh /path/to/packages
```

## Environment Variables

Set these environment variables to customize the build:

### Required Variables
- **`AK_VERSION`** - Application version (e.g., "3.3.0")
- **`AK_BUNDLE_ID`** - Bundle identifier (e.g., "dev.ak.ak")

### Optional Variables
- **`AK_SIGNING_IDENTITY`** - Code signing identity
- **`AK_NOTARIZE`** - Enable notarization (true/false)
- **`AK_INCLUDE_QT`** - Bundle Qt frameworks (true/false)
- **`QT_DIR`** - Qt installation directory
- **`APPLE_ID`** - Apple ID for notarization
- **`APP_PASSWORD`** - App-specific password for notarization

### Example Configuration
```bash
export AK_VERSION="3.4.0"
export AK_BUNDLE_ID="dev.ak.ak"
export AK_SIGNING_IDENTITY="Developer ID Application: Your Name (ABC123DEF4)"
export AK_NOTARIZE=true
export AK_INCLUDE_QT=true

./scripts/package-all.sh
```

## Script Details

### create-app-bundle.sh
Creates native macOS application bundles with:
- Proper Info.plist configuration
- Executable launching scripts
- Resource bundling (icons, documentation)
- Qt framework bundling (optional)
- Code signing support

**Usage:**
```bash
./create-app-bundle.sh [OPTIONS]
  -v, --version VERSION      App version
  -b, --bundle-id ID         Bundle identifier
  --include-qt              Bundle Qt frameworks
  -o, --output DIR          Output directory
```

### create-pkg.sh
Creates professional PKG installers with:
- Distribution definition with component packages
- Pre/post-install scripts
- Custom installer appearance
- Code signing support
- System requirements validation

**Usage:**
```bash
./create-pkg.sh [OPTIONS]
  -v, --version VERSION      Package version
  -s, --sign IDENTITY       Signing identity
  -o, --output DIR          Output directory
```

### create-dmg.sh
Creates DMG disk images with:
- Custom background and layout
- Drag-and-drop installation interface
- Applications symlink
- Uninstaller application
- Compression optimization

**Usage:**
```bash
./create-dmg.sh [OPTIONS]
  -v, --version VERSION      DMG version
  --app-bundle PATH         Path to app bundle
  -o, --output DIR          Output directory
```

### package-all.sh
Master packaging script that:
- Orchestrates all packaging operations
- Handles dependencies between package types
- Provides unified configuration
- Supports selective package creation
- Generates comprehensive build reports

**Usage:**
```bash
./package-all.sh [OPTIONS] [TYPES...]
  --clean                   Clean output directory
  --include-qt             Bundle Qt frameworks
  -s, --sign IDENTITY      Sign all packages
  -n, --notarize          Enable notarization
```

### validate-packages.sh
Comprehensive validation tool that checks:
- App bundle structure and Info.plist format
- PKG installer integrity and signatures
- DMG format and mounting capability
- Code signing status
- File permissions and dependencies
- System requirements compliance

**Usage:**
```bash
./validate-packages.sh [OUTPUT_DIR]
```

## Prerequisites

### System Requirements
- **macOS**: 10.15 (Catalina) or later for building
- **Xcode Command Line Tools**: Required for packaging tools
- **CMake**: 3.16+ for building the application
- **Qt6**: Required if GUI support is enabled

### Install Prerequisites
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Verify installation
pkgbuild --version
productbuild --version
hdiutil --version
codesign --version
```

### Build AK First
Before running packaging scripts:
```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the application
make -j$(nproc)
```

## Code Signing and Notarization

### Developer Account Setup
1. Enroll in Apple Developer Program
2. Create Developer ID certificates in Keychain Access
3. Generate app-specific password for notarization

### Signing Process
```bash
# List available signing identities
security find-identity -v -p codesigning

# Sign and notarize packages
./scripts/package-all.sh \
    --sign "Developer ID Application: Your Name" \
    --notarize
```

### Notarization Requirements
- Valid Developer ID certificate
- Apple ID with app-specific password
- Hardened runtime enabled
- Secure timestamp enabled

## Output Structure

Packages are created in `build/macos-packages/`:
```
build/macos-packages/
├── AK.app                    # App bundle
├── AK-3.3.0.pkg             # PKG installer
├── AK-3.3.0.dmg             # DMG disk image
├── package-summary.txt      # Build summary
└── validation-report.txt    # Validation results
```

## Troubleshooting

### Common Issues

**Build artifacts not found:**
- Ensure project is built with CMake first
- Check `build/` directory contains `ak` and `ak-gui` executables

**Code signing failures:**
- Verify Developer ID certificates are installed in Keychain
- Use `security find-identity -v` to list available identities
- Check certificate is not expired

**Qt framework issues:**
- Set `QT_DIR` environment variable to Qt installation
- Ensure Qt6 is properly installed
- Use `--include-qt` flag for standalone distribution

**Notarization problems:**
- Verify Apple ID and app-specific password
- Check Developer Program enrollment status
- Ensure internet connection for notarization service

### Debug Mode
Add `set -x` to any script for detailed execution tracing:
```bash
# Enable debug output
export BASH_XTRACEFD=1
./scripts/create-app-bundle.sh --version 3.3.0
```

## Integration with Build Systems

### CMake Integration
Add to `CMakeLists.txt`:
```cmake
# macOS packaging targets (example for future integration)
if(APPLE)
    add_custom_target(macos-packages
        COMMAND ${CMAKE_SOURCE_DIR}/macos/scripts/package-all.sh
        DEPENDS ak
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()
```

### CI/CD Integration
Example GitHub Actions workflow:
```yaml
- name: Create macOS Packages
  if: runner.os == 'macOS'
  run: |
    export AK_VERSION="${{ github.ref_name }}"
    ./macos/scripts/package-all.sh
  
- name: Validate Packages
  if: runner.os == 'macOS'
  run: ./macos/scripts/validate-packages.sh