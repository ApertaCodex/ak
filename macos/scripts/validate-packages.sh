#!/bin/bash

# AK macOS Package Validation Script
# Validates created packages for common issues and compliance

set -e

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MACOS_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$MACOS_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
OUTPUT_DIR="${1:-$BUILD_DIR/macos-packages}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Validation counters
CHECKS_PASSED=0
CHECKS_FAILED=0
CHECKS_WARNING=0

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((CHECKS_PASSED++))
}

log_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
    ((CHECKS_WARNING++))
}

log_error() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((CHECKS_FAILED++))
}

log_check() {
    echo -e "${CYAN}[CHECK]${NC} $1"
}

# Usage information
usage() {
    cat << EOF
Usage: $0 [OUTPUT_DIR]

Validate macOS packages for common issues and compliance

ARGUMENTS:
    OUTPUT_DIR                 Directory containing packages to validate
                              (default: $BUILD_DIR/macos-packages)

VALIDATION CHECKS:
    - App bundle structure and Info.plist
    - PKG installer integrity and signatures
    - DMG format and layout
    - Code signing status
    - File permissions
    - Dependencies and framework linking

EXAMPLES:
    $0                         Validate packages in default location
    $0 /path/to/packages      Validate packages in custom location
EOF
}

# Parse arguments
parse_args() {
    if [[ "$1" == "-h" || "$1" == "--help" ]]; then
        usage
        exit 0
    fi
    
    if [[ ! -d "$OUTPUT_DIR" ]]; then
        log_error "Output directory not found: $OUTPUT_DIR"
        exit 1
    fi
}

# Find packages to validate
find_packages() {
    log_info "Scanning for packages in: $OUTPUT_DIR"
    
    APP_BUNDLES=($(find "$OUTPUT_DIR" -name "*.app" -type d 2>/dev/null))
    PKG_FILES=($(find "$OUTPUT_DIR" -name "*.pkg" -type f 2>/dev/null))
    DMG_FILES=($(find "$OUTPUT_DIR" -name "*.dmg" -type f 2>/dev/null))
    
    log_info "Found ${#APP_BUNDLES[@]} app bundle(s), ${#PKG_FILES[@]} PKG(s), ${#DMG_FILES[@]} DMG(s)"
}

# Validate app bundle
validate_app_bundle() {
    local app_bundle="$1"
    local app_name=$(basename "$app_bundle")
    
    log_check "Validating app bundle: $app_name"
    
    # Check basic structure
    if [[ ! -d "$app_bundle/Contents" ]]; then
        log_error "$app_name: Missing Contents directory"
        return
    fi
    
    if [[ ! -f "$app_bundle/Contents/Info.plist" ]]; then
        log_error "$app_name: Missing Info.plist"
        return
    fi
    
    if [[ ! -d "$app_bundle/Contents/MacOS" ]]; then
        log_error "$app_name: Missing MacOS directory"
        return
    fi
    
    log_success "$app_name: Basic structure valid"
    
    # Validate Info.plist
    if plutil -lint "$app_bundle/Contents/Info.plist" >/dev/null 2>&1; then
        log_success "$app_name: Info.plist format valid"
    else
        log_error "$app_name: Info.plist format invalid"
    fi
    
    # Check for executable
    local info_plist="$app_bundle/Contents/Info.plist"
    local executable_name=$(plutil -extract CFBundleExecutable raw "$info_plist" 2>/dev/null || echo "")
    
    if [[ -n "$executable_name" && -f "$app_bundle/Contents/MacOS/$executable_name" ]]; then
        log_success "$app_name: Main executable found"
        
        # Check executable permissions
        if [[ -x "$app_bundle/Contents/MacOS/$executable_name" ]]; then
            log_success "$app_name: Executable has proper permissions"
        else
            log_error "$app_name: Executable not executable"
        fi
    else
        log_error "$app_name: Main executable missing"
    fi
    
    # Check code signing
    check_code_signature "$app_bundle"
    
    # Check dependencies
    check_dependencies "$app_bundle"
    
    # Check resources
    if [[ -d "$app_bundle/Contents/Resources" ]]; then
        log_success "$app_name: Resources directory present"
        
        # Check for icon
        local icon_file=$(plutil -extract CFBundleIconFile raw "$info_plist" 2>/dev/null || echo "")
        if [[ -n "$icon_file" ]]; then
            if [[ -f "$app_bundle/Contents/Resources/$icon_file" ]]; then
                log_success "$app_name: Icon file found"
            else
                log_warning "$app_name: Icon file specified but not found"
            fi
        fi
    else
        log_warning "$app_name: No Resources directory"
    fi
}

