#!/bin/bash

# AK macOS Master Packaging Script
# Creates all macOS package types (App Bundle, PKG, DMG)

set -e

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MACOS_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$MACOS_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
OUTPUT_DIR="$BUILD_DIR/macos-packages"

# Default values
VERSION="${AK_VERSION:-4.2.16}"
BUNDLE_ID="${AK_BUNDLE_ID:-dev.ak.ak}"
SIGNING_IDENTITY="${AK_SIGNING_IDENTITY:-}"
NOTARIZE="${AK_NOTARIZE:-false}"
INCLUDE_QT="${AK_INCLUDE_QT:-false}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
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

log_step() {
    echo -e "${CYAN}[STEP]${NC} $1"
}

# Usage information
usage() {
    cat << EOF
Usage: $0 [OPTIONS] [PACKAGE_TYPES...]

Create all macOS packages for AK

PACKAGE TYPES:
    app                        Create app bundle only
    pkg                        Create PKG installer only  
    dmg                        Create DMG disk image only
    all                        Create all package types (default)

OPTIONS:
    -v, --version VERSION      Set package version (default: $VERSION)
    -b, --bundle-id ID         Set bundle identifier (default: $BUNDLE_ID)
    -s, --sign IDENTITY        Code signing identity
    -n, --notarize            Enable notarization (requires signing)
    --include-qt              Bundle Qt frameworks with app
    -o, --output DIR          Output directory (default: $OUTPUT_DIR)
    --clean                   Clean output directory before building
    -h, --help                Show this help message

ENVIRONMENT VARIABLES:
    AK_VERSION                Package version
    AK_BUNDLE_ID             Bundle identifier
    AK_SIGNING_IDENTITY      Code signing identity
    AK_NOTARIZE              Enable notarization (true/false)
    AK_INCLUDE_QT           Include Qt frameworks (true/false)

EXAMPLES:
    $0                        Create all package types
    $0 app dmg               Create app bundle and DMG only
    $0 -s "Developer ID" --notarize  Create signed and notarized packages
    $0 --include-qt app      Create standalone app bundle with Qt
EOF
}

