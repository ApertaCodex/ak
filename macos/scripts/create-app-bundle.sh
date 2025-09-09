#!/bin/bash

# AK macOS App Bundle Creation Script
# Creates a native macOS application bundle for AK

set -e

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MACOS_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$MACOS_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
APP_BUNDLE_DIR="$MACOS_DIR/app-bundle"
OUTPUT_DIR="$BUILD_DIR/macos-packages"

# Default values
VERSION="${AK_VERSION:-4.2.16}"
BUNDLE_ID="${AK_BUNDLE_ID:-dev.ak.ak}"
BUILD_NUMBER="${AK_BUILD_NUMBER:-1}"
BUILD_DATE="$(date '+%Y-%m-%d %H:%M:%S')"
APP_NAME="AK"

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

# Usage information
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Create macOS App Bundle for AK

OPTIONS:
    -v, --version VERSION       Set app version (default: $VERSION)
    -b, --bundle-id ID         Set bundle identifier (default: $BUNDLE_ID)
    -n, --name NAME            App name (default: $APP_NAME)
    --build-number NUMBER      Build number (default: $BUILD_NUMBER)
    -o, --output DIR           Output directory (default: $OUTPUT_DIR)
    --include-qt               Bundle Qt frameworks (for standalone distribution)
    -h, --help                 Show this help message

ENVIRONMENT VARIABLES:
    AK_VERSION                 App version
    AK_BUNDLE_ID              Bundle identifier
    AK_BUILD_NUMBER           Build number
    QT_DIR                    Qt installation directory

EXAMPLES:
    $0                         Create app bundle with default settings
    $0 --include-qt           Create standalone app bundle with Qt frameworks
    $0 -v 3.4.0 -n "AK Beta"  Create app bundle with version 3.4.0
EOF
}

# Parse command line arguments
parse_args() {
    INCLUDE_QT=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            -v|--version)
                VERSION="$2"
                shift 2
                ;;
            -b|--bundle-id)
                BUNDLE_ID="$2"
                shift 2
                ;;
            -n|--name)
                APP_NAME="$2"
                shift 2
                ;;
            --build-number)
                BUILD_NUMBER="$2"
                shift 2
                ;;
            -o|--output)
                OUTPUT_DIR="$2"
                shift 2
                ;;
            --include-qt)
                INCLUDE_QT=true
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    # Check for GUI executable
    if [[ ! -f "$BUILD_DIR/ak-gui" ]]; then
        log_error "AK GUI executable not found at $BUILD_DIR/ak-gui"
        log_error "Please build the project with GUI support enabled"
        exit 1
    fi
    
    # Check for CLI executable
    if [[ ! -f "$BUILD_DIR/ak" ]]; then
        log_warning "AK CLI executable not found, app bundle will only contain GUI"
    fi
    
    log_success "Prerequisites check passed"
}

# Setup workspace
setup_workspace() {
    log_info "Setting up workspace..."
    
    APP_BUNDLE="$OUTPUT_DIR/${APP_NAME}.app"
    CONTENTS_DIR="$APP_BUNDLE/Contents"
    MACOS_BIN_DIR="$CONTENTS_DIR/MacOS"
    RESOURCES_DIR="$CONTENTS_DIR/Resources"
    FRAMEWORKS_DIR="$CONTENTS_DIR/Frameworks"
    
    # Remove existing bundle
    [[ -d "$APP_BUNDLE" ]] && rm -rf "$APP_BUNDLE"
    
    # Create bundle structure
    mkdir -p "$OUTPUT_DIR"
    mkdir -p "$MACOS_BIN_DIR"
    mkdir -p "$RESOURCES_DIR"
    
    if [[ "$INCLUDE_QT" == "true" ]]; then
        mkdir -p "$FRAMEWORKS_DIR"
    fi
    
    log_success "Workspace created"
}

# Create Info.plist
create_info_plist() {
    log_info "Creating Info.plist..."
    
    local info_plist="$CONTENTS_DIR/Info.plist"
    
    # Process template
    sed -e "s/{{VERSION}}/$VERSION/g" \
        -e "s/{{BUNDLE_ID}}/$BUNDLE_ID/g" \
        -e "s/{{BUILD_NUMBER}}/$BUILD_NUMBER/g" \
        "$APP_BUNDLE_DIR/Info.plist" > "$info_plist"
    
    # Validate plist
    plutil -lint "$info_plist" || {
        log_error "Invalid Info.plist created"
        exit 1
    }
    
    log_success "Info.plist created"
}