# Check code signature
check_code_signature() {
    local bundle="$1"
    local bundle_name=$(basename "$bundle")
    
    if codesign --verify --verbose=2 "$bundle" >/dev/null 2>&1; then
        log_success "$bundle_name: Code signature valid"
        
        # Get signing identity
        local identity=$(codesign -dv "$bundle" 2>&1 | grep "Authority=" | head -1 | sed 's/.*Authority=//')
        if [[ -n "$identity" ]]; then
            log_info "$bundle_name: Signed with: $identity"
        fi
    else
        log_warning "$bundle_name: Not code signed or signature invalid"
    fi
}

# Check dependencies and linking
check_dependencies() {
    local app_bundle="$1"
    local app_name=$(basename "$app_bundle")
    local executable_name=$(plutil -extract CFBundleExecutable raw "$app_bundle/Contents/Info.plist" 2>/dev/null || echo "")
    
    if [[ -z "$executable_name" ]]; then
        return
    fi
    
    local executable_path="$app_bundle/Contents/MacOS/$executable_name"
    if [[ ! -f "$executable_path" ]]; then
        return
    fi
    
    # Check for external dependencies
    local external_deps=$(otool -L "$executable_path" 2>/dev/null | grep -v "^$executable_path" | grep -v "@rpath" | grep -v "@executable_path" | grep -v "/usr/lib" | grep -v "/System/Library" | wc -l | tr -d ' ')
    
    if [[ "$external_deps" -eq 0 ]]; then
        log_success "$app_name: No problematic external dependencies"
    else
        log_warning "$app_name: $external_deps external dependencies found"
        otool -L "$executable_path" | grep -v "^$executable_path" | grep -v "@rpath" | grep -v "@executable_path" | grep -v "/usr/lib" | grep -v "/System/Library" | while read -r line; do
            log_warning "  - $(echo "$line" | awk '{print $1}')"
        done
    fi
    
    # Check for bundled frameworks
    if [[ -d "$app_bundle/Contents/Frameworks" ]]; then
        local framework_count=$(find "$app_bundle/Contents/Frameworks" -name "*.framework" -type d | wc -l | tr -d ' ')
        log_success "$app_name: $framework_count bundled framework(s)"
    fi
}

# Validate PKG files
validate_pkg() {
    local pkg_file="$1"
    local pkg_name=$(basename "$pkg_file")
    
    log_check "Validating PKG: $pkg_name"
    
    # Check PKG integrity
    if pkgutil --check-signature "$pkg_file" >/dev/null 2>&1; then
        log_success "$pkg_name: Signature valid"
    else
        log_warning "$pkg_name: Not signed or signature invalid"
    fi
    
    # Check PKG contents
    local temp_dir=$(mktemp -d)
    if pkgutil --expand "$pkg_file" "$temp_dir" >/dev/null 2>&1; then
        log_success "$pkg_name: Package structure valid"
        
        # Check for Distribution file
        if [[ -f "$temp_dir/Distribution" ]]; then
            log_success "$pkg_name: Distribution file present"
        else
            log_warning "$pkg_name: No Distribution file"
        fi
        
        # Check for component packages
        local components=$(find "$temp_dir" -name "*.pkg" -type d | wc -l | tr -d ' ')
        log_info "$pkg_name: $components component package(s)"
        
        rm -rf "$temp_dir"
    else
        log_error "$pkg_name: Cannot expand package"
    fi
    
    # Check file size (should be reasonable)
    local size_mb=$(du -m "$pkg_file" | cut -f1)
    if [[ $size_mb -gt 500 ]]; then
        log_warning "$pkg_name: Large package size: ${size_mb}MB"
    else
        log_success "$pkg_name: Reasonable package size: ${size_mb}MB"
    fi
}

# Validate DMG files
validate_dmg() {
    local dmg_file="$1"
    local dmg_name=$(basename "$dmg_file")
    
    log_check "Validating DMG: $dmg_name"
    
    # Verify DMG integrity
    if hdiutil verify "$dmg_file" >/dev/null 2>&1; then
        log_success "$dmg_name: DMG integrity valid"
    else
        log_error "$dmg_name: DMG integrity check failed"
        return
    fi
    
    # Check code signature
    check_code_signature "$dmg_file"
    
    # Check file size
    local size_mb=$(du -m "$dmg_file" | cut -f1)
    if [[ $size_mb -gt 1000 ]]; then
        log_warning "$dmg_name: Large DMG size: ${size_mb}MB"
    else
        log_success "$dmg_name: Reasonable DMG size: ${size_mb}MB"
    fi
    
    # Mount and check contents
    local temp_mount="/tmp/dmg_validation_$$"
    if hdiutil attach "$dmg_file" -mountpoint "$temp_mount" -quiet >/dev/null 2>&1; then
        log_success "$dmg_name: DMG mounts successfully"
        
        # Check for expected contents
        if [[ -d "$temp_mount/AK.app" ]]; then
            log_success "$dmg_name: App bundle present in DMG"
        else
            log_warning "$dmg_name: No app bundle found in DMG"
        fi
        
        if [[ -L "$temp_mount/Applications" ]]; then
            log_success "$dmg_name: Applications symlink present"
        else
            log_warning "$dmg_name: No Applications symlink"
        fi
        
        # Unmount
        hdiutil detach "$temp_mount" -quiet >/dev/null 2>&1
    else
        log_error "$dmg_name: Cannot mount DMG"
    fi
}

