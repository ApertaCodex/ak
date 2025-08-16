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

# Update config.cpp
file(READ "${PROJECT_DIR}/src/core/config.cpp" CONFIG_CONTENT)
string(REGEX REPLACE
    "const std::string AK_VERSION = \"[0-9]+\\.[0-9]+\\.[0-9]+\";"
    "const std::string AK_VERSION = \"${NEW_VERSION}\";"
    NEW_CONFIG_CONTENT
    "${CONFIG_CONTENT}"
)

file(WRITE "${PROJECT_DIR}/src/core/config.cpp" "${NEW_CONFIG_CONTENT}")

message(STATUS "📈 Version: ${MAJOR}.${MINOR}.${PATCH} → ${NEW_VERSION}")
message(STATUS "✅ Updated version to ${NEW_VERSION}")