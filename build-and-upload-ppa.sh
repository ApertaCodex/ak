#!/bin/bash
# Special script for building and uploading PPA packages
# This uses a clean temporary repository and preserves ak-apt-repo and ak-macos-repo directories

set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Parse arguments
SERIES="${1:-noble}"
KEYID="${DEBSIGN_KEYID:-}"

# Ensure GPG key is available
if [ -z "${KEYID}" ]; then
    KEYID="$(gpg --list-secret-keys --with-colons 2>/dev/null | awk -F: '$1=="fpr"{print $10; exit}')"
    if [ -z "${KEYID}" ]; then
        echo -e "${RED}No GPG secret key found. Set DEBSIGN_KEYID or create/import a key.${NC}"
        echo "Example: gpg --full-generate-key"
        exit 1
    fi
fi

# Create a temporary directory for the clean repository
TEMP_DIR=$(mktemp -d)
ORIG_DIR=$(pwd)

echo -e "${BLUE}Creating clean repository copy in ${TEMP_DIR}...${NC}"

# Create a clean copy using git archive
git archive --format=tar HEAD | tar -x -C "${TEMP_DIR}"

# Change to the temp directory
cd "${TEMP_DIR}"

# Ensure repository directories exist (do not delete them)
mkdir -p ak-apt-repo ak-macos-repo

# Ensure we have the correct debian/source/format
echo "3.0 (native)" > debian/source/format

# Update changelog to match current version
VERSION=$(grep -o '"[0-9]\+\.[0-9]\+\.[0-9]\+"' CMakeLists.txt | head -1 | tr -d '"')
echo -e "${BLUE}Building version ${VERSION} for ${SERIES}...${NC}"

# Build the source package
echo -e "${YELLOW}Building source package...${NC}"
dpkg-buildpackage -S -sa -k"${KEYID}"

# Find the changes file
CHANGES_FILE=$(ls -t ../ak_*.changes | head -1)

if [ -z "${CHANGES_FILE}" ]; then
    echo -e "${RED}Failed to find changes file.${NC}"
    exit 1
fi

echo -e "${BLUE}Source package built: ${CHANGES_FILE}${NC}"

# Upload to PPA
echo -e "${YELLOW}Uploading to PPA...${NC}"
dput "ppa:apertacodex/ak" "${CHANGES_FILE}"

echo -e "${GREEN}✅ Package uploaded to PPA!${NC}"
echo "Monitor build status at https://launchpad.net/~apertacodex/+archive/ubuntu/ak"

# Copy built files back to original directory
cp ../*.changes ../*.dsc ../*.tar.* "${ORIG_DIR}/" 2>/dev/null || true

# Clean up
cd "${ORIG_DIR}"
rm -rf "${TEMP_DIR}"

echo -e "${GREEN}Build artifacts copied to ${ORIG_DIR}${NC}"
