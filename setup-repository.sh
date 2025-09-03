#!/bin/bash
# AK API Key Manager Repository Setup
# Quick setup script for adding AK APT repository to Debian/Ubuntu systems

set -e

echo "🔧 Setting up AK API Key Manager repository..."
echo ""

# Check if running on supported system
if ! command -v apt-get &> /dev/null; then
    echo "❌ This script requires a Debian/Ubuntu system with apt-get"
    exit 1
fi

# Add GPG key
echo "📋 Adding GPG signing key..."
curl -fsSL https://apertacodex.github.io/ak/ak-repository-key.gpg | sudo apt-key add -
echo "✅ GPG key added"

# Add repository
echo "📦 Adding AK repository..."
echo "deb https://apertacodex.github.io/ak/ak-apt-repo stable main" | sudo tee /etc/apt/sources.list.d/ak.list
echo "✅ Repository added"

# Update package list
echo "🔄 Updating package lists..."
sudo apt update
echo "✅ Package lists updated"

echo ""
echo "🎉 Repository setup complete!"
echo ""
echo "Install AK with:"
echo "  sudo apt install ak"
echo ""
echo "After installation, verify with:"
echo "  ak --version"
echo "  ak --help"