# Check system requirements
check_system_requirements() {
    log_check "Checking system requirements for packaging"
    
    # Check macOS version
    local macos_version=$(sw_vers -productVersion)
    log_info "macOS version: $macos_version"
    
    # Check Xcode tools
    if xcode-select -p >/dev/null 2>&1; then
        log_success "Xcode Command Line Tools installed"
    else
        log_warning "Xcode Command Line Tools not found"
    fi
    
    # Check required tools
    local tools=("pkgbuild" "productbuild" "hdiutil" "codesign" "plutil")
    for tool in "${tools[@]}"; do
        if command -v "$tool" >/dev/null 2>&1; then
            log_success "Tool available: $tool"
        else
            log_error "Missing tool: $tool"
        fi
    done
}

# Generate validation report
generate_report() {
    local report_file="$OUTPUT_DIR/validation-report.txt"
    
    cat > "$report_file" << EOF
AK macOS Package Validation Report
==================================

Validation Date: $(date '+%Y-%m-%d %H:%M:%S')
Output Directory: $OUTPUT_DIR

Summary:
- Checks Passed: $CHECKS_PASSED
- Checks Failed: $CHECKS_FAILED
- Warnings: $CHECKS_WARNING

Packages Validated:
- App Bundles: ${#APP_BUNDLES[@]}
- PKG Files: ${#PKG_FILES[@]}
- DMG Files: ${#DMG_FILES[@]}

Status: $(if [[ $CHECKS_FAILED -eq 0 ]]; then echo "PASSED"; else echo "FAILED"; fi)
EOF
    
    if [[ ${#APP_BUNDLES[@]} -gt 0 ]]; then
        echo -e "\nApp Bundles:" >> "$report_file"
        for app in "${APP_BUNDLES[@]}"; do
            echo "- $(basename "$app")" >> "$report_file"
        done
    fi
    
    if [[ ${#PKG_FILES[@]} -gt 0 ]]; then
        echo -e "\nPKG Files:" >> "$report_file"
        for pkg in "${PKG_FILES[@]}"; do
            echo "- $(basename "$pkg")" >> "$report_file"
        done
    fi
    
    if [[ ${#DMG_FILES[@]} -gt 0 ]]; then
        echo -e "\nDMG Files:" >> "$report_file"
        for dmg in "${DMG_FILES[@]}"; do
            echo "- $(basename "$dmg")" >> "$report_file"
        done
    fi
    
    log_info "Validation report saved to: $report_file"
}

# Main execution
main() {
    echo -e "${CYAN}╔══════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║     AK macOS Package Validator       ║${NC}"
    echo -e "${CYAN}╚══════════════════════════════════════╝${NC}"
    echo
    
    parse_args "$@"
    check_system_requirements
    find_packages
    
    # Validate app bundles
    for app_bundle in "${APP_BUNDLES[@]}"; do
        validate_app_bundle "$app_bundle"
        echo
    done
    
    # Validate PKG files
    for pkg_file in "${PKG_FILES[@]}"; do
        validate_pkg "$pkg_file"
        echo
    done
    
    # Validate DMG files
    for dmg_file in "${DMG_FILES[@]}"; do
        validate_dmg "$dmg_file"
        echo
    done
    
    # Generate summary
    echo -e "${CYAN}═══════════════════════════════════════${NC}"
    echo -e "${GREEN}Validation Summary${NC}"
    echo -e "${CYAN}═══════════════════════════════════════${NC}"
    echo -e "Checks Passed: ${GREEN}$CHECKS_PASSED${NC}"
    echo -e "Checks Failed: ${RED}$CHECKS_FAILED${NC}"
    echo -e "Warnings: ${YELLOW}$CHECKS_WARNING${NC}"
    echo
    
    generate_report
    
    if [[ $CHECKS_FAILED -eq 0 ]]; then
        log_success "All validations passed!"
        exit 0
    else
        log_error "Some validations failed. Please review the issues above."
        exit 1
    fi
}

# Run main function
main "$@"