#!/bin/bash

# AK macOS Package Creation Script (Linux Compatible)
# Creates compressed packages for macOS distribution using available Linux tools

set -e

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MACOS_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$MACOS_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
OUTPUT_DIR="$PROJECT_ROOT/ak-macos-repo/packages"

# Default values
VERSION="${AK_VERSION:-4.1.5}"
BUNDLE_ID="${AK_BUNDLE_ID:-dev.ak.ak}"
PACKAGE_NAME="AK-${VERSION}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    local missing_tools=()
    
    for tool in 7z; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            missing_tools+=("$tool")
        fi
    done
    
    if [[ ${#missing_tools[@]} -gt 0 ]]; then
        log_error "Missing required tools: ${missing_tools[*]}"
        exit 1
    fi
    
    log_success "Prerequisites check passed"
}

# Setup workspace
setup_workspace() {
    log_info "Setting up workspace..."
    
    WORK_DIR="$(mktemp -d)"
    PACKAGE_STAGING="$WORK_DIR/package-staging"
    MACOS_STAGING="$PACKAGE_STAGING/AK-${VERSION}"
    
    mkdir -p "$PACKAGE_STAGING"
    mkdir -p "$MACOS_STAGING"
    mkdir -p "$OUTPUT_DIR"
    
    log_success "Workspace created at $WORK_DIR"
}

# Create macOS app bundle structure (Linux simulation)
create_app_bundle() {
    log_info "Creating macOS app bundle structure..."
    
    local app_bundle="$MACOS_STAGING/AK.app"
    mkdir -p "$app_bundle/Contents/MacOS"
    mkdir -p "$app_bundle/Contents/Resources"
    
    # Create Info.plist
    cat > "$app_bundle/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDisplayName</key>
    <string>AK</string>
    <key>CFBundleExecutable</key>
    <string>ak</string>
    <key>CFBundleIdentifier</key>
    <string>${BUNDLE_ID}</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>AK</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>$VERSION</string>
    <key>CFBundleVersion</key>
    <string>$VERSION</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.15</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF
    
    # Copy binary
    if [[ -f "$PROJECT_ROOT/ak" ]]; then
        cp "$PROJECT_ROOT/ak" "$app_bundle/Contents/MacOS/"
        chmod +x "$app_bundle/Contents/MacOS/ak"
    else
        log_error "Binary not found: $PROJECT_ROOT/ak"
        exit 1
    fi
    
    # Copy resources
    if [[ -f "$PROJECT_ROOT/logo.png" ]]; then
        cp "$PROJECT_ROOT/logo.png" "$app_bundle/Contents/Resources/"
    fi
    
    log_success "App bundle structure created"
}

# Create Applications symlink (Linux simulation)
create_applications_link() {
    log_info "Creating install instructions..."
    
    # Instead of symlink, create install script
    cat > "$MACOS_STAGING/Install AK.sh" << EOF
#!/bin/bash
# AK Installation Script for macOS

echo "Installing AK..."
cp -r "AK.app" "/Applications/"
echo "✅ AK installed successfully!"
echo "You can now run AK from Applications or use: /Applications/AK.app/Contents/MacOS/ak"
EOF
    chmod +x "$MACOS_STAGING/Install AK.sh"
    
    log_success "Install script created"
}

# Create README
create_readme() {
    log_info "Creating README..."
    
    cat > "$MACOS_STAGING/README.txt" << EOF
AK - API Key Manager v${VERSION}
================================

Installation Instructions:
1. Run "Install AK.sh" to copy AK.app to Applications
   OR
2. Manually drag AK.app to your Applications folder

Command Line Usage:
After installation, you can use AK from the command line:
/Applications/AK.app/Contents/MacOS/ak --help

Features:
- Secure API key storage
- Cross-platform compatibility
- Command-line interface
- Built with C++17

Build Date: $(date '+%Y-%m-%d %H:%M:%S')
Project: https://github.com/ApertaCodex/ak

© 2025 ApertaCodex
EOF
    
    log_success "README created"
}

# Create compressed package
create_compressed_package() {
    log_info "Creating compressed package..."
    
    # Create 7z archive (highly compressed, cross-platform)
    local archive_file="$OUTPUT_DIR/AK-${VERSION}.7z"
    [[ -f "$archive_file" ]] && rm "$archive_file"
    
    cd "$PACKAGE_STAGING"
    7z a -t7z -mx=9 "$archive_file" "AK-${VERSION}/"
    
    # Also create tar.xz for better Unix compatibility
    local tarxz_file="$OUTPUT_DIR/AK-${VERSION}-macos.tar.xz"
    [[ -f "$tarxz_file" ]] && rm "$tarxz_file"
    
    tar -cJf "$tarxz_file" "AK-${VERSION}/"
    
    log_success "Compressed packages created"
    
    # Show file sizes
    if [[ -f "$archive_file" ]]; then
        local size_7z=$(du -h "$archive_file" | cut -f1)
        log_info "7z package size: $size_7z"
    fi
    
    if [[ -f "$tarxz_file" ]]; then
        local size_xz=$(du -h "$tarxz_file" | cut -f1)
        log_info "tar.xz package size: $size_xz"
    fi
}

# Create installation notes
create_install_notes() {
    log_info "Creating installation notes..."
    
    cat > "$OUTPUT_DIR/INSTALL-${VERSION}.md" << EOF
# AK ${VERSION} - macOS Installation

## Package Contents
- \`AK-${VERSION}.7z\` - Highly compressed package (recommended)
- \`AK-${VERSION}-macos.tar.xz\` - Unix-compatible compressed package

## Installation Methods

### Method 1: Extract and Install
1. Extract the package: \`7z x AK-${VERSION}.7z\` or \`tar -xJf AK-${VERSION}-macos.tar.xz\`
2. Run the install script: \`./AK-${VERSION}/Install\ AK.sh\`

### Method 2: Manual Installation
1. Extract the package
2. Copy \`AK-${VERSION}/AK.app\` to your Applications folder

## Usage
After installation:
- Launch from Applications
- Command line: \`/Applications/AK.app/Contents/MacOS/ak --help\`

## Requirements
- macOS 10.15 (Catalina) or later
- 64-bit Intel or Apple Silicon Mac

Built on: $(date '+%Y-%m-%d %H:%M:%S')
EOF
    
    log_success "Installation notes created"
}

# Cleanup function
cleanup() {
    if [[ -n "$WORK_DIR" && -d "$WORK_DIR" ]]; then
        log_info "Cleaning up temporary files..."
        rm -rf "$WORK_DIR"
    fi
}

# Main execution
main() {
    log_info "Starting macOS package creation for AK v$VERSION (Linux build)"
    
    trap cleanup EXIT
    
    check_prerequisites
    setup_workspace
    create_app_bundle
    create_applications_link
    create_readme
    create_compressed_package
    create_install_notes
    
    log_success "macOS packages created successfully!"
    log_info "Output directory: $OUTPUT_DIR"
    log_info "Packages created:"
    ls -la "$OUTPUT_DIR/"*${VERSION}*
}

# Run main function
main "$@"