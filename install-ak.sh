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

# Function to compare version numbers
version_compare() {
    if [[ $1 == $2 ]]; then
        return 0
    fi
    local IFS=.
    local i ver1=($1) ver2=($2)
    # fill empty fields in ver1 with zeros
    for ((i=${#ver1[@]}; i<${#ver2[@]}; i++)); do
        ver1[i]=0
    done
    for ((i=0; i<${#ver1[@]}; i++)); do
        if [[ -z ${ver2[i]} ]]; then
            # fill empty fields in ver2 with zeros
            ver2[i]=0
        fi
        if ((10#${ver1[i]} > 10#${ver2[i]})); then
            return 1
        fi
        if ((10#${ver1[i]} < 10#${ver2[i]})); then
            return 2
        fi
    done
    return 0
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

REQUIRED_QT_VERSION="6.9.0"

# Install Qt6 dependencies first
echo "📦 Installing Qt6 dependencies..."
sudo apt install -y qt6-base-dev libqt6widgets6 qt6-tools-dev qt6-tools-dev-tools

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
    echo "📍 Required Qt6 version: $REQUIRED_QT_VERSION"
    
    version_compare $QT6_VERSION $REQUIRED_QT_VERSION
    case $? in
        0) echo "✅ Qt6 runtime version is compatible" ;;
        1) echo "✅ Qt6 runtime version is newer than required" ;;
        2)
            echo "⚠️  Warning: Qt6 runtime version $QT6_VERSION is older than required $REQUIRED_QT_VERSION"
            echo "   This will likely cause the 'Qt_6.9 not found' error!"
            echo ""
            echo "🔧 Solutions to fix Qt6 compatibility:"
            echo "   1. Update Qt6 libraries:"
            echo "      sudo apt update && sudo apt install libqt6core6 libqt6widgets6 libqt6gui6"
            echo "   2. Try adding newer Qt6 repository:"
            echo "      sudo add-apt-repository ppa:beineri/opt-qt-5.15.2-focal"
            echo "   3. Build AK from source with your Qt6 version:"
            echo "      git clone https://github.com/apertacodex/ak && cd ak && mkdir build && cd build"
            echo "      cmake .. -DBUILD_GUI=ON && make"
            echo "   4. Use CLI-only version (if you don't need GUI):"
            echo "      Build with: cmake .. -DBUILD_GUI=OFF"
            echo ""
            read -p "❓ Continue installation anyway? (y/N): " -n 1 -r
            echo
            if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                echo "❌ Installation cancelled by user"
                exit 1
            fi
            ;;
    esac
else
    echo "⚠️  Could not detect Qt6 version, proceeding with installation..."
fi

# Install ak
echo "⬇️  Installing AK..."
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