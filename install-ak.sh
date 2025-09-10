#!/bin/bash
# AK API Key Manager - Quick Install Script
# Alternative to the problematic Launchpad PPA

set -e

echo "🚀 Installing AK API Key Manager..."
echo "📍 Using GitHub Pages repository (recommended)"
echo

# Check if running on supported system
if ! command -v apt >/dev/null 2>&1; then
    echo "❌ This installer requires apt (Ubuntu/Debian)"
    echo "📖 See README.md for other installation methods"
    exit 1
fi

# Add GPG key
echo "🔐 Adding GPG key..."
sudo mkdir -p /usr/share/keyrings
curl -fsSL https://apertacodex.github.io/ak/ak-repository-key.gpg | sudo tee /usr/share/keyrings/ak-archive-keyring.gpg > /dev/null

# Add repository  
echo "📦 Adding AK repository..."
echo "deb [signed-by=/usr/share/keyrings/ak-archive-keyring.gpg] https://apertacodex.github.io/ak/ak-apt-repo stable main" | sudo tee /etc/apt/sources.list.d/ak.list

# Update package list
echo "🔄 Updating package list..."
sudo apt update

# Check Qt6 version compatibility
echo "🔍 Checking Qt6 version requirements..."

# Simple version check - just ensure Qt6 major version is >= 6
is_qt6_compatible() {
    local version=$1
    local major_version=$(echo "$version" | cut -d. -f1)
    
    if [[ $major_version -ge 6 ]]; then
        return 0  # compatible
    else
        return 1  # not compatible
    fi
}

# Get Qt6 runtime version from installed packages (most reliable)
QT6_VERSION=""
if dpkg-query -W -f='${Version}' libqt6core6t64 >/dev/null 2>&1; then
    QT6_VERSION=$(dpkg-query -W -f='${Version}' libqt6core6t64 | cut -d'+' -f1 | cut -d'-' -f1)
elif dpkg-query -W -f='${Version}' libqt6core6 >/dev/null 2>&1; then
    QT6_VERSION=$(dpkg-query -W -f='${Version}' libqt6core6 | cut -d'+' -f1 | cut -d'-' -f1)
elif dpkg-query -W -f='${Version}' qt6-base-dev >/dev/null 2>&1; then
    QT6_VERSION=$(dpkg-query -W -f='${Version}' qt6-base-dev | cut -d'+' -f1 | cut -d'-' -f1)
elif command -v qmake6 >/dev/null 2>&1; then
    # Fallback to qmake6, but this may not reflect runtime libraries
    QT6_VERSION=$(qmake6 -query QT_VERSION 2>/dev/null)
fi

REQUIRED_QT_VERSION="6.8.0"  # Minimum for building from source
BINARY_REQUIRES_QT_VERSION="6.9.0"  # Published binary requirement

# Try to install Qt6 6.9.0 specifically first
echo "📦 Installing Qt6 6.9.0 dependencies..."
echo "🔄 Trying to install Qt6 6.9.0..."

if sudo apt install -y qt6-base-dev=6.9* libqt6widgets6=6.9* qt6-tools-dev=6.9* qt6-tools-dev-tools=6.9* 2>/dev/null; then
    echo "✅ Successfully installed Qt6 6.9.x"
else
    echo "⚠️  Qt6 6.9.x not available in repositories"
    echo "📦 Installing available Qt6 version..."
    sudo apt install -y qt6-base-dev libqt6widgets6 qt6-tools-dev qt6-tools-dev-tools
    
    echo ""
    echo "🔧 To get Qt6 6.9.0, you can try:"
    echo "   1. Add Qt's official repository:"
    echo "      wget -qO - https://download.qt.io/official_releases/qt/6.9/6.9.0/submodules/apt-key.gpg | sudo apt-key add -"
    echo "      echo 'deb https://download.qt.io/official_releases/qt/6.9/6.9.0/submodules/ focal main' | sudo tee /etc/apt/sources.list.d/qt6.list"
    echo "      sudo apt update && sudo apt install qt6-base-dev=6.9*"
    echo ""
    echo "   2. Or build AK from source to use your current Qt6 version"
    echo ""
fi

# Re-check Qt6 runtime version after installation
if dpkg-query -W -f='${Version}' libqt6core6t64 >/dev/null 2>&1; then
    QT6_VERSION=$(dpkg-query -W -f='${Version}' libqt6core6t64 | cut -d'+' -f1 | cut -d'-' -f1)
elif dpkg-query -W -f='${Version}' libqt6core6 >/dev/null 2>&1; then
    QT6_VERSION=$(dpkg-query -W -f='${Version}' libqt6core6 | cut -d'+' -f1 | cut -d'-' -f1)
elif dpkg-query -W -f='${Version}' qt6-base-dev >/dev/null 2>&1; then
    QT6_VERSION=$(dpkg-query -W -f='${Version}' qt6-base-dev | cut -d'+' -f1 | cut -d'-' -f1)
elif command -v qmake6 >/dev/null 2>&1; then
    QT6_VERSION=$(qmake6 -query QT_VERSION 2>/dev/null)
fi

if [[ -n "$QT6_VERSION" ]]; then
    echo "📍 Qt6 runtime version detected: $QT6_VERSION"
    echo "📍 Binary requires Qt6 version: $BINARY_REQUIRES_QT_VERSION"
    echo "📍 Source build minimum: $REQUIRED_QT_VERSION"
    
    if is_qt6_compatible "$QT6_VERSION"; then
        # Check if version is compatible with published binary
        major_minor=$(echo "$QT6_VERSION" | cut -d. -f1,2)
        if [[ "$major_minor" == "6.9" ]] || [[ "$major_minor" > "6.9" ]]; then
            echo "✅ Qt6 $QT6_VERSION detected - compatible with published AK binary"
        else
            echo "⚠️  Qt6 $QT6_VERSION detected - INCOMPATIBLE with published binary (needs 6.9+)"
            echo ""
            echo "🔧 Your options:"
            echo "   1. Build AK from source (works with Qt6 $QT6_VERSION):"
            echo "      git clone https://github.com/apertacodex/ak && cd ak"
            echo "      mkdir build && cd build && cmake .. -DBUILD_GUI=ON && make"
            echo ""
            echo "   2. Upgrade to Qt6 6.9+ (if available):"
            echo "      sudo apt update && sudo apt install qt6-base-dev=6.9*"
            echo ""
            echo "   3. Continue anyway (will likely fail at runtime)"
            echo ""
            read -p "❓ Continue with binary installation anyway? (y/N): " -n 1 -r
            echo
            if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                echo "❌ Installation cancelled by user"
                echo "💡 Consider building from source for Qt6 $QT6_VERSION compatibility"
                exit 1
            fi
        fi
    else
        echo "⚠️  Warning: Qt6 version $QT6_VERSION appears to be Qt5 or older"
        echo "   AK requires Qt6. Please install Qt6 libraries."
    fi
else
    echo "⚠️  Could not detect Qt6 version, proceeding with installation..."
fi

# Install ak
echo "⬇️  Installing AK..."
sudo apt install -y ak

# Install desktop integration
echo "🖥️  Installing desktop integration..."

# Ensure desktop file is installed
if [ ! -f /usr/share/applications/ak.desktop ]; then
    echo "📄 Installing desktop file..."
    sudo mkdir -p /usr/share/applications
    sudo cp -f "${0%/*}/ak.desktop" /usr/share/applications/ 2>/dev/null || \
    sudo curl -fsSL https://apertacodex.github.io/ak/ak.desktop -o /usr/share/applications/ak.desktop
fi

# Ensure icon is installed
echo "🎨 Installing application icon..."
sudo mkdir -p /usr/share/icons/hicolor/scalable/apps
if [ -f "${0%/*}/logo.svg" ]; then
    sudo cp -f "${0%/*}/logo.svg" /usr/share/icons/hicolor/scalable/apps/ak.svg
else
    sudo curl -fsSL https://apertacodex.github.io/ak/logo.svg -o /usr/share/icons/hicolor/scalable/apps/ak.svg
fi

# Install PNG icons in different sizes if available
if [ -f "${0%/*}/logo.png" ]; then
    for size in 16 32 48 64 128 256; do
        sudo mkdir -p "/usr/share/icons/hicolor/${size}x${size}/apps"
        # Use convert from ImageMagick if available
        if command -v convert >/dev/null 2>&1; then
            convert "${0%/*}/logo.png" -resize ${size}x${size} "/usr/share/icons/hicolor/${size}x${size}/apps/ak.png"
        else
            # Otherwise just copy the original to all sizes
            sudo cp -f "${0%/*}/logo.png" "/usr/share/icons/hicolor/${size}x${size}/apps/ak.png"
        fi
    done
fi

# Update icon cache
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    sudo gtk-update-icon-cache -f -t /usr/share/icons/hicolor
fi

# Verify installation
echo "✅ Installation complete!"
echo
echo "🔍 Verifying installation..."

if ak --version 2>/dev/null; then
    echo
    echo "🎉 AK API Key Manager is now installed and working!"
    echo "📚 Run 'ak --help' to get started"
    echo "🔧 Run 'ak install-shell' to enable shell completions"
    echo "🖥️  A desktop shortcut has been created in your applications menu"
else
    echo "❌ Installation verification failed!"
    echo
    echo "🔧 Troubleshooting steps:"
    echo "   1. Check Qt6 version compatibility:"
    echo "      qmake6 -query QT_VERSION"
    echo "   2. Try installing a newer Qt6 version:"
    echo "      sudo apt update && sudo apt install qt6-base-dev=6.9* || sudo apt install qt6-base-dev"
    echo "   3. Check available Qt6 versions:"
    echo "      apt list --upgradable | grep qt6"
    echo "   4. If Qt6 6.9+ is not available, consider:"
    echo "      - Building AK from source with your Qt6 version"
    echo "      - Using a newer Ubuntu/Debian release"
    echo "      - Running AK in CLI-only mode (if available)"
    echo
    echo "🐛 Error details:"
    ak --version 2>&1 || true
    echo
    echo "For more help, visit: https://github.com/apertacodex/ak/issues"
    exit 1
fi