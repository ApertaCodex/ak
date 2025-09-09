#!/bin/bash
# AK API Key Manager Repository Setup
# Run this script on user machines to add the AK repository

set -e

echo "🔧 Setting up AK API Key Manager repository..."

# Add GPG key (modern method for newer systems)
sudo mkdir -p /usr/share/keyrings
curl -fsSL https://apertacodex.github.io/ak/ak-repository-key.gpg | sudo tee /usr/share/keyrings/ak-archive-keyring.gpg > /dev/null
echo "✅ GPG key added"

# Add repository with keyring specification
echo "deb [signed-by=/usr/share/keyrings/ak-archive-keyring.gpg] https://apertacodex.github.io/ak/ak-apt-repo/ stable main" | sudo tee /etc/apt/sources.list.d/ak.list
echo "✅ Repository added"

# Create a backup of existing sources (if any)
if [ -f /etc/apt/sources.list.d/apertacodex-ubuntu-ak-plucky.sources ]; then
  sudo mv /etc/apt/sources.list.d/apertacodex-ubuntu-ak-plucky.sources /etc/apt/sources.list.d/apertacodex-ubuntu-ak-plucky.sources.bak
  echo "✅ Moved existing PPA source file to backup"
fi

# Update package list
sudo apt update

echo "✅ Repository setup complete!"
echo "📦 Install AK with: sudo apt install ak"
