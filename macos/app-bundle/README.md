# App Bundle Templates

This directory contains templates for creating native macOS application bundles for AK.

## Files Overview

### Core Templates
- **`Info.plist`** - Application bundle property list defining metadata, capabilities, and system requirements
- **`launcher.sh`** - Launch script that sets up the environment and starts the GUI application

### Bundle Structure

The created app bundle follows the standard macOS application structure:
```
AK.app/
├── Contents/
│   ├── Info.plist              # Application metadata
│   ├── MacOS/                  # Executable binaries
│   │   ├── ak-gui              # GUI application
│   │   ├── ak                  # CLI tool (optional)
│   │   └── launcher            # Launch script
│   ├── Resources/              # Application resources
│   │   ├── ak.icns             # Application icon
│   │   ├── README.md           # Documentation
│   │   ├── LICENSE             # License file
│   │   └── ak.1                # Manual page
│   └── Frameworks/             # Bundled frameworks (optional)
│       └── Qt*.framework       # Qt frameworks
```

## Info.plist Configuration

The Info.plist template includes:

### Basic Information
- **CFBundleDisplayName**: "AK" - Display name in Finder and Dock
- **CFBundleExecutable**: "ak-gui" - Main executable name
- **CFBundleIdentifier**: Template for bundle ID (e.g., `dev.ak.ak`)
- **CFBundleVersion**: Build-specific version number

### System Requirements
- **LSMinimumSystemVersion**: "10.15" - Requires macOS Catalina or later
- **LSArchitecturePriority**: Prefers Apple Silicon, falls back to Intel

### Capabilities
- **Document Types**: Supports `.akprofile` files
- **URL Schemes**: Handles `ak://` protocol URLs
- **High Resolution**: Optimized for Retina displays

### Security Permissions
- **NSAppleEventsUsageDescription**: Permission for system event access
- **NSSystemAdministrationUsageDescription**: Permission for admin operations

## Launcher Script

The launcher script (`launcher.sh`):
1. **Environment Setup**: Configures Qt framework paths and XDG directories
2. **Logging**: Creates debug logs for troubleshooting
3. **Permissions**: Ensures executable has proper permissions
4. **Execution**: Launches the GUI application with proper arguments

## Template Variables

These placeholders are replaced during build:
- `{{VERSION}}` - Application version (e.g., "3.3.0")
- `{{BUNDLE_ID}}` - Bundle identifier (e.g., "dev.ak.ak")
- `{{BUILD_NUMBER}}` - Build-specific number for App Store requirements

## Icon Requirements

The app bundle expects an icon file at `Contents/Resources/ak.icns`:
- **Format**: ICNS (Apple Icon Image format)
- **Sizes**: Multiple resolutions from 16x16 to 1024x1024
- **Retina Support**: Includes @2x variants for high-DPI displays

If ICNS is not available, PNG format is acceptable as fallback.

## Qt Framework Bundling

When using `--include-qt` option:
- Copies required Qt frameworks to `Contents/Frameworks/`
- Updates library paths using `install_name_tool`
- Creates self-contained application bundle
- Enables distribution without Qt installation requirement

## Usage

Create app bundles using the `../scripts/create-app-bundle.sh` script:

```bash
# Basic app bundle
./scripts/create-app-bundle.sh

# Standalone bundle with Qt frameworks
./scripts/create-app-bundle.sh --include-qt

# Custom configuration
./scripts/create-app-bundle.sh \
    --version 3.4.0 \
    --bundle-id "com.example.ak" \
    --name "AK Beta"
```

## Validation

App bundles can be validated using:
```bash
./scripts/validate-packages.sh
```

This checks for:
- Proper bundle structure
- Valid Info.plist format
- Executable permissions
- Code signing status
- Framework dependencies
- Icon presence

## Distribution

App bundles can be distributed via:
1. **Direct Distribution**: Upload `.app` bundle directly
2. **DMG**: Embed in disk image for professional presentation
3. **PKG**: Include in installer package
4. **Mac App Store**: Submit for store distribution (requires additional setup)