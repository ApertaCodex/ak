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

# Install wxWidgets dependencies
echo "📦 Installing wxWidgets dependencies..."

# Try to detect distribution for better package names
DISTRO=""
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
fi

# Install wxWidgets based on the distribution
case $DISTRO in
    ubuntu|debian|pop|mint|elementary)
        echo "🔍 Detected Debian/Ubuntu-based distribution"
        sudo apt install -y libwxgtk3.2-dev
        ;;
    *)
        # Default to Ubuntu/Debian package
        echo "🔍 Using default wxWidgets package for Debian/Ubuntu"
        sudo apt install -y libwxgtk3.0-gtk3-dev
        ;;
esac

# Verify wxWidgets installation
if wx-config --version >/dev/null 2>&1; then
    WX_VERSION=$(wx-config --version)
    echo "✅ wxWidgets $WX_VERSION installed successfully"
else
    echo "⚠️ Could not detect wxWidgets version, but proceeding with installation..."
fi

# Install ak
echo "⬇️ Installing AK..."
sudo apt install -y ak

# Verify installation
echo "✅ Installation complete!"
echo
echo "🔍 Verifying installation..."

if ak --version 2>/dev/null; then
    echo
    echo "🎉 AK API Key Manager is now installed and working!"
    echo "📚 Run 'ak --help' to get started"
    echo "🔧 Run 'ak install-shell' to enable shell completions"
else
    echo "❌ Installation verification failed!"
    echo
    echo "🔧 Troubleshooting steps:"
    echo "   1. Check wxWidgets installation:"
    echo "      wx-config --version"
    echo "   2. Try installing wxWidgets manually:"
    echo "      sudo apt update && sudo apt install libwxgtk3.0-gtk3-dev"
    echo "   3. If wxWidgets is not available, consider:"
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