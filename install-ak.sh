#!/usr/bin/env bash

#
# AK API Key Manager - Installation Script
# Cross-platform installer for AK API Key Manager
#
# Copyright (C) 2024 AK Team
# Licensed under the Apache License, Version 2.0
#

# Exit on any error and undefined variables
set -euo pipefail

# Program version and options
readonly SCRIPT_VERSION="2.0.0"
readonly REPO_URL="https://apertacodex.github.io/ak"
readonly REPO_KEY_URL="${REPO_URL}/ak-repository-key.gpg"
readonly DESKTOP_FILE_URL="${REPO_URL}/ak.desktop"
readonly LOGO_SVG_URL="${REPO_URL}/logo.svg"
readonly LOGO_PNG_URL="${REPO_URL}/logo.png"

# Program options
OPT_UNINSTALL=0
OPT_VERBOSE=0
OPT_BUILD_FROM_SOURCE=0

# Platform variables
PLATFORM_OS=""
DISTRO=""
CMD_SUDO=""

# Colors and formatting
readonly BOLD='\033[1m'
readonly RED='\033[0;31m'
readonly GREEN='\033[0;32m'
readonly YELLOW='\033[0;33m'
readonly BLUE='\033[0;34m'
readonly NC='\033[0m' # No Color

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --uninstall)
                OPT_UNINSTALL=1
                shift
                ;;
            --verbose)
                OPT_VERBOSE=1
                shift
                ;;
            --build-from-source)
                OPT_BUILD_FROM_SOURCE=1
                shift
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            --version|-v)
                echo "AK Installer v${SCRIPT_VERSION}"
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

# Show help message
show_help() {
    cat << EOF
AK API Key Manager Installer v${SCRIPT_VERSION}

Usage: $0 [OPTIONS]

OPTIONS:
    --uninstall           Remove AK from the system
    --build-from-source   Build AK from source instead of using packages
    --verbose             Enable verbose output
    --help, -h            Show this help message
    --version, -v         Show version information

Examples:
    $0                    Install AK using packages
    $0 --build-from-source    Build and install from source
    $0 --uninstall        Remove AK installation

EOF
}

# Logging functions
title() {
    echo -e "${BOLD}$*${NC}\n"
}

print() {
    echo -e "▶ $*"
}

success() {
    echo -e "${GREEN}✓ $*${NC}"
}

warning() {
    echo -e "${YELLOW}⚠ $*${NC}"
}

error() {
    echo -e "${RED}✗ $*${NC}" >&2
}

verbose() {
    if [[ $OPT_VERBOSE -eq 1 ]]; then
        echo -e "${BLUE}• $*${NC}"
    fi
}

# Check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check required programs
check_dependencies() {
    local missing_deps=()
    
    for cmd in curl wget sudo apt; do
        if ! command_exists "$cmd"; then
            missing_deps+=("$cmd")
        fi
    done
    
    if [[ ${#missing_deps[@]} -gt 0 ]]; then
        error "Missing required dependencies: ${missing_deps[*]}"
        error "Please install the missing programs and try again."
        exit 1
    fi
}

# Detect platform
detect_platform() {
    verbose "Detecting platform..."
    
    case "$(uname -s)" in
        Linux*)
            PLATFORM_OS="linux"
            if [[ -f /etc/os-release ]]; then
                . /etc/os-release
                DISTRO="$ID"
            fi
            ;;
        Darwin*)
            PLATFORM_OS="macos"
            error "macOS installation not yet supported. Please use Homebrew or build from source."
            exit 1
            ;;
        *)
            error "Unsupported operating system: $(uname -s)"
            exit 1
            ;;
    esac
    
    # Check for Debian/Ubuntu
    if [[ "$PLATFORM_OS" == "linux" ]] && ! command_exists apt; then
        error "This installer currently supports Debian/Ubuntu systems only."
        error "For other distributions, please build from source."
        exit 1
    fi
    
    # Check for sudo
    if [[ $EUID -ne 0 ]]; then
        if ! command_exists sudo; then
            error "This script requires sudo or root privileges."
            exit 1
        fi
        CMD_SUDO="sudo"
    fi
    
    success "Platform detected: $PLATFORM_OS ($DISTRO)"
}

