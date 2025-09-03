#!/bin/bash
# AK API Key Manager Repository Setup
# Run this script on user machines to add the AK repository

set -e

echo "🔧 Setting up AK API Key Manager repository..."

# Add GPG key
if [ -f "ak-repository-key.gpg" ]; then
    sudo apt-key add ak-repository-key.gpg
    echo "✅ GPG key added"
else
    echo "❌ GPG key file not found. Please ensure ak-repository-key.gpg is in the current directory."
    exit 1
fi

# Add repository (you'll need to update this URL)
echo "deb https://your-username.github.io/ak-apt-repo stable main" | sudo tee /etc/apt/sources.list.d/ak.list

# Update package list
sudo apt update

echo "✅ Repository setup complete!"
echo "📦 Install AK with: sudo apt install ak"