# Copy executables
copy_executables() {
    log_info "Copying executables..."
    
    # Copy GUI executable
    cp "$BUILD_DIR/ak-gui" "$MACOS_BIN_DIR/"
    chmod +x "$MACOS_BIN_DIR/ak-gui"
    
    # Copy CLI executable if it exists
    if [[ -f "$BUILD_DIR/ak" ]]; then
        cp "$BUILD_DIR/ak" "$MACOS_BIN_DIR/"
        chmod +x "$MACOS_BIN_DIR/ak"
        log_info "CLI executable included"
    fi
    
    # Create launcher script
    local launcher="$MACOS_BIN_DIR/launcher"
    sed -e "s/{{VERSION}}/$VERSION/g" \
        -e "s/{{BUNDLE_ID}}/$BUNDLE_ID/g" \
        "$APP_BUNDLE_DIR/launcher.sh" > "$launcher"
    chmod +x "$launcher"
    
    log_success "Executables copied"
}

# Copy resources
copy_resources() {
    log_info "Copying resources..."
    
    # Copy icon if it exists
    if [[ -f "$PROJECT_ROOT/logo.png" ]]; then
        # Convert PNG to ICNS if possible
        if command -v iconutil >/dev/null 2>&1; then
            create_icns_from_png "$PROJECT_ROOT/logo.png" "$RESOURCES_DIR/ak.icns"
        else
            cp "$PROJECT_ROOT/logo.png" "$RESOURCES_DIR/ak.png"
            log_warning "iconutil not found, using PNG instead of ICNS"
        fi
    else
        log_warning "Logo not found at $PROJECT_ROOT/logo.png"
    fi
    
    # Copy documentation
    if [[ -f "$PROJECT_ROOT/README.md" ]]; then
        cp "$PROJECT_ROOT/README.md" "$RESOURCES_DIR/"
    fi
    
    if [[ -f "$PROJECT_ROOT/LICENSE" ]]; then
        cp "$PROJECT_ROOT/LICENSE" "$RESOURCES_DIR/"
    fi
    
    if [[ -f "$PROJECT_ROOT/docs/ak.1" ]]; then
        cp "$PROJECT_ROOT/docs/ak.1" "$RESOURCES_DIR/"
    fi
    
    log_success "Resources copied"
}

# Create ICNS from PNG
create_icns_from_png() {
    local png_file="$1"
    local icns_file="$2"
    
    log_info "Creating ICNS from PNG..."
    
    local temp_iconset="${icns_file%.icns}.iconset"
    mkdir -p "$temp_iconset"
    
    # Generate different sizes
    local sizes=(16 32 64 128 256 512 1024)
    for size in "${sizes[@]}"; do
        sips -z $size $size "$png_file" --out "$temp_iconset/icon_${size}x${size}.png" >/dev/null 2>&1
        
        # Create @2x versions for retina
        if [[ $size -le 512 ]]; then
            local retina_size=$((size * 2))
            sips -z $retina_size $retina_size "$png_file" --out "$temp_iconset/icon_${size}x${size}@2x.png" >/dev/null 2>&1
        fi
    done
    
    # Convert to ICNS
    iconutil -c icns "$temp_iconset" -o "$icns_file"
    rm -rf "$temp_iconset"
    
    log_success "ICNS created"
}

# Bundle Qt frameworks
bundle_qt_frameworks() {
    if [[ "$INCLUDE_QT" != "true" ]]; then
        return 0
    fi
    
    log_info "Bundling Qt frameworks..."
    
    # Find Qt installation
    local qt_dir="${QT_DIR:-}"
    if [[ -z "$qt_dir" ]]; then
        # Try to find Qt using qmake
        if command -v qmake >/dev/null 2>&1; then
            qt_dir="$(qmake -query QT_INSTALL_PREFIX)"
        else
            log_warning "Qt directory not found, skipping framework bundling"
            return 0
        fi
    fi
    
    local qt_libs_dir="$qt_dir/lib"
    if [[ ! -d "$qt_libs_dir" ]]; then
        log_warning "Qt libraries directory not found: $qt_libs_dir"
        return 0
    fi
    
    # Find required Qt frameworks
    local required_frameworks=($(otool -L "$MACOS_BIN_DIR/ak-gui" | grep -o "$qt_libs_dir/[^[:space:]]*.framework[^[:space:]]*" | sed 's|.*/\([^/]*\)\.framework.*|\1|' | sort -u))
    
    log_info "Found ${#required_frameworks[@]} Qt frameworks to bundle"
    
    # Copy frameworks
    for framework in "${required_frameworks[@]}"; do
        local src_framework="$qt_libs_dir/${framework}.framework"
        local dst_framework="$FRAMEWORKS_DIR/${framework}.framework"
        
        if [[ -d "$src_framework" ]]; then
            cp -R "$src_framework" "$dst_framework"
            log_info "Bundled framework: $framework"
        else
            log_warning "Framework not found: $src_framework"
        fi
    done
    
    # Fix library paths using install_name_tool
    fix_library_paths
    
    log_success "Qt frameworks bundled"
}