# Download file safely
download_file() {
    local url="$1"
    local output="$2"
    local temp_file="/tmp/ak-download-$(basename "$output").tmp"
    
    verbose "Downloading $url to $output"
    
    if command_exists curl; then
        curl -fsSL "$url" -o "$temp_file"
    elif command_exists wget; then
        wget -q "$url" -O "$temp_file"
    else
        error "Neither curl nor wget found"
        return 1
    fi
    
    $CMD_SUDO mv "$temp_file" "$output"
    rm -f "$temp_file" 2>/dev/null || true
}

# Setup APT repository
setup_repository() {
    print "Setting up AK repository..."
    
    # Create keyring directory
    $CMD_SUDO mkdir -p /usr/share/keyrings
    
    # Download and install GPG key
    if ! download_file "$REPO_KEY_URL" "/usr/share/keyrings/ak-archive-keyring.gpg"; then
        error "Failed to download repository GPG key"
        exit 1
    fi
    
    # Add repository to sources list
    echo "deb [signed-by=/usr/share/keyrings/ak-archive-keyring.gpg] ${REPO_URL}/ak-apt-repo stable main" | \
        $CMD_SUDO tee /etc/apt/sources.list.d/ak.list >/dev/null
    
    # Update package lists
    print "Updating package lists..."
    if ! $CMD_SUDO apt update -qq 2>/dev/null; then
        warning "APT update had some issues (likely broken 3rd party repos), but continuing..."
        # Try to update without failing on warnings/errors
        $CMD_SUDO apt update -qq --allow-releaseinfo-change 2>/dev/null || true
    fi
    
    success "Repository configured successfully"
}

# Install AK package
install_package() {
    print "Installing AK package..."
    
    if ! $CMD_SUDO apt install -y ak 2>/dev/null; then
        error "Failed to install AK package from repository"
        warning "This might be due to broken system repositories"
        error "Try using --build-from-source option instead"
        exit 1
    fi
    
    success "AK package installed successfully"
}

# Build AK from source
build_from_source() {
    print "Building AK from source..."
    
    # Install build dependencies
    print "Installing build dependencies..."
    $CMD_SUDO apt update -qq 2>/dev/null || true
    if ! $CMD_SUDO apt install -y build-essential cmake git qt6-base-dev qt6-tools-dev qt6-svg-dev 2>/dev/null; then
        error "Failed to install build dependencies"
        error "Please fix your system's APT configuration and try again"
        exit 1
    fi
    
    # Create temporary directory
    local build_dir
    build_dir=$(mktemp -d)
    verbose "Using build directory: $build_dir"
    
    # Clone repository
    print "Cloning AK repository..."
    if ! git clone --depth 1 https://github.com/apertacodex/ak.git "$build_dir"; then
        error "Failed to clone AK repository"
        exit 1
    fi
    
    # Build
    print "Compiling AK..."
    (
        cd "$build_dir"
        mkdir build
        cd build
        cmake .. -DBUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release
        make -j"$(nproc)"
        $CMD_SUDO make install
    )
    
    # Cleanup
    rm -rf "$build_dir"
    
    success "AK built and installed from source"
}

