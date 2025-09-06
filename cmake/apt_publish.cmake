# CMake script for APT repository publishing
# Usage: cmake -DPROJECT_DIR=<dir> -P apt_publish.cmake

if(NOT PROJECT_DIR)
    message(FATAL_ERROR "PROJECT_DIR must be specified")
endif()

# Read current version from CMakeLists.txt
file(READ "${PROJECT_DIR}/CMakeLists.txt" CMAKE_CONTENT)
string(REGEX MATCH "set\\(AK_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)\"" VERSION_MATCH ${CMAKE_CONTENT})

if(NOT VERSION_MATCH)
    message(FATAL_ERROR "Could not find version in CMakeLists.txt")
endif()

set(VERSION ${CMAKE_MATCH_1})
set(DEB_VERSION "${VERSION}")
set(REPO_DIR "${PROJECT_DIR}/ak-apt-repo")
set(POOL_DIR "${REPO_DIR}/pool/main")
set(PACKAGES_DIR "${REPO_DIR}/dists/stable/main/binary-amd64")

message(STATUS "🚀 Publishing AK v${VERSION} to APT repository...")

# Check if Debian package exists
set(DEB_FILE "${PROJECT_DIR}/../ak_${DEB_VERSION}_amd64.deb")
if(NOT EXISTS ${DEB_FILE})
    message(STATUS "📦 Building Debian package...")
    
    # Clean previous build
    execute_process(
        COMMAND rm -rf obj-x86_64-linux-gnu debian/.debhelper debian/debhelper-build-stamp
        WORKING_DIRECTORY ${PROJECT_DIR}
    )
    
    # Build Debian package
    execute_process(
        COMMAND dpkg-buildpackage -us -uc -b
        WORKING_DIRECTORY ${PROJECT_DIR}
        RESULT_VARIABLE BUILD_RESULT
        OUTPUT_VARIABLE BUILD_OUTPUT
        ERROR_VARIABLE BUILD_ERROR
    )
    
    if(NOT BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build Debian package: ${BUILD_ERROR}")
    endif()
    
    message(STATUS "✅ Debian package built successfully")
endif()

# Copy package to repository pool
message(STATUS "📦 Updating repository pool...")
execute_process(
    COMMAND cp "${DEB_FILE}" "${POOL_DIR}/"
    RESULT_VARIABLE COPY_RESULT
)

if(NOT COPY_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to copy package to repository pool")
endif()

# Generate Packages file with correct paths
message(STATUS "📋 Regenerating package metadata...")
execute_process(
    COMMAND dpkg-scanpackages --multiversion ../../../../pool/main /dev/null
    WORKING_DIRECTORY ${PACKAGES_DIR}
    OUTPUT_VARIABLE PACKAGES_OUTPUT
    RESULT_VARIABLE SCAN_RESULT
)

if(NOT SCAN_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to scan packages")
endif()

# Fix relative paths in Packages output
string(REGEX REPLACE "Filename: ../../../../" "Filename: " PACKAGES_FIXED ${PACKAGES_OUTPUT})
file(WRITE "${PACKAGES_DIR}/Packages" ${PACKAGES_FIXED})

# Create compressed Packages file
execute_process(
    COMMAND gzip -9c Packages
    WORKING_DIRECTORY ${PACKAGES_DIR}
    OUTPUT_FILE "${PACKAGES_DIR}/Packages.gz"
    RESULT_VARIABLE GZIP_RESULT
)

if(NOT GZIP_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to compress Packages file")
endif()

# Get file sizes and hashes
file(SIZE "${PACKAGES_DIR}/Packages" PACKAGES_SIZE)
file(SIZE "${PACKAGES_DIR}/Packages.gz" PACKAGES_GZ_SIZE)

file(MD5 "${PACKAGES_DIR}/Packages" PACKAGES_MD5)
file(MD5 "${PACKAGES_DIR}/Packages.gz" PACKAGES_GZ_MD5)
file(SHA1 "${PACKAGES_DIR}/Packages" PACKAGES_SHA1)
file(SHA1 "${PACKAGES_DIR}/Packages.gz" PACKAGES_GZ_SHA1)
file(SHA256 "${PACKAGES_DIR}/Packages" PACKAGES_SHA256)
file(SHA256 "${PACKAGES_DIR}/Packages.gz" PACKAGES_GZ_SHA256)

# Generate Release file
message(STATUS "🔐 Generating Release file...")
string(TIMESTAMP CURRENT_DATE "%a, %d %b %Y %H:%M:%S +0000" UTC)

set(RELEASE_CONTENT "Archive: stable
Codename: stable
Components: main
Date: ${CURRENT_DATE}
Description: AK API Key Manager Repository
Label: AK Repository
Origin: AK
Suite: stable
Version: 1.0
Architectures: amd64
MD5Sum:
 ${PACKAGES_MD5} ${PACKAGES_SIZE} main/binary-amd64/Packages
 ${PACKAGES_GZ_MD5} ${PACKAGES_GZ_SIZE} main/binary-amd64/Packages.gz
SHA1:
 ${PACKAGES_SHA1} ${PACKAGES_SIZE} main/binary-amd64/Packages
 ${PACKAGES_GZ_SHA1} ${PACKAGES_GZ_SIZE} main/binary-amd64/Packages.gz
SHA256:
 ${PACKAGES_SHA256} ${PACKAGES_SIZE} main/binary-amd64/Packages
 ${PACKAGES_GZ_SHA256} ${PACKAGES_GZ_SIZE} main/binary-amd64/Packages.gz
")

file(WRITE "${REPO_DIR}/dists/stable/Release" ${RELEASE_CONTENT})

# Sign Release file
message(STATUS "✍️  Signing Release file...")
execute_process(
    COMMAND gpg --yes --clearsign -o InRelease Release
    WORKING_DIRECTORY "${REPO_DIR}/dists/stable"
    INPUT_FILE /dev/null
    RESULT_VARIABLE SIGN1_RESULT
)

execute_process(
    COMMAND gpg --yes --armor --detach-sig -o Release.gpg Release
    WORKING_DIRECTORY "${REPO_DIR}/dists/stable"
    INPUT_FILE /dev/null
    RESULT_VARIABLE SIGN2_RESULT
)

if(NOT SIGN1_RESULT EQUAL 0 OR NOT SIGN2_RESULT EQUAL 0)
    message(WARNING "Failed to sign Release file - continuing without signatures")
endif()

message(STATUS "✅ APT repository updated successfully")
message(STATUS "📍 Repository URL: https://apertacodex.github.io/ak/ak-apt-repo")
message(STATUS "📦 Package: ak_${DEB_VERSION}_amd64.deb")