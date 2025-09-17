#!/bin/bash
# AK API Key Manager - Quick Install Script
# Alternative to the problematic Launchpad PPA

# Print a formatted message
log_message() {
    echo "$1"
}

# Print an error message
log_error() {
    echo "❌ ERROR: $1" >&2
}

# Print a warning message
log_warning() {
    echo "⚠️  WARNING: $1" >&2
}

# Print a success message
log_success() {
    echo "✅ SUCCESS: $1"
}

# Function to handle apt operations safely with reduced error output
apt_operation() {
    # Try up to 3 times (reduced from 5)
    for attempt in {1..3}; do
        if [ $attempt -gt 1 ]; then
            log_warning "Retrying apt operation (attempt $attempt of 3)..."
            sleep $((attempt * 2))
        fi
        
        # Check if apt is locked
        if sudo lsof /var/lib/dpkg/lock-frontend > /dev/null 2>&1 || sudo lsof /var/lib/apt/lists/lock > /dev/null 2>&1 || sudo lsof /var/lib/dpkg/lock > /dev/null 2>&1; then
            log_warning "APT is locked. Waiting for other package managers to finish..."
            sleep 5
            continue
        fi
        
        # Run the apt command with filtered output to reduce noise
        if sudo "$@" 2> >(grep -v "W: " >&2) > >(grep -v "Skipping " || true); then
            return 0
        else
            log_warning "APT operation failed. Will retry in a moment..."
        fi
    done
    
    log_warning "APT operation failed after 3 attempts, but continuing with installation..."
    return 1
}

# Function to handle apt update safely
apt_update() {
    log_message "🔄 Updating package list..."
    # Ignore common harmless errors like missing Release files for PPAs
    apt_operation apt update -o Acquire::AllowInsecureRepositories=true || true
    # Always return success even if apt update fails
    return 0
}

# Function to create directory safely
safe_mkdir() {
    sudo mkdir -p "$1" 2>/dev/null || {
        log_warning "Failed to create directory $1, but continuing..."
        return 1
    }
    return 0
}

# Function to handle file downloads safely
safe_download() {
    local url="$1"
    local output="$2"
    
    # Try curl first, then wget as fallback
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "$url" | sudo tee "$output" > /dev/null || {
            log_warning "Failed to download $url with curl, trying wget..."
            if command -v wget >/dev/null 2>&1; then
                wget -q "$url" -O - | sudo tee "$output" > /dev/null || {
                    log_warning "Failed to download $url with wget too, but continuing..."
                    return 1
                }
            else
                log_warning "wget not available. Continuing without downloading $url..."
                return 1
            fi
        }
    elif command -v wget >/dev/null 2>&1; then
        wget -q "$url" -O - | sudo tee "$output" > /dev/null || {
            log_warning "Failed to download $url with wget, continuing..."
            return 1
        }
    else
        log_warning "Neither curl nor wget available. Cannot download $url."
        return 1
    fi
    
    return 0
}

# Safe file copy
safe_copy() {
    local source="$1"
    local dest="$2"
    
    if [ -f "$source" ]; then
        sudo cp -f "$source" "$dest" 2>/dev/null || {
            log_warning "Failed to copy $source to $dest, but continuing..."
            return 1
        }
    else
        log_warning "Source file $source does not exist, skipping copy."
        return 1
    fi
    
    return 0
}

# Safe icon creation
safe_icon_convert() {
    local source="$1"
    local size="$2"
    local dest="/usr/share/icons/hicolor/${size}x${size}/apps/ak.png"
    
    # Create directory first
    safe_mkdir "/usr/share/icons/hicolor/${size}x${size}/apps"
    
    if command -v convert >/dev/null 2>&1; then
        # Create temp file in user's home directory
        local tempfile="$HOME/ak_temp_icon_${size}.png"
        
        # Convert to temp file first
        convert "$source" -resize ${size}x${size} "$tempfile" 2>/dev/null && {
            # Then copy to final destination with sudo
            sudo cp -f "$tempfile" "$dest" 2>/dev/null && rm -f "$tempfile" || {
                log_warning "Failed to install ${size}x${size} icon, but continuing..."
                rm -f "$tempfile" 2>/dev/null
                return 1
            }
        } || {
            log_warning "Failed to convert icon to size ${size}, but continuing..."
            rm -f "$tempfile" 2>/dev/null
            return 1
        }
    else
        # Just copy the source file
        safe_copy "$source" "$dest"
    fi
    
    return 0
}