# Install desktop integration
install_desktop_integration() {
    print "Installing desktop integration..."
    
    # Install desktop file
    $CMD_SUDO mkdir -p /usr/share/applications
    if ! download_file "$DESKTOP_FILE_URL" "/usr/share/applications/ak.desktop"; then
        warning "Failed to download desktop file, skipping"
        return 0
    fi
    
    # Install icons
    $CMD_SUDO mkdir -p /usr/share/icons/hicolor/scalable/apps
    if download_file "$LOGO_SVG_URL" "/usr/share/icons/hicolor/scalable/apps/ak.svg"; then
        verbose "Installed SVG icon"
    fi
    
    # Install PNG icon sizes
    for size in 16 32 48 64 128 256; do
        local icon_dir="/usr/share/icons/hicolor/${size}x${size}/apps"
        $CMD_SUDO mkdir -p "$icon_dir"
        if download_file "$LOGO_PNG_URL" "$icon_dir/ak.png"; then
            verbose "Installed ${size}x${size} PNG icon"
        fi
    done
    
    # Update desktop database
    if command_exists update-desktop-database; then
        $CMD_SUDO update-desktop-database /usr/share/applications 2>/dev/null || true
    fi
    
    # Update icon cache
    if command_exists gtk-update-icon-cache; then
        $CMD_SUDO gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true
    fi
    
    success "Desktop integration installed"
}

# Verify installation
verify_installation() {
    print "Verifying installation..."
    
    if ! command_exists ak; then
        error "AK command not found in PATH"
        error "Installation may have failed"
        exit 1
    fi
    
    # Test basic functionality
    local version_output
    if version_output=$(ak --version 2>&1); then
        success "AK is working correctly: $version_output"
    else
        warning "AK installed but may have runtime issues"
        verbose "Error output: $version_output"
    fi
    
    success "Installation verified"
}

# Uninstall AK
uninstall_ak() {
    print "Uninstalling AK..."
    
    # Remove package
    if command_exists ak; then
        $CMD_SUDO apt remove -y ak 2>/dev/null || true
    fi
    
    # Remove repository
    $CMD_SUDO rm -f /etc/apt/sources.list.d/ak.list
    $CMD_SUDO rm -f /usr/share/keyrings/ak-archive-keyring.gpg
    
    # Remove desktop integration
    $CMD_SUDO rm -f /usr/share/applications/ak.desktop
    $CMD_SUDO rm -f /usr/share/icons/hicolor/scalable/apps/ak.svg
    for size in 16 32 48 64 128 256; do
        $CMD_SUDO rm -f "/usr/share/icons/hicolor/${size}x${size}/apps/ak.png"
    done
    
    # Update caches
    if command_exists update-desktop-database; then
        $CMD_SUDO update-desktop-database /usr/share/applications 2>/dev/null || true
    fi
    if command_exists gtk-update-icon-cache; then
        $CMD_SUDO gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true
    fi
    
    # Update package lists
    $CMD_SUDO apt update -qq 2>/dev/null || true
    
    success "AK uninstalled successfully"
}

# Main installation function
install_ak() {
    title "🚀 AK API Key Manager Installer v${SCRIPT_VERSION}"
    
    # Setup repository and install
    setup_repository
    
    if [[ $OPT_BUILD_FROM_SOURCE -eq 1 ]]; then
        build_from_source
    else
        install_package
    fi
    
    # Install desktop integration
    install_desktop_integration
    
    # Verify installation
    verify_installation
    
    echo
    success "🎉 AK API Key Manager installed successfully!"
    echo
    print "You can now:"
    print "  • Run 'ak --help' to see available commands"
    print "  • Launch the GUI from your applications menu"
    print "  • Visit https://github.com/apertacodex/ak for documentation"
    echo
}

# Main function
main() {
    # Parse command line arguments
    parse_args "$@"
    
    # Check dependencies and platform
    check_dependencies
    detect_platform
    
    # Handle uninstall
    if [[ $OPT_UNINSTALL -eq 1 ]]; then
        uninstall_ak
        return 0
    fi
    
    # Run installation
    install_ak
}

# Cleanup on exit
cleanup() {
    if [[ $? -ne 0 ]]; then
        echo
        error "Installation failed!"
        print "For help and troubleshooting, visit:"
        print "https://github.com/apertacodex/ak/issues"
    fi
}

trap cleanup EXIT

# Run main function
main "$@"
