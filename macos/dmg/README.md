# DMG Disk Image Resources

This directory contains resources for creating professional macOS DMG disk images for AK distribution.

## Files Overview

### Configuration
- **`dmg-layout.json`** - JSON configuration defining DMG appearance, window size, and item positions
- **`dmg-setup.applescript`** - AppleScript for customizing DMG visual layout and positioning

### Content Files
- **`README.txt`** - User-facing documentation included in the DMG
- **`background.png`** - Background image for DMG (to be created)

## DMG Layout

The DMG is configured with:
- **Window Size**: 640x480 pixels
- **Icon Size**: 80x80 pixels with bottom labels
- **Background**: Custom background image
- **Format**: UDZO (compressed) with bzip2 compression

### Item Positioning
- **AK.app**: Positioned at (180, 170) for drag-and-drop to Applications
- **Applications**: Symlink positioned at (460, 170) 
- **README.txt**: Documentation at (180, 280)
- **Uninstall AK.app**: Uninstaller at (460, 280)

## Background Image

Create a `background.png` file (640x480) with:
- Professional appearance matching AK branding
- Clear visual guidance for installation (drag arrow, etc.)
- Proper contrast for icon visibility

## Template Variables

The following placeholders are replaced during build:
- `{{VERSION}}` - Application version (e.g., "3.3.0")
- `{{BUILD_DATE}}` - Build timestamp

## Usage

Use the `../scripts/create-dmg.sh` script to build DMG images. The script will:
1. Create a temporary DMG with proper filesystem
2. Copy app bundle and create Applications symlink
3. Apply visual customization using AppleScript
4. Convert to final compressed format

## Requirements

- **hdiutil** - DMG creation and manipulation
- **osascript** - AppleScript execution for layout customization
- **sips** or **iconutil** - Image processing (optional)

## Notes

- The DMG auto-opens when mounted for better user experience
- Proper file permissions are set for all contents
- The layout supports both Intel and Apple Silicon applications
- Background image should be optimized for file size while maintaining quality