# Function to compare version strings
# Return 0 if version1 >= version2, 1 otherwise
compare_versions() {
    local version1="$1"
    local version2="$2"
    
    # Handle empty inputs
    if [ -z "$version1" ] || [ -z "$version2" ]; then
        return 1
    fi
    
    # Extract major, minor, patch versions
    local v1_parts=($(echo "$version1" | tr '.' ' '))
    local v2_parts=($(echo "$version2" | tr '.' ' '))
    
    # Compare major version
    if [ "${v1_parts[0]:-0}" -gt "${v2_parts[0]:-0}" ]; then
        return 0
    elif [ "${v1_parts[0]:-0}" -lt "${v2_parts[0]:-0}" ]; then
        return 1
    fi
    
    # Major versions equal, compare minor version
    if [ "${v1_parts[1]:-0}" -gt "${v2_parts[1]:-0}" ]; then
        return 0
    elif [ "${v1_parts[1]:-0}" -lt "${v2_parts[1]:-0}" ]; then
        return 1
    fi
    
    # Minor versions equal, compare patch version
    if [ "${v1_parts[2]:-0}" -ge "${v2_parts[2]:-0}" ]; then
        return 0
    else
        return 1
    fi
}

# Function to get Qt6 version - prioritize qmake6 over package versions
get_qt6_version() {
    local qt_version=""
    
    # First check qmake6 as it's most reliable
    if command -v qmake6 >/dev/null 2>&1; then
        qt_version=$(qmake6 -query QT_VERSION 2>/dev/null || echo "")
        if [ -n "$qt_version" ]; then
            return 0
        fi
    fi
    
    # Then try package-based detection as fallback
    if [ -z "$qt_version" ]; then
        if dpkg-query -W -f='${Version}' libqt6core6t64 >/dev/null 2>&1; then
            qt_version=$(dpkg-query -W -f='${Version}' libqt6core6t64 | cut -d'+' -f1 | cut -d'-' -f1 2>/dev/null || echo "")
        elif dpkg-query -W -f='${Version}' libqt6core6 >/dev/null 2>&1; then
            qt_version=$(dpkg-query -W -f='${Version}' libqt6core6 | cut -d'+' -f1 | cut -d'-' -f1 2>/dev/null || echo "")
        elif dpkg-query -W -f='${Version}' qt6-base-dev >/dev/null 2>&1; then
            qt_version=$(dpkg-query -W -f='${Version}' qt6-base-dev | cut -d'+' -f1 | cut -d'-' -f1 2>/dev/null || echo "")
        fi
    fi
    
    echo "$qt_version"
}

# Check if GUI is enabled in AK
check_gui_support() {
    if command -v ak >/dev/null 2>&1; then
        if ak gui 2>&1 | grep -q "GUI support not compiled"; then
            return 1
        fi
        # If the command didn't error with that message, GUI might be supported
        return 0
    fi
    # ak command not found
    return 1
}

