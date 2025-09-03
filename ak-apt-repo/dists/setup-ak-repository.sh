#!/bin/bash
# AK API Key Manager Repository Setup
# Run this script on user machines to add the AK repository

set -e

echo "🔧 Setting up AK API Key Manager repository..."

# Add GPG key
curl -fsSL https://apertacodex.github.io/ak/ak-repository-key.gpg | sudo apt-key add -
echo "✅ GPG key added"

# Add repository
echo "deb https://apertacodex.github.io/ak/ak-apt-repo stable main" | sudo tee /etc/apt/sources.list.d/ak.list
echo "✅ Repository added"

# Update package list
sudo apt update

echo "✅ Repository setup complete!"
echo "📦 Install AK with: sudo apt install ak"