# Fix library paths for bundled frameworks
fix_library_paths() {
    log_info "Fixing library paths..."
    
    # Fix executable
    local executable="$MACOS_BIN_DIR/ak-gui"
    local frameworks=($(find "$FRAMEWORKS_DIR" -name "*.framework" -type d | xargs -I {} basename {} .framework))
    
    for framework in "${frameworks[@]}"; do
        local old_path="@rpath/${framework}.framework/${framework}"
        local new_path="@executable_path/../Frameworks/${framework}.framework/${framework}"
        
        install_name_tool -change "$old_path" "$new_path" "$executable" 2>/dev/null || true
    done
    
    # Fix framework dependencies
    for framework_dir in "$FRAMEWORKS_DIR"/*.framework; do
        if [[ -d "$framework_dir" ]]; then
            local framework=$(basename "$framework_dir" .framework)
            local framework_binary="$framework_dir/$framework"
            
            if [[ -f "$framework_binary" ]]; then
                # Fix the framework's own ID
                install_name_tool -id "@executable_path/../Frameworks/${framework}.framework/${framework}" "$framework_binary" 2>/dev/null || true
                
                # Fix dependencies
                for dep_framework in "${frameworks[@]}"; do
                    if [[ "$dep_framework" != "$framework" ]]; then
                        local old_dep_path="@rpath/${dep_framework}.framework/${dep_framework}"
                        local new_dep_path="@executable_path/../Frameworks/${dep_framework}.framework/${dep_framework}"
                        install_name_tool -change "$old_dep_path" "$new_dep_path" "$framework_binary" 2>/dev/null || true
                    fi
                done
            fi
        fi
    done
    
    log_success "Library paths fixed"
}

# Set bundle permissions
set_permissions() {
    log_info "Setting bundle permissions..."
    
    # Set proper permissions
    find "$APP_BUNDLE" -type d -exec chmod 755 {} \;
    find "$APP_BUNDLE" -type f -exec chmod 644 {} \;
    
    # Make executables executable
    find "$MACOS_BIN_DIR" -type f -exec chmod +x {} \;
    
    # Make frameworks executable
    if [[ -d "$FRAMEWORKS_DIR" ]]; then
        find "$FRAMEWORKS_DIR" -name "*.framework" -type d | while read -r framework_dir; do
            local framework=$(basename "$framework_dir" .framework)
            local framework_binary="$framework_dir/$framework"
            [[ -f "$framework_binary" ]] && chmod +x "$framework_binary"
        done
    fi
    
    log_success "Permissions set"
}

# Verify app bundle
verify_bundle() {
    log_info "Verifying app bundle..."
    
    # Check bundle structure
    if [[ ! -f "$CONTENTS_DIR/Info.plist" ]]; then
        log_error "Info.plist missing"
        exit 1
    fi
    
    if [[ ! -f "$MACOS_BIN_DIR/ak-gui" ]]; then
        log_error "GUI executable missing"
        exit 1
    fi
    
    # Test executable
    if ! "$MACOS_BIN_DIR/ak-gui" --version >/dev/null 2>&1; then
        log_warning "GUI executable test failed (may be expected if Qt not available)"
    fi
    
    # Calculate bundle size
    local bundle_size=$(du -sh "$APP_BUNDLE" | cut -f1)
    log_info "Bundle size: $bundle_size"
    
    log_success "App bundle verification completed"
}

# Main execution
main() {
    log_info "Starting app bundle creation for AK v$VERSION"
    
    parse_args "$@"
    check_prerequisites
    setup_workspace
    create_info_plist
    copy_executables
    copy_resources
    bundle_qt_frameworks
    set_permissions
    verify_bundle
    
    log_success "App bundle creation completed successfully!"
    log_info "Output: $APP_BUNDLE"
}

# Run main function
main "$@"