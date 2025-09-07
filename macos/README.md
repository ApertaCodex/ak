# macOS Package Creation Infrastructure

This directory contains all the resources and scripts needed to create professional macOS packages for the AK application.

## Directory Structure

- **`installer/`** - PKG installer resources and templates
- **`dmg/`** - DMG disk image creation resources and layouts  
- **`app-bundle/`** - App Bundle templates and configuration
- **`scripts/`** - Core packaging automation scripts

## Package Types

### 1. PKG Installer (.pkg)
Professional installer package that can be distributed through the Mac App Store or directly to users. Includes pre/post-install scripts and custom installer appearance.

### 2. DMG Disk Image (.dmg)
Drag-and-drop disk image with custom background, icons, and layout for easy installation by end users.

### 3. App Bundle (.app)
Native macOS application bundle that can be run directly or embedded in other package types.

## Usage

See individual README files in each subdirectory for specific usage instructions.

## Requirements

- macOS 10.15+ (Catalina)
- Xcode Command Line Tools
- CMake 3.16+
- Qt6 (for GUI builds)

## Integration

These scripts are designed to integrate with the existing CMake build system. Build placeholders like `{{VERSION}}` and `{{BUNDLE_ID}}` will be replaced during the build process.