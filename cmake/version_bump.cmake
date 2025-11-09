# CMake script for version bumping
# Usage: cmake -DPROJECT_DIR=<dir> -DBUMP_TYPE=<patch|minor|major> -P version_bump.cmake

if(NOT PROJECT_DIR)
    message(FATAL_ERROR "PROJECT_DIR must be specified")
endif()

if(NOT BUMP_TYPE)
    message(FATAL_ERROR "BUMP_TYPE must be specified (patch, minor, or major)")
endif()

# Read current version from CMakeLists.txt
file(READ "${PROJECT_DIR}/CMakeLists.txt" CMAKE_CONTENT)
string(REGEX MATCH "set\\(AK_VERSION \"([0-9]+)\\.([0-9]+)\\.([0-9]+)\"" VERSION_MATCH ${CMAKE_CONTENT})

if(NOT VERSION_MATCH)
    message(FATAL_ERROR "Could not find version in CMakeLists.txt")
endif()

set(MAJOR ${CMAKE_MATCH_1})
set(MINOR ${CMAKE_MATCH_2})
set(PATCH ${CMAKE_MATCH_3})

# Calculate new version
if(BUMP_TYPE STREQUAL "patch")
    math(EXPR NEW_PATCH "${PATCH} + 1")
    set(NEW_VERSION "${MAJOR}.${MINOR}.${NEW_PATCH}")
elseif(BUMP_TYPE STREQUAL "minor")
    math(EXPR NEW_MINOR "${MINOR} + 1")
    set(NEW_VERSION "${MAJOR}.${NEW_MINOR}.0")
elseif(BUMP_TYPE STREQUAL "major")
    math(EXPR NEW_MAJOR "${MAJOR} + 1")
    set(NEW_VERSION "${NEW_MAJOR}.0.0")
else()
    message(FATAL_ERROR "Invalid BUMP_TYPE: ${BUMP_TYPE}")
endif()

# Update CMakeLists.txt
string(REGEX REPLACE 
    "set\\(AK_VERSION \"[0-9]+\\.[0-9]+\\.[0-9]+\"" 
    "set(AK_VERSION \"${NEW_VERSION}\""
    NEW_CMAKE_CONTENT 
    "${CMAKE_CONTENT}"
)

file(WRITE "${PROJECT_DIR}/CMakeLists.txt" "${NEW_CMAKE_CONTENT}")

# Update config.cpp fallback version
file(READ "${PROJECT_DIR}/src/core/config.cpp" CONFIG_CONTENT)
string(REGEX REPLACE
    "#define AK_VERSION_STRING \"[0-9]+\\.[0-9]+\\.[0-9]+\""
    "#define AK_VERSION_STRING \"${NEW_VERSION}\""
    NEW_CONFIG_CONTENT
    "${CONFIG_CONTENT}"
)
file(WRITE "${PROJECT_DIR}/src/core/config.cpp" "${NEW_CONFIG_CONTENT}")

# Update all other version references
set(FILES_TO_UPDATE
    "CITATION.cff:version: \"([0-9]+\\.[0-9]+\\.[0-9]+)\""
    "codemeta.json:\"version\": \"([0-9]+\\.[0-9]+\\.[0-9]+)\""
    "docs/ak.1:AK version ([0-9]+\\.[0-9]+\\.[0-9]+)"
    "docs/MANUAL.md:AK version ([0-9]+\\.[0-9]+\\.[0-9]+)"
    "Makefile:VERSION[ \t]*\\?=[ \t]*([0-9]+\\.[0-9]+\\.[0-9]+)"
    "Formula/ak.rb:version \"([0-9]+\\.[0-9]+\\.[0-9]+)\""
    "macos/homebrew/generated/ak.rb:version \"([0-9]+\\.[0-9]+\\.[0-9]+)\""
    "ak-apt-repo/index.html:Latest version: ([0-9]+\\.[0-9]+\\.[0-9]+)"
    "ak-macos-repo/index.html:Latest version: ([0-9]+\\.[0-9]+\\.[0-9]+)"
    "README.md:- \\*\\*Version\\*\\*: ([0-9]+\\.[0-9]+\\.[0-9]+)"
    "README.md:ak_([0-9]+\\.[0-9]+\\.[0-9]+)_amd64.deb"
    "windows-installer.nsi:VIProductVersion \"([0-9]+\\.[0-9]+\\.[0-9]+)\\.0\""
    "windows-installer.nsi:VIAddVersionKey \"FileVersion\" \"([0-9]+\\.[0-9]+\\.[0-9]+)\""
    "windows-installer.nsi:VIAddVersionKey \"ProductVersion\" \"([0-9]+\\.[0-9]+\\.[0-9]+)\""
    "windows-installer.nsi:WriteRegStr HKLM \"Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\AK\" \"DisplayVersion\" \"([0-9]+\\.[0-9]+\\.[0-9]+)\""
    "debian/changelog:ak \\(([0-9]+\\.[0-9]+\\.[0-9]+).*\\)"
)

