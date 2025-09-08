#!/bin/bash

# AK macOS DMG Creation Script
# Creates a professional disk image for AK distribution

set -e

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MACOS_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$MACOS_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
DMG_DIR="$MACOS_DIR/dmg"
OUTPUT_DIR="$BUILD_DIR/macos-packages"

# Default values
VERSION="${AK_VERSION:-4.2.5}"
BUNDLE_ID="${AK_BUNDLE_ID:-dev.ak.ak}"
DMG_NAME="AK-${VERSION}"
VOLUME_NAME="AK ${VERSION}"
BUILD_DATE="$(date '+%Y-%m-%d %H:%M:%S')"

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

Create macOS DMG disk image for AK

OPTIONS:
    -v, --version VERSION       Set package version (default: $VERSION)
    -n, --name NAME            DMG filename (default: AK-VERSION)
    -t, --title TITLE          Volume title (default: "AK VERSION")
    -o, --output DIR           Output directory (default: $OUTPUT_DIR)
    --app-bundle PATH          Path to app bundle (default: build from source)
    -h, --help                 Show this help message

ENVIRONMENT VARIABLES:
    AK_VERSION                 Package version
    AK_BUNDLE_ID              Bundle identifier

EXAMPLES:
    $0                         Create DMG with default settings
    $0 -v 3.4.0 -n "AK-Beta"  Create DMG with version 3.4.0 named AK-Beta
EOF
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -v|--version)
                VERSION="$2"
                DMG_NAME="AK-${VERSION}"
                VOLUME_NAME="AK ${VERSION}"
                shift 2
                ;;
            -n|--name)
                DMG_NAME="$2"
                shift 2
                ;;
            -t|--title)
                VOLUME_NAME="$2"
                shift 2
                ;;
            -o|--output)
                OUTPUT_DIR="$2"
                shift 2
                ;;
            --app-bundle)
                APP_BUNDLE_PATH="$2"
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

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    # Check for required tools
    local missing_tools=()
    
    for tool in hdiutil osascript; do
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
    DMG_STAGING="$WORK_DIR/dmg-staging"
    TEMP_RESOURCES="$WORK_DIR/resources"
    
    mkdir -p "$DMG_STAGING"
    mkdir -p "$TEMP_RESOURCES"
    mkdir -p "$OUTPUT_DIR"
    
    log_success "Workspace created at $WORK_DIR"
}

# Create or copy app bundle
prepare_app_bundle() {
    log_info "Preparing app bundle..."
    
    local app_bundle="$DMG_STAGING/AK.app"
    
    if [[ -n "$APP_BUNDLE_PATH" && -d "$APP_BUNDLE_PATH" ]]; then
        # Use provided app bundle
        cp -R "$APP_BUNDLE_PATH" "$app_bundle"
        log_info "Using provided app bundle: $APP_BUNDLE_PATH"
    else
        # Create app bundle from build artifacts
        "$SCRIPT_DIR/create-app-bundle.sh" \
            --version "$VERSION" \
            --bundle-id "$BUNDLE_ID" \
            --output "$DMG_STAGING"
        log_info "Created app bundle from build artifacts"
    fi
    
    if [[ ! -d "$app_bundle" ]]; then
        log_error "App bundle not found: $app_bundle"
        exit 1
    fi
    
    log_success "App bundle prepared"
}

# Create Applications symlink
create_applications_link() {
    log_info "Creating Applications symlink..."
    
    ln -sf /Applications "$DMG_STAGING/Applications"
    
    log_success "Applications symlink created"
}

# Process template files and copy resources
prepare_dmg_resources() {
    log_info "Preparing DMG resources..."
    
    # Process README template
    if [[ -f "$DMG_DIR/README.txt" ]]; then
        sed -e "s/{{VERSION}}/$VERSION/g" \
            -e "s/{{BUILD_DATE}}/$BUILD_DATE/g" \
            "$DMG_DIR/README.txt" > "$DMG_STAGING/README.txt"
        log_info "README.txt processed"
    fi
    
    # Create uninstaller if it doesn't exist
    create_uninstaller
    
    # Copy background image if it exists
    if [[ -f "$DMG_DIR/background.png" ]]; then
        mkdir -p "$DMG_STAGING/.background"
        cp "$DMG_DIR/background.png" "$DMG_STAGING/.background/"
        log_info "Background image copied"
    else
        log_warning "Background image not found, DMG will use default appearance"
    fi
    
    log_success "DMG resources prepared"
}

