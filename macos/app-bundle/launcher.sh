#!/bin/bash

# AK GUI Application Launcher
# This script launches the AK GUI application with proper environment setup

set -e

# Get the app bundle path
APP_PATH="$(dirname "$(dirname "$(realpath "$0")")")"
CONTENTS_PATH="$APP_PATH/Contents"
MACOS_PATH="$CONTENTS_PATH/MacOS"
RESOURCES_PATH="$CONTENTS_PATH/Resources"

# Set up environment variables
export AK_BUNDLE_PATH="$APP_PATH"
export AK_RESOURCES_PATH="$RESOURCES_PATH"

# Add Qt libraries to library path if they exist
if [[ -d "$CONTENTS_PATH/Frameworks" ]]; then
    export DYLD_FRAMEWORK_PATH="$CONTENTS_PATH/Frameworks:$DYLD_FRAMEWORK_PATH"
fi

# Set up XDG directories for configuration
export XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
export XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-$HOME/.cache}"

# Create config directory if it doesn't exist
mkdir -p "$XDG_CONFIG_HOME/ak"

# Log startup (for debugging)
LOG_FILE="$XDG_CACHE_HOME/ak/gui.log"
mkdir -p "$(dirname "$LOG_FILE")"
echo "$(date '+%Y-%m-%d %H:%M:%S') - Starting AK GUI from bundle: $APP_PATH" >> "$LOG_FILE"

# Check for GUI executable
GUI_EXECUTABLE="$MACOS_PATH/ak-gui"
if [[ ! -f "$GUI_EXECUTABLE" ]]; then
    echo "Error: GUI executable not found at $GUI_EXECUTABLE" >&2
    echo "$(date '+%Y-%m-%d %H:%M:%S') - ERROR: GUI executable not found" >> "$LOG_FILE"
    exit 1
fi

# Make sure executable has proper permissions
chmod +x "$GUI_EXECUTABLE"

# Launch the GUI application
echo "$(date '+%Y-%m-%d %H:%M:%S') - Launching GUI executable" >> "$LOG_FILE"
exec "$GUI_EXECUTABLE" "$@"