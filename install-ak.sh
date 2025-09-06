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

# Install Qt6 dependencies first
echo "📦 Installing Qt6 dependencies..."
sudo apt install -y qt6-base-dev libqt6widgets6 qt6-tools-dev qt6-tools-dev-tools

# Install ak
echo "⬇️  Installing AK..."
sudo apt install -y ak

# Verify installation
echo "✅ Installation complete!"
echo
echo "🔍 Verifying installation..."
ak --version

echo
echo "🎉 AK API Key Manager is now installed!"
echo "📚 Run 'ak --help' to get started"
echo "🔧 Run 'ak install-shell' to enable shell completions"