# Create uninstaller app
create_uninstaller() {
    log_info "Creating uninstaller..."
    
    local uninstaller="$DMG_STAGING/Uninstall AK.app"
    mkdir -p "$uninstaller/Contents/MacOS"
    mkdir -p "$uninstaller/Contents/Resources"
    
    # Create uninstaller Info.plist
    cat > "$uninstaller/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDisplayName</key>
    <string>Uninstall AK</string>
    <key>CFBundleExecutable</key>
    <string>uninstall</string>
    <key>CFBundleIdentifier</key>
    <string>${BUNDLE_ID}.uninstaller</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>Uninstall AK</string>
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
    
    # Create uninstaller script
    cat > "$uninstaller/Contents/MacOS/uninstall" << 'EOF'
#!/bin/bash

# AK Uninstaller Script

osascript << 'APPLESCRIPT'
tell application "Finder"
    set response to display dialog "Are you sure you want to uninstall AK?" buttons {"Cancel", "Uninstall"} default button "Cancel" with icon caution
    if button returned of response is "Uninstall" then
        set progress total steps to 4
        set progress completed steps to 0
        set progress description to "Uninstalling AK..."
        
        -- Remove application bundle
        try
            do shell script "rm -rf '/Applications/AK.app'" with administrator privileges
            set progress completed steps to 1
        end try
        
        -- Remove CLI tools
        try
            do shell script "rm -f '/usr/local/bin/ak' '/usr/local/bin/ak-gui'" with administrator privileges
            set progress completed steps to 2
        end try
        
        -- Remove shared resources
        try
            do shell script "rm -rf '/usr/local/share/ak' '/usr/local/share/man/man1/ak.1'" with administrator privileges
            set progress completed steps to 3
        end try
        
        set progress completed steps to 4
        display dialog "AK has been successfully uninstalled." buttons {"OK"} default button "OK" with icon note
        
        -- Ask about user data
        set response2 to display dialog "Do you want to remove your AK configuration data?" buttons {"Keep Data", "Remove Data"} default button "Keep Data" with icon question
        if button returned of response2 is "Remove Data" then
            try
                do shell script "rm -rf '$HOME/.config/ak' '$HOME/.local/share/ak' '$HOME/.cache/ak'"
                display dialog "User data removed successfully." buttons {"OK"} default button "OK" with icon note
            end try
        end if
    end if
end tell
APPLESCRIPT
EOF
    
    chmod +x "$uninstaller/Contents/MacOS/uninstall"
    
    log_success "Uninstaller created"
}

# Create temporary DMG
create_temp_dmg() {
    log_info "Creating temporary DMG..."
    
    local temp_dmg="$WORK_DIR/temp.dmg"
    local staging_size=$(du -sm "$DMG_STAGING" | cut -f1)
    local dmg_size=$((staging_size + 50))  # Add 50MB padding
    
    hdiutil create \
        -srcfolder "$DMG_STAGING" \
        -volname "$VOLUME_NAME" \
        -fs HFS+ \
        -fsargs "-c c=64,a=16,e=16" \
        -format UDRW \
        -size "${dmg_size}m" \
        "$temp_dmg"
    
    log_success "Temporary DMG created"
    echo "$temp_dmg"
}

# Customize DMG appearance
customize_dmg_appearance() {
    local temp_dmg="$1"
    log_info "Customizing DMG appearance..."
    
    # Mount the DMG
    local mount_point="/Volumes/$VOLUME_NAME"
    hdiutil attach "$temp_dmg" -readwrite -mount required
    
    # Wait for mount
    sleep 2
    
    # Apply AppleScript customization if available
    if [[ -f "$DMG_DIR/dmg-setup.applescript" ]]; then
        local setup_script="$TEMP_RESOURCES/dmg-setup.applescript"
        sed -e "s/{{VERSION}}/$VERSION/g" \
            "$DMG_DIR/dmg-setup.applescript" > "$setup_script"
        
        osascript "$setup_script" || log_warning "DMG customization script failed"
    else
        log_warning "DMG setup script not found, using basic layout"
    fi
    
    # Ensure proper permissions
    chmod -R 755 "$mount_point" 2>/dev/null || true
    
    # Unmount
    hdiutil detach "$mount_point"
    
    log_success "DMG appearance customized"
}

# Create final compressed DMG
create_final_dmg() {
    local temp_dmg="$1"
    log_info "Creating final compressed DMG..."
    
    local final_dmg="$OUTPUT_DIR/${DMG_NAME}.dmg"
    
    # Remove existing DMG if it exists
    [[ -f "$final_dmg" ]] && rm "$final_dmg"
    
    # Create compressed DMG
    hdiutil convert \
        "$temp_dmg" \
        -format UDZO \
        -imagekey zlib-level=9 \
        -o "$final_dmg"
    
    # Verify the DMG
    hdiutil verify "$final_dmg"
    
    log_success "Final DMG created: $(basename "$final_dmg")"
    
    # Show file size
    local dmg_size=$(du -h "$final_dmg" | cut -f1)
    log_info "DMG size: $dmg_size"
}

# Cleanup function
cleanup() {
    if [[ -n "$WORK_DIR" && -d "$WORK_DIR" ]]; then
        log_info "Cleaning up temporary files..."
        
        # Unmount any mounted DMGs
        local mount_point="/Volumes/$VOLUME_NAME"
        if [[ -d "$mount_point" ]]; then
            hdiutil detach "$mount_point" 2>/dev/null || true
        fi
        
        rm -rf "$WORK_DIR"
    fi
}

# Main execution
main() {
    log_info "Starting DMG creation for AK v$VERSION"
    
    trap cleanup EXIT
    
    parse_args "$@"
    check_prerequisites
    setup_workspace
    prepare_app_bundle
    create_applications_link
    prepare_dmg_resources
    
    local temp_dmg
    temp_dmg=$(create_temp_dmg)
    customize_dmg_appearance "$temp_dmg"
    create_final_dmg "$temp_dmg"
    
    log_success "DMG creation completed successfully!"
    log_info "Output: $OUTPUT_DIR/${DMG_NAME}.dmg"
}

# Run main function
main "$@"