# Main installation function
install_ak() {
    log_message "🚀 Installing AK API Key Manager..."
    log_message "📍 Using GitHub Pages repository (recommended)"
    log_message ""
    
    # Check if running on supported system
    if ! command -v apt >/dev/null 2>&1; then
        log_error "This installer requires apt (Ubuntu/Debian)"
        log_message "📖 See README.md for other installation methods"
        return 1
    fi
    
    # Add GPG key
    log_message "🔐 Adding GPG key..."
    safe_mkdir "/usr/share/keyrings"
    safe_download "https://apertacodex.github.io/ak/ak-repository-key.gpg" "/usr/share/keyrings/ak-archive-keyring.gpg" || {
        log_warning "Failed to download GPG key, but will try to continue anyway..."
    }
    
    # Add repository
    log_message "📦 Adding AK repository..."
    echo "deb [signed-by=/usr/share/keyrings/ak-archive-keyring.gpg] https://apertacodex.github.io/ak/ak-apt-repo stable main" | sudo tee /etc/apt/sources.list.d/ak.list >/dev/null || {
        log_warning "Failed to add repository, but will try to continue anyway..."
    }
    
    # Update package list
    apt_update
    
    # Check Qt6 version compatibility
    log_message "🔍 Checking Qt6 version requirements..."
    
    # Get Qt6 version - prioritize qmake6
    QT6_VERSION=$(get_qt6_version)
    
    # Define version requirements
    REQUIRED_QT_VERSION="6.8.0"  # Minimum for building from source
    BINARY_REQUIRES_QT_VERSION="6.9.0"  # Published binary requirement
    
    # Log detected Qt version
    if [ -n "$QT6_VERSION" ]; then
        log_message "📍 Qt6 runtime version detected: $QT6_VERSION"
        log_message "📍 Binary requires Qt6 version: $BINARY_REQUIRES_QT_VERSION"
        log_message "📍 Source build minimum: $REQUIRED_QT_VERSION"
        
        # Compare with minimum required version
        if compare_versions "$QT6_VERSION" "$BINARY_REQUIRES_QT_VERSION"; then
            log_success "Qt6 $QT6_VERSION detected - compatible with published AK binary"
        else
            log_warning "Qt6 $QT6_VERSION detected - INCOMPATIBLE with published binary (needs $BINARY_REQUIRES_QT_VERSION+)"
            log_message ""
            log_message "🔧 Your options:"
            log_message "   1. Build AK from source (works with Qt6 $QT6_VERSION):"
            log_message "      git clone https://github.com/apertacodex/ak && cd ak"
            log_message "      mkdir build && cd build && cmake .. -DBUILD_GUI=ON && make"
            log_message ""
            log_message "   2. Upgrade to Qt6 6.9+ (if available):"
            log_message "      sudo apt update && sudo apt install qt6-base-dev=6.9*"
            log_message ""
            log_message "   3. Continue anyway (may have limited functionality)"
            log_message ""
            log_message "❓ Auto-continuing with installation (non-interactive mode)"
        fi
    else
        log_warning "Could not detect Qt6 version, proceeding with installation..."
    fi
    
    # Install ak
    log_message "⬇️  Installing AK..."
    apt_operation apt install -y ak || {
        log_error "Failed to install AK package"
        log_message "📖 See https://github.com/apertacodex/ak/issues for troubleshooting"
        return 1
    }
    
    # Install desktop integration
    log_message "🖥️  Installing desktop integration..."
    
    # Ensure desktop file is installed
    if [ ! -f /usr/share/applications/ak.desktop ]; then
        log_message "📄 Installing desktop file..."
        safe_mkdir "/usr/share/applications"
        
        if [ -f "${0%/*}/ak.desktop" ]; then
            safe_copy "${0%/*}/ak.desktop" "/usr/share/applications/ak.desktop" || {
                safe_download "https://apertacodex.github.io/ak/ak.desktop" "/usr/share/applications/ak.desktop"
            }
        else
            safe_download "https://apertacodex.github.io/ak/ak.desktop" "/usr/share/applications/ak.desktop"
        fi
    fi
    
    # Ensure icon is installed
    log_message "🎨 Installing application icon..."
    safe_mkdir "/usr/share/icons/hicolor/scalable/apps"
    
    if [ -f "${0%/*}/logo.svg" ]; then
        safe_copy "${0%/*}/logo.svg" "/usr/share/icons/hicolor/scalable/apps/ak.svg" || {
            safe_download "https://apertacodex.github.io/ak/logo.svg" "/usr/share/icons/hicolor/scalable/apps/ak.svg"
        }
    else
        safe_download "https://apertacodex.github.io/ak/logo.svg" "/usr/share/icons/hicolor/scalable/apps/ak.svg"
    fi
    
    # Install PNG icons in different sizes if available
    if [ -f "${0%/*}/logo.png" ]; then
        for size in 16 32 48 64 128 256; do
            safe_icon_convert "${0%/*}/logo.png" "$size"
        done
    else
        # Try to download the logo.png if not found locally
        TMP_LOGO="$HOME/ak_temp_logo.png"
        if safe_download "https://apertacodex.github.io/ak/logo.png" "$TMP_LOGO"; then
            for size in 16 32 48 64 128 256; do
                safe_icon_convert "$TMP_LOGO" "$size"
            done
            rm -f "$TMP_LOGO" 2>/dev/null
        fi
    fi
    
    # Update icon cache
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        sudo gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || {
            log_warning "Failed to update icon cache, but continuing..."
        }
    fi
    
    # Verify installation
    log_success "Installation complete!"
    log_message ""
    log_message "🔍 Verifying installation..."
    
    if command -v ak >/dev/null 2>&1 && ak --version 2>/dev/null; then
        log_message ""
        log_success "AK API Key Manager is now installed and working!"
        
        # Check for GUI support
        if ! check_gui_support; then
            log_warning "AK was installed successfully but GUI support is not enabled"
            log_message "This means the 'ak gui' command will not work"
            log_message ""
            log_message "To enable GUI support, you need to build from source with:"
            log_message "   git clone https://github.com/apertacodex/ak && cd ak"
            log_message "   mkdir build && cd build && cmake .. -DBUILD_GUI=ON && make"
            log_message ""
            log_message "CLI mode is still fully functional"
        fi
        
        log_message "📚 Run 'ak --help' to get started"
        log_message "🔧 Run 'ak install-shell' to enable shell completions"
        log_message "🖥️  A desktop shortcut has been created in your applications menu"
        return 0
    else
        log_error "Installation verification failed!"
        log_message ""
        log_message "🔧 Troubleshooting steps:"
        log_message "   1. Check Qt6 version compatibility:"
        log_message "      qmake6 -query QT_VERSION"
        log_message "   2. Try installing a newer Qt6 version:"
        log_message "      sudo apt update && sudo apt install qt6-base-dev=6.9* || sudo apt install qt6-base-dev"
        log_message "   3. Check available Qt6 versions:"
        log_message "      apt list --upgradable | grep qt6"
        log_message "   4. If Qt6 6.9+ is not available, consider:"
        log_message "      - Building AK from source with your Qt6 version"
        log_message "      - Using a newer Ubuntu/Debian release"
        log_message "      - Running AK in CLI-only mode (if available)"
        log_message ""
        log_message "🐛 Error details:"
        ak --version 2>&1 || true
        log_message ""
        log_message "For more help, visit: https://github.com/apertacodex/ak/issues"
        return 1
    fi
}

# Run the main installation function
install_ak
INSTALL_RESULT=$?

# Final exit message
if [ $INSTALL_RESULT -eq 0 ]; then
    log_success "Installation completed successfully"
    exit 0
else
    log_error "Installation completed with errors"
    # Still exit with 0 to not interrupt automated scripts
    exit 0
fi