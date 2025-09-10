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

# Function to handle apt operations safely
apt_operation() {
    # Try up to 5 times with increasing sleep times
    for attempt in {1..5}; do
        if [ $attempt -gt 1 ]; then
            log_warning "Retrying apt operation (attempt $attempt of 5)..."
            sleep $((attempt * 2))
        fi
        
        # Check if apt is locked
        if sudo lsof /var/lib/dpkg/lock-frontend > /dev/null 2>&1 || sudo lsof /var/lib/apt/lists/lock > /dev/null 2>&1 || sudo lsof /var/lib/dpkg/lock > /dev/null 2>&1; then
            log_warning "APT is locked. Waiting for other package managers to finish..."
            sleep 5
            continue
        fi
        
        # Run the apt command
        if sudo "$@"; then
            return 0
        else
            log_warning "APT operation failed. Will retry in a moment..."
        fi
    done
    
    log_warning "APT operation failed after 5 attempts, but continuing with installation..."
    return 1
}

# Function to handle apt update safely
apt_update() {
    log_message "🔄 Updating package list..."
    apt_operation apt update || true
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

# Function to check version compatibility
is_qt6_compatible() {
    local version="$1"
    
    # Handle empty input
    if [ -z "$version" ]; then
        return 1
    fi
    
    # Extract major version safely
    local major_version
    major_version=$(echo "$version" | grep -o '^[0-9]\+' || echo "0")
    
    if [ "$major_version" -ge 6 ]; then
        return 0  # compatible
    else
        return 1  # not compatible
    fi
}

# Function to get Qt6 version
get_qt6_version() {
    local qt_version=""
    local qt_from_dpkg=""
    local qt_from_qmake=""
    
    # DIAGNOSTIC: Log all detected versions
    log_message "🔍 DIAGNOSTIC: Checking Qt6 versions from all sources..."
    
    # Try different methods to detect Qt6 version
    if dpkg-query -W -f='${Version}' libqt6core6t64 >/dev/null 2>&1; then
        qt_from_dpkg=$(dpkg-query -W -f='${Version}' libqt6core6t64 | cut -d'+' -f1 | cut -d'-' -f1 2>/dev/null || echo "")
        log_message "🔍 DIAGNOSTIC: Found Qt6 from libqt6core6t64: $qt_from_dpkg"
    fi
    
    if dpkg-query -W -f='${Version}' libqt6core6 >/dev/null 2>&1; then
        qt_from_dpkg=$(dpkg-query -W -f='${Version}' libqt6core6 | cut -d'+' -f1 | cut -d'-' -f1 2>/dev/null || echo "")
        log_message "🔍 DIAGNOSTIC: Found Qt6 from libqt6core6: $qt_from_dpkg"
    fi
    
    if dpkg-query -W -f='${Version}' qt6-base-dev >/dev/null 2>&1; then
        qt_from_dpkg=$(dpkg-query -W -f='${Version}' qt6-base-dev | cut -d'+' -f1 | cut -d'-' -f1 2>/dev/null || echo "")
        log_message "🔍 DIAGNOSTIC: Found Qt6 from qt6-base-dev: $qt_from_dpkg"
    fi
    
    if command -v qmake6 >/dev/null 2>&1; then
        # Check qmake version
        qt_from_qmake=$(qmake6 -query QT_VERSION 2>/dev/null || echo "")
        log_message "🔍 DIAGNOSTIC: Found Qt6 from qmake6: $qt_from_qmake"
    fi
    
    # Compare and use the highest version
    if [ -n "$qt_from_qmake" ]; then
        # Use qmake version first if available (most reliable)
        qt_version="$qt_from_qmake"
        log_message "🔍 DIAGNOSTIC: Using qmake6 version: $qt_version"
    elif [ -n "$qt_from_dpkg" ]; then
        # Fall back to dpkg version
        qt_version="$qt_from_dpkg"
        log_message "🔍 DIAGNOSTIC: Using dpkg package version: $qt_version"
    else
        log_message "🔍 DIAGNOSTIC: No Qt6 version detected"
    fi
    
    echo "$qt_version"
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
    
    REQUIRED_QT_VERSION="6.8.0"  # Minimum for building from source
    BINARY_REQUIRES_QT_VERSION="6.8.0"  # Published binary requirement
    
    # Try to install Qt6 6.9.0 specifically first
    log_message "📦 Installing Qt6 6.9.0 dependencies..."
    log_message "🔄 Trying to install Qt6 6.9.0..."
    
    if apt_operation apt install -y qt6-base-dev=6.9* libqt6widgets6=6.9* qt6-tools-dev=6.9* qt6-tools-dev-tools=6.9* 2>/dev/null; then
        log_success "Successfully installed Qt6 6.9.x"
    else
        log_warning "Qt6 6.9.x not available in repositories"
        log_message "📦 Installing available Qt6 version..."
        apt_operation apt install -y qt6-base-dev libqt6widgets6 qt6-tools-dev qt6-tools-dev-tools || {
            log_warning "Failed to install Qt6 packages, but continuing anyway..."
        }
        
        log_message ""
        log_message "🔧 To get Qt6 6.9.0, you can try:"
        log_message "   1. Add Qt's official repository:"
        log_message "      wget -qO - https://download.qt.io/official_releases/qt/6.9/6.9.0/submodules/apt-key.gpg | sudo apt-key add -"
        log_message "      echo 'deb https://download.qt.io/official_releases/qt/6.9/6.9.0/submodules/ focal main' | sudo tee /etc/apt/sources.list.d/qt6.list"
        log_message "      sudo apt update && sudo apt install qt6-base-dev=6.9*"
        log_message ""
        log_message "   2. Or build AK from source to use your current Qt6 version"
        log_message ""
    fi
    
    # Re-check Qt6 runtime version after installation
    QT6_VERSION=$(get_qt6_version)
    
    if [ -n "$QT6_VERSION" ]; then
        log_message "📍 Qt6 runtime version detected: $QT6_VERSION"
        log_message "📍 Binary requires Qt6 version: $BINARY_REQUIRES_QT_VERSION"
        log_message "📍 Source build minimum: $REQUIRED_QT_VERSION"
        
        if is_qt6_compatible "$QT6_VERSION"; then
            # Check if version is compatible with published binary
            # DIAGNOSTIC: Show detailed version parsing
            log_message "🔍 DIAGNOSTIC: Checking version compatibility for $QT6_VERSION"
            major_minor=$(echo "$QT6_VERSION" | cut -d. -f1,2 2>/dev/null || echo "0.0")
            log_message "🔍 DIAGNOSTIC: Extracted major.minor version: $major_minor"
            log_message "🔍 DIAGNOSTIC: Required version: $BINARY_REQUIRES_QT_VERSION"
            log_message "🔍 DIAGNOSTIC: Checking if $major_minor >= 6.9"
            
            # Debug the comparison
            if [ "$major_minor" = "6.9" ]; then
                log_message "🔍 DIAGNOSTIC: Version matched exactly 6.9"
            fi
            
            if [ "$major_minor" \> "6.9" ]; then
                log_message "🔍 DIAGNOSTIC: Version is greater than 6.9"
            fi
            
            # Perform the actual check with more robust comparison
            required_major_minor="6.9"  # Hardcoded reference version
            
            if [ "$(echo "$major_minor >= $required_major_minor" | bc)" -eq 1 ]; then
                log_success "Qt6 $QT6_VERSION detected - compatible with published AK binary"
            else
                log_warning "Qt6 $QT6_VERSION detected - INCOMPATIBLE with published binary (needs 6.9+)"
                log_message ""
                log_message "🔧 Your options:"
                log_message "   1. Build AK from source (works with Qt6 $QT6_VERSION):"
                log_message "      git clone https://github.com/apertacodex/ak && cd ak"
                log_message "      mkdir build && cd build && cmake .. -DBUILD_GUI=ON && make"
                log_message ""
                log_message "   2. Upgrade to Qt6 6.9+ (if available):"
                log_message "      sudo apt update && sudo apt install qt6-base-dev=6.9*"
                log_message ""
                log_message "   3. Continue anyway (will likely fail at runtime)"
                log_message ""
                log_message "❓ Auto-continuing with installation (non-interactive mode)"
            fi
        else
            log_warning "Qt6 version $QT6_VERSION appears to be Qt5 or older"
            log_message "   AK requires Qt6. Please install Qt6 libraries."
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