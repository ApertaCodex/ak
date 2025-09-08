#!/bin/bash

# AK macOS PKG Installer Creation Script
# Creates a professional PKG installer for AK

set -e

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MACOS_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$MACOS_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
INSTALLER_DIR="$MACOS_DIR/installer"
OUTPUT_DIR="$BUILD_DIR/macos-packages"

# Default values (can be overridden by environment variables)
VERSION="${AK_VERSION:-4.2.9}"
BUNDLE_ID="${AK_BUNDLE_ID:-dev.ak.ak}"
SIGNING_IDENTITY="${AK_SIGNING_IDENTITY:-}"
BUILD_DATE="$(date '+%Y-%m-%d %H:%M:%S')"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

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

Create macOS PKG installer for AK

OPTIONS:
    -v, --version VERSION       Set package version (default: $VERSION)
    -b, --bundle-id ID         Set bundle identifier (default: $BUNDLE_ID)
    -s, --sign IDENTITY        Code signing identity
    -o, --output DIR           Output directory (default: $OUTPUT_DIR)
    -h, --help                 Show this help message

ENVIRONMENT VARIABLES:
    AK_VERSION                 Package version
    AK_BUNDLE_ID              Bundle identifier  
    AK_SIGNING_IDENTITY       Code signing identity

EXAMPLES:
    $0                         Create PKG with default settings
    $0 -v 3.4.0 -s "Developer ID" Create signed PKG with version 3.4.0
EOF
}

# Parse command line arguments
parse_args() {
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
            -s|--sign)
                SIGNING_IDENTITY="$2"
                shift 2
                ;;
            -o|--output)
                OUTPUT_DIR="$2"
                shift 2
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