# Parse command line arguments
parse_args() {
    PACKAGE_TYPES=()
    CLEAN_OUTPUT=false
    
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
            -n|--notarize)
                NOTARIZE=true
                shift
                ;;
            --include-qt)
                INCLUDE_QT=true
                shift
                ;;
            -o|--output)
                OUTPUT_DIR="$2"
                shift 2
                ;;
            --clean)
                CLEAN_OUTPUT=true
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            app|pkg|dmg|all)
                PACKAGE_TYPES+=("$1")
                shift
                ;;
            *)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done
    
    # Default to all package types if none specified
    if [[ ${#PACKAGE_TYPES[@]} -eq 0 ]]; then
        PACKAGE_TYPES=("all")
    fi
    
    # Expand "all" to individual types
    if [[ " ${PACKAGE_TYPES[*]} " =~ " all " ]]; then
        PACKAGE_TYPES=("app" "pkg" "dmg")
    fi
}

# Validate configuration
validate_config() {
    log_info "Validating configuration..."
    
    # Check if notarization requires signing
    if [[ "$NOTARIZE" == "true" && -z "$SIGNING_IDENTITY" ]]; then
        log_error "Notarization requires code signing. Please specify --sign option."
        exit 1
    fi
    
    # Check for build artifacts
    if [[ ! -f "$BUILD_DIR/ak" && ! -f "$BUILD_DIR/ak-gui" ]]; then
        log_error "No build artifacts found. Please build the project first."
        exit 1
    fi
    
    log_success "Configuration validated"
}

# Setup output directory
setup_output_dir() {
    log_info "Setting up output directory..."
    
    if [[ "$CLEAN_OUTPUT" == "true" && -d "$OUTPUT_DIR" ]]; then
        log_info "Cleaning output directory"
        rm -rf "$OUTPUT_DIR"
    fi
    
    mkdir -p "$OUTPUT_DIR"
    
    log_success "Output directory ready: $OUTPUT_DIR"
}

# Create app bundle
create_app_bundle() {
    log_step "Creating app bundle..."
    
    local args=(
        --version "$VERSION"
        --bundle-id "$BUNDLE_ID"
        --output "$OUTPUT_DIR"
    )
    
    if [[ "$INCLUDE_QT" == "true" ]]; then
        args+=(--include-qt)
    fi
    
    "$SCRIPT_DIR/create-app-bundle.sh" "${args[@]}"
    
    # Sign app bundle if identity provided
    if [[ -n "$SIGNING_IDENTITY" ]]; then
        sign_app_bundle "$OUTPUT_DIR/AK.app"
    fi
    
    log_success "App bundle created"
}

# Create PKG installer
create_pkg_installer() {
    log_step "Creating PKG installer..."
    
    local args=(
        --version "$VERSION"
        --bundle-id "$BUNDLE_ID"
        --output "$OUTPUT_DIR"
    )
    
    if [[ -n "$SIGNING_IDENTITY" ]]; then
        args+=(--sign "$SIGNING_IDENTITY")
    fi
    
    "$SCRIPT_DIR/create-pkg.sh" "${args[@]}"
    
    log_success "PKG installer created"
}

# Create DMG disk image
create_dmg_image() {
    log_step "Creating DMG disk image..."
    
    # Use existing app bundle if available
    local app_bundle="$OUTPUT_DIR/AK.app"
    local args=(
        --version "$VERSION"
        --output "$OUTPUT_DIR"
    )
    
    if [[ -d "$app_bundle" ]]; then
        args+=(--app-bundle "$app_bundle")
    fi
    
    "$SCRIPT_DIR/create-dmg.sh" "${args[@]}"
    
    # Sign DMG if identity provided
    if [[ -n "$SIGNING_IDENTITY" ]]; then
        sign_dmg "$OUTPUT_DIR/AK-${VERSION}.dmg"
    fi
    
    log_success "DMG disk image created"
}

# Code signing for app bundle
sign_app_bundle() {
    local app_bundle="$1"
    log_info "Signing app bundle: $(basename "$app_bundle")"
    
    # Sign frameworks first (if any)
    if [[ -d "$app_bundle/Contents/Frameworks" ]]; then
        find "$app_bundle/Contents/Frameworks" -name "*.framework" -type d | while read -r framework; do
            codesign --sign "$SIGNING_IDENTITY" --verbose --force --options runtime "$framework"
        done
    fi
    
    # Sign executables
    find "$app_bundle/Contents/MacOS" -type f -perm +111 | while read -r executable; do
        codesign --sign "$SIGNING_IDENTITY" --verbose --force --options runtime "$executable"
    done
    
    # Sign the entire bundle
    codesign --sign "$SIGNING_IDENTITY" --verbose --force --options runtime "$app_bundle"
    
    # Verify signature
    codesign --verify --verbose=2 "$app_bundle"
    
    log_success "App bundle signed successfully"
}

# Code signing for DMG
sign_dmg() {
    local dmg_file="$1"
    log_info "Signing DMG: $(basename "$dmg_file")"
    
    codesign --sign "$SIGNING_IDENTITY" --verbose --force "$dmg_file"
    codesign --verify --verbose=2 "$dmg_file"
    
    log_success "DMG signed successfully"
}

# Notarize packages
notarize_packages() {
    if [[ "$NOTARIZE" != "true" ]]; then
        return 0
    fi
    
    log_step "Notarizing packages..."
    
    # Check for notarization tool
    if ! command -v xcrun >/dev/null 2>&1; then
        log_warning "xcrun not found, skipping notarization"
        return 0
    fi
    
    # Notarize app bundle
    local app_bundle="$OUTPUT_DIR/AK.app"
    if [[ -d "$app_bundle" && " ${PACKAGE_TYPES[*]} " =~ " app " ]]; then
        notarize_file "$app_bundle"
    fi
    
    # Notarize DMG
    local dmg_file="$OUTPUT_DIR/AK-${VERSION}.dmg"
    if [[ -f "$dmg_file" && " ${PACKAGE_TYPES[*]} " =~ " dmg " ]]; then
        notarize_file "$dmg_file"
    fi
    
    log_success "Notarization completed"
}

# Notarize individual file
notarize_file() {
    local file_path="$1"
    local filename=$(basename "$file_path")
    
    log_info "Notarizing: $filename"
    
    # Submit for notarization
    local request_uuid=$(xcrun altool --notarize-app \
        --primary-bundle-id "$BUNDLE_ID" \
        --username "$APPLE_ID" \
        --password "$APP_PASSWORD" \
        --file "$file_path" \
        2>/dev/null | grep "RequestUUID" | awk '{print $3}')
    
    if [[ -n "$request_uuid" ]]; then
        log_info "Notarization request submitted: $request_uuid"
        log_info "Check status with: xcrun altool --notarization-info $request_uuid --username \$APPLE_ID --password \$APP_PASSWORD"
    else
        log_warning "Notarization submission failed for $filename"
    fi
}

# Generate summary report
generate_summary() {
    log_step "Generating package summary..."
    
    local summary_file="$OUTPUT_DIR/package-summary.txt"
    
    cat > "$summary_file" << EOF
AK macOS Package Build Summary
==============================

Build Information:
- Version: $VERSION
- Bundle ID: $BUNDLE_ID
- Build Date: $(date '+%Y-%m-%d %H:%M:%S')
- Signed: $(if [[ -n "$SIGNING_IDENTITY" ]]; then echo "Yes ($SIGNING_IDENTITY)"; else echo "No"; fi)
- Notarized: $(if [[ "$NOTARIZE" == "true" ]]; then echo "Yes"; else echo "No"; fi)
- Qt Bundled: $(if [[ "$INCLUDE_QT" == "true" ]]; then echo "Yes"; else echo "No"; fi)

Package Types Created:
$(for type in "${PACKAGE_TYPES[@]}"; do echo "- $type"; done)

Output Files:
EOF
    
    # List created files
    find "$OUTPUT_DIR" -maxdepth 1 \( -name "*.app" -o -name "*.pkg" -o -name "*.dmg" \) | while read -r file; do
        local size=$(du -h "$file" | cut -f1)
        echo "- $(basename "$file") ($size)" >> "$summary_file"
    done
    
    cat "$summary_file"
    
    log_success "Summary saved to: $summary_file"
}

# Main execution
main() {
    echo -e "${CYAN}╔══════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║        AK macOS Package Builder      ║${NC}"
    echo -e "${CYAN}╚══════════════════════════════════════╝${NC}"
    echo
    
    parse_args "$@"
    
    log_info "Building packages: ${PACKAGE_TYPES[*]}"
    log_info "Version: $VERSION"
    log_info "Bundle ID: $BUNDLE_ID"
    
    validate_config
    setup_output_dir
    
    # Create packages based on requested types
    for package_type in "${PACKAGE_TYPES[@]}"; do
        case "$package_type" in
            app)
                create_app_bundle
                ;;
            pkg)
                create_pkg_installer
                ;;
            dmg)
                create_dmg_image
                ;;
            *)
                log_warning "Unknown package type: $package_type"
                ;;
        esac
    done
    
    # Post-processing
    notarize_packages
    generate_summary
    
    echo
    log_success "All packaging operations completed successfully!"
    log_info "Output directory: $OUTPUT_DIR"
}

# Run main function
main "$@"