foreach(FILE_PATTERN ${FILES_TO_UPDATE})
    string(REPLACE ":" ";" PARTS ${FILE_PATTERN})
    list(GET PARTS 0 FILE_PATH)
    list(GET PARTS 1 REGEX_PATTERN)
    
    if(EXISTS "${PROJECT_DIR}/${FILE_PATH}")
        file(READ "${PROJECT_DIR}/${FILE_PATH}" FILE_CONTENT)
        
        if("${FILE_PATH}" STREQUAL "CITATION.cff")
            string(REGEX REPLACE "version: \"[0-9]+\\.[0-9]+\\.[0-9]+\"" "version: \"${NEW_VERSION}\"" NEW_FILE_CONTENT "${FILE_CONTENT}")
        elseif("${FILE_PATH}" STREQUAL "codemeta.json")
            string(REGEX REPLACE "\"version\": \"[0-9]+\\.[0-9]+\\.[0-9]+\"" "\"version\": \"${NEW_VERSION}\"" NEW_FILE_CONTENT "${FILE_CONTENT}")
        elseif("${FILE_PATH}" MATCHES "docs/(ak\\.1|MANUAL\\.md)")
            string(REGEX REPLACE "AK version [0-9]+\\.[0-9]+\\.[0-9]+" "AK version ${NEW_VERSION}" NEW_FILE_CONTENT "${FILE_CONTENT}")
        elseif("${FILE_PATH}" STREQUAL "Makefile")
            string(REGEX REPLACE "VERSION[ \t]*\\?=[ \t]*[0-9]+\\.[0-9]+\\.[0-9]+" "VERSION   ?= ${NEW_VERSION}" NEW_FILE_CONTENT "${FILE_CONTENT}")
        elseif("${FILE_PATH}" MATCHES ".*\\.rb$")
            string(REGEX REPLACE "version \"[0-9]+\\.[0-9]+\\.[0-9]+\"" "version \"${NEW_VERSION}\"" NEW_FILE_CONTENT "${FILE_CONTENT}")
            string(REGEX REPLACE "/archive/v[0-9]+\\.[0-9]+\\.[0-9]+\\.tar\\.gz" "/archive/v${NEW_VERSION}.tar.gz" NEW_FILE_CONTENT "${NEW_FILE_CONTENT}")
        elseif("${FILE_PATH}" STREQUAL "ak-apt-repo/index.html")
            string(REGEX REPLACE "Latest version: [0-9]+\\.[0-9]+\\.[0-9]+" "Latest version: ${NEW_VERSION}" NEW_FILE_CONTENT "${FILE_CONTENT}")
        elseif("${FILE_PATH}" STREQUAL "ak-macos-repo/index.html")
            string(REGEX REPLACE "Latest version: [0-9]+\\.[0-9]+\\.[0-9]+" "Latest version: ${NEW_VERSION}" NEW_FILE_CONTENT "${FILE_CONTENT}")
        elseif("${FILE_PATH}" STREQUAL "README.md")
            string(REGEX REPLACE "- \\*\\*Version\\*\\*: [0-9]+\\.[0-9]+\\.[0-9]+"
                                  "- **Version**: ${NEW_VERSION}"
                                  NEW_FILE_CONTENT
                                  "${FILE_CONTENT}")
            string(REGEX REPLACE "ak_[0-9]+\\.[0-9]+\\.[0-9]+_amd64\\.deb"
                                  "ak_${NEW_VERSION}_amd64.deb"
                                  NEW_FILE_CONTENT
                                  "${NEW_FILE_CONTENT}")
        elseif("${FILE_PATH}" STREQUAL "windows-installer.nsi")
            string(REGEX REPLACE "VIProductVersion \"[0-9]+\\.[0-9]+\\.[0-9]+\\.0\""
                                  "VIProductVersion \"${NEW_VERSION}.0\""
                                  NEW_FILE_CONTENT
                                  "${FILE_CONTENT}")
            string(REGEX REPLACE "VIAddVersionKey \"FileVersion\" \"[0-9]+\\.[0-9]+\\.[0-9]+\""
                                  "VIAddVersionKey \"FileVersion\" \"${NEW_VERSION}\""
                                  NEW_FILE_CONTENT
                                  "${NEW_FILE_CONTENT}")
            string(REGEX REPLACE "VIAddVersionKey \"ProductVersion\" \"[0-9]+\\.[0-9]+\\.[0-9]+\""
                                  "VIAddVersionKey \"ProductVersion\" \"${NEW_VERSION}\""
                                  NEW_FILE_CONTENT
                                  "${NEW_FILE_CONTENT}")
            string(REGEX REPLACE "WriteRegStr HKLM \"Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\AK\" \"DisplayVersion\" \"[0-9]+\\.[0-9]+\\.[0-9]+\""
                                  "WriteRegStr HKLM \"Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\AK\" \"DisplayVersion\" \"${NEW_VERSION}\""
                                  NEW_FILE_CONTENT
                                  "${NEW_FILE_CONTENT}")
        elseif("${FILE_PATH}" STREQUAL "debian/changelog")
            string(REGEX REPLACE "ak \\([0-9]+\\.[0-9]+\\.[0-9]+"
                                  "ak (${NEW_VERSION}"
                                  NEW_FILE_CONTENT
                                  "${FILE_CONTENT}")
        endif()
        
        file(WRITE "${PROJECT_DIR}/${FILE_PATH}" "${NEW_FILE_CONTENT}")
        message(STATUS "📝 Updated ${FILE_PATH}")
    endif()
endforeach()

# Update macOS script default versions
# Note: These scripts use shell variable syntax that's complex to update via CMake regex
# The scripts will use the version from CMakeLists.txt at build time, so manual update is optional
# set(MACOS_SCRIPTS
#     "macos/scripts/create-app-bundle.sh"
#     "macos/scripts/create-dmg.sh"
#     "macos/scripts/create-pkg.sh"
#     "macos/scripts/package-all.sh"
# )
# macOS scripts will pick up version from environment or CMakeLists.txt automatically

message(STATUS "📈 Version: ${MAJOR}.${MINOR}.${PATCH} → ${NEW_VERSION}")
message(STATUS "✅ Updated version to ${NEW_VERSION} in all files")