# Validate prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    # Check for required tools
    local missing_tools=()
    
    for tool in pkgbuild productbuild; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            missing_tools+=("$tool")
        fi
    done
    
    if [[ ${#missing_tools[@]} -gt 0 ]]; then
        log_error "Missing required tools: ${missing_tools[*]}"
        log_error "Please install Xcode Command Line Tools: xcode-select --install"
        exit 1
    fi
    
    # Check for build artifacts
    if [[ ! -f "$BUILD_DIR/ak" ]]; then
        log_error "AK CLI executable not found at $BUILD_DIR/ak"
        log_error "Please build the project first using CMake"
        exit 1
    fi
    
    log_success "Prerequisites check passed"
}

# Create temporary working directory
setup_workspace() {
    log_info "Setting up workspace..."
    
    WORK_DIR="$(mktemp -d)"
    PKG_ROOT="$WORK_DIR/pkg-root"
    TEMP_RESOURCES="$WORK_DIR/resources"
    
    mkdir -p "$PKG_ROOT/usr/local/bin"
    mkdir -p "$PKG_ROOT/usr/local/share/ak"
    mkdir -p "$PKG_ROOT/usr/local/share/man/man1"
    mkdir -p "$TEMP_RESOURCES"
    mkdir -p "$OUTPUT_DIR"
    
    log_success "Workspace created at $WORK_DIR"
}

# Prepare installation files
prepare_files() {
    log_info "Preparing installation files..."
    
    # Copy CLI executable
    cp "$BUILD_DIR/ak" "$PKG_ROOT/usr/local/bin/"
    chmod 755 "$PKG_ROOT/usr/local/bin/ak"
    
    # Copy GUI executable if it exists
    if [[ -f "$BUILD_DIR/ak-gui" ]]; then
        cp "$BUILD_DIR/ak-gui" "$PKG_ROOT/usr/local/bin/"
        chmod 755 "$PKG_ROOT/usr/local/bin/ak-gui"
        log_info "GUI executable included"
    fi
    
    # Copy man page if it exists
    if [[ -f "$PROJECT_ROOT/docs/ak.1" ]]; then
        cp "$PROJECT_ROOT/docs/ak.1" "$PKG_ROOT/usr/local/share/man/man1/"
        chmod 644 "$PKG_ROOT/usr/local/share/man/man1/ak.1"
        log_info "Manual page included"
    fi
    
    log_success "Installation files prepared"
}

# Process template files
process_templates() {
    log_info "Processing template files..."
    
    # Process installer resources
    for template in "$INSTALLER_DIR"/{Distribution.xml,resources/Welcome.html}; do
        if [[ -f "$template" ]]; then
            local target="$TEMP_RESOURCES/$(basename "$template")"
            sed -e "s/{{VERSION}}/$VERSION/g" \
                -e "s/{{BUNDLE_ID}}/$BUNDLE_ID/g" \
                -e "s/{{BUILD_DATE}}/$BUILD_DATE/g" \
                "$template" > "$target"
            log_info "Processed $(basename "$template")"
        fi
    done
    
    # Copy other resources
    if [[ -f "$INSTALLER_DIR/resources/LICENSE.txt" ]]; then
        cp "$INSTALLER_DIR/resources/LICENSE.txt" "$TEMP_RESOURCES/"
    fi
    
    log_success "Template processing completed"
}

# Create component packages
create_component_packages() {
    log_info "Creating component packages..."
    
    local cli_pkg="$WORK_DIR/ak-cli.pkg"
    local gui_pkg="$WORK_DIR/ak-gui.pkg"
    
    # Create CLI component package
    pkgbuild \
        --root "$PKG_ROOT" \
        --identifier "${BUNDLE_ID}.cli" \
        --version "$VERSION" \
        --scripts "$INSTALLER_DIR/scripts" \
        "$cli_pkg"
    
    log_success "CLI component package created"
    
    # Create GUI component package (if GUI exists)
    if [[ -f "$PKG_ROOT/usr/local/bin/ak-gui" ]]; then
        # Create separate root for GUI
        local gui_root="$WORK_DIR/gui-root"
        mkdir -p "$gui_root/usr/local/bin"
        cp "$PKG_ROOT/usr/local/bin/ak-gui" "$gui_root/usr/local/bin/"
        
        pkgbuild \
            --root "$gui_root" \
            --identifier "${BUNDLE_ID}.gui" \
            --version "$VERSION" \
            "$gui_pkg"
        
        log_success "GUI component package created"
    fi
}

# Create final distribution package
create_distribution_package() {
    log_info "Creating distribution package..."
    
    local final_pkg="$OUTPUT_DIR/AK-${VERSION}.pkg"
    
    # Build arguments
    local build_args=(
        --distribution "$TEMP_RESOURCES/Distribution.xml"
        --resources "$TEMP_RESOURCES"
        --package-path "$WORK_DIR"
    )
    
    # Add signing if specified
    if [[ -n "$SIGNING_IDENTITY" ]]; then
        build_args+=(--sign "$SIGNING_IDENTITY")
        log_info "Package will be signed with: $SIGNING_IDENTITY"
    fi
    
    build_args+=("$final_pkg")
    
    # Create the distribution package
    productbuild "${build_args[@]}"
    
    log_success "Distribution package created: $(basename "$final_pkg")"
}

# Verify the created package
verify_package() {
    log_info "Verifying package..."
    
    local pkg_file="$OUTPUT_DIR/AK-${VERSION}.pkg"
    
    if [[ ! -f "$pkg_file" ]]; then
        log_error "Package file not found: $pkg_file"
        return 1
    fi
    
    # Check package info
    local pkg_size=$(du -h "$pkg_file" | cut -f1)
    log_info "Package size: $pkg_size"
    
    # Verify package structure
    pkgutil --check-signature "$pkg_file" 2>/dev/null || log_warning "Package is not signed"
    
    log_success "Package verification completed"
}

# Cleanup temporary files
cleanup() {
    if [[ -n "$WORK_DIR" && -d "$WORK_DIR" ]]; then
        log_info "Cleaning up temporary files..."
        rm -rf "$WORK_DIR"
    fi
}

# Main execution
main() {
    log_info "Starting PKG creation for AK v$VERSION"
    
    trap cleanup EXIT
    
    parse_args "$@"
    check_prerequisites
    setup_workspace
    prepare_files
    process_templates
    create_component_packages
    create_distribution_package
    verify_package
    
    log_success "PKG creation completed successfully!"
    log_info "Output: $OUTPUT_DIR/AK-${VERSION}.pkg"
}

# Run main function
main "$@"