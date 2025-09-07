# PKG Installer Resources

This directory contains all resources needed to create professional macOS PKG installers for AK.

## Files Overview

### Core Configuration
- **`Distribution.xml`** - Installer configuration defining packages, choices, and installation logic
- **`scripts/preinstall`** - Pre-installation script for system checks and preparation  
- **`scripts/postinstall`** - Post-installation script for setup and verification

### User Interface Resources
- **`resources/Welcome.html`** - Welcome page shown during installation
- **`resources/LICENSE.txt`** - License agreement text
- **`resources/background.png`** - Background image for installer (to be created)

## Build Process

The PKG creation process involves:

1. **Component Packages**: Create individual packages for CLI and GUI components
2. **Distribution**: Combine components using Distribution.xml
3. **Resources**: Embed welcome page, license, and visual assets
4. **Scripts**: Include pre/post-install automation

## Placeholder Replacement

During build, these placeholders are replaced:
- `{{VERSION}}` - Current application version
- `{{BUNDLE_ID}}` - macOS bundle identifier
- `{{BUILD_DATE}}` - Build timestamp

## Script Permissions

Install scripts must be executable:
```bash
chmod +x scripts/preinstall
chmod +x scripts/postinstall
```

## Usage

Use the `../scripts/create-pkg.sh` script to build the installer package.

## Requirements

- macOS 10.15+ target
- Xcode Command Line Tools
- `pkgbuild` and `productbuild` utilities