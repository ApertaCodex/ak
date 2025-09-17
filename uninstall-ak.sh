#!/bin/bash
# AK API Key Manager - Uninstall Script

set -e

echo "🧹 Uninstalling AK API Key Manager..."
echo

# Check if running with sudo
if [ "$EUID" -ne 0 ]; then
    echo "❌ This script must be run with sudo privileges"
    echo "💡 Try: sudo ./uninstall-ak.sh"
    exit 1
fi

# Function to handle apt operations that might fail
apt_operation() {
    "$@" || true
}

# Uninstall ak package
echo "📦 Removing AK package..."
apt_operation apt purge -y ak

# Remove repository configuration
echo "🗑️ Removing AK repository..."
apt_operation rm -f /etc/apt/sources.list.d/ak.list

# Remove GPG key
echo "🔑 Removing GPG key..."
apt_operation rm -f /usr/share/keyrings/ak-archive-keyring.gpg

# Remove desktop integration
echo "🖥️ Removing desktop integration..."
apt_operation rm -f /usr/share/applications/ak.desktop
apt_operation rm -f /usr/share/icons/hicolor/scalable/apps/ak.svg

# Remove PNG icons in different sizes
echo "🎨 Removing icon files..."
for size in 16 32 48 64 128 256; do
    apt_operation rm -f "/usr/share/icons/hicolor/${size}x${size}/apps/ak.png"
done

# Update icon cache
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    apt_operation gtk-update-icon-cache -f -t /usr/share/icons/hicolor
fi

# Clean up apt cache
echo "🧹 Cleaning up apt cache..."
apt_operation apt autoremove -y
apt_operation apt clean

# Verify uninstallation
echo "🔍 Verifying uninstallation..."
if ! command -v ak >/dev/null 2>&1 || ! [ -f /usr/bin/ak ]; then
    echo "✅ AK API Key Manager has been successfully uninstalled!"
else
    echo "⚠️ Warning: AK binary still detected at $(which ak 2>/dev/null || echo "unknown location")"
    echo "💡 You may need to restart your shell or manually remove any remaining components."
fi

echo
echo "🗑️ Uninstallation complete!"