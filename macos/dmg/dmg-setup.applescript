-- AppleScript to customize DMG appearance
-- This script sets up the visual layout of the disk image

tell application "Finder"
    tell disk "AK {{VERSION}}"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {200, 120, 840, 600}
        set viewOptions to the icon view options of container window
        set arrangement of viewOptions to not arranged
        set icon size of viewOptions to 80
        set text size of viewOptions to 16
        
        -- Set background image
        set background picture of viewOptions to file ".background:background.png"
        
        -- Position items
        set position of item "AK.app" of container window to {180, 170}
        set position of item "Applications" of container window to {460, 170}
        
        -- Position additional items if they exist
        try
            set position of item "README.txt" of container window to {180, 280}
        end try
        
        try
            set position of item "Uninstall AK.app" of container window to {460, 280}
        end try
        
        -- Update the display
        update without registering applications
        delay 2
        
        -- Set icon view options
        set icon size of viewOptions to 80
        set text size of viewOptions to 16
        set shows icon preview of viewOptions to true
        set shows item info of viewOptions to false
        
        close
    end tell
end tell

-- Eject the disk image when done
tell application "Finder"
    eject disk "AK {{VERSION}}"
end tell