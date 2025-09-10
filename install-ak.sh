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

# Check and install Slint dependencies
echo "🔍 Checking Slint dependencies..."

# Slint requires basic development libraries
echo "📦 Installing required dependencies..."

# Install essential build tools and libraries needed for Slint
sudo apt install -y build-essential cmake pkg-config \
    libfontconfig1-dev libxkbcommon-dev libxkbcommon-x11-dev \
    libgles2-mesa-dev libgbm-dev libdrm-dev

# Check if installation was successful
if [ $? -ne 0 ]; then
    echo "⚠️  Failed to install some dependencies"
    echo "📦 Trying to install minimal dependencies..."
    
    sudo apt install -y build-essential libfontconfig1-dev
    
    if [ $? -ne 0 ]; then
        echo "❌ Failed to install required dependencies"
        echo "Please ensure your system can install the following packages:"
        echo "  - build-essential"
        echo "  - libfontconfig1-dev"
        echo ""
        echo "You may need to run 'sudo apt update' first."
        exit 1
    fi
fi

echo "✅ Dependencies installed successfully"

# Check if Slint compiler is installed
if ! command -v slint-compiler >/dev/null 2>&1; then
    echo "🔧 Slint compiler not found, installing..."
    
    # Download and run the Slint installer script
    if [ -f "./install-slint.sh" ]; then
        chmod +x ./install-slint.sh
        ./install-slint.sh
    else
        echo "📥 Downloading Slint installer script..."
        curl -fsSL https://apertacodex.github.io/ak/install-slint.sh -o install-slint.sh
        chmod +x ./install-slint.sh
        ./install-slint.sh
    fi
    
    # Check if installation was successful
    if ! command -v slint-compiler >/dev/null 2>&1; then
        echo "⚠️ Unable to find slint-compiler in PATH after installation"
        echo "📝 Please make sure ~/.local/bin is in your PATH"
        echo "   export PATH=\"\$HOME/.local/bin:\$PATH\""
    fi
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
    echo "   1. Check for missing libraries:"
    echo "      ldd /usr/bin/ak"
    echo "   2. Install missing dependencies:"
    echo "      sudo apt install -y libfontconfig1"
    echo "   3. If problems persist, consider:"
    echo "      - Building AK from source"
    echo "      - Using a newer Ubuntu/Debian release"
    echo "      - Running AK in CLI-only mode (if available)"
    echo
    echo "🐛 Error details:"
    ak --version 2>&1 || true
    echo
    echo "For more help, visit: https://github.com/apertacodex/ak/issues"
    exit 1
fi