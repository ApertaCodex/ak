#!/bin/bash
# Automated Release Script for AK API Key Manager
# Handles version bumping, building, APT repository updates, and publishing

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default to patch release
RELEASE_TYPE="${1:-patch}"

echo -e "${BLUE}🚀 AK Automated Release Process${NC}"
echo "================================="
echo ""

# Validate release type
if [[ ! "$RELEASE_TYPE" =~ ^(patch|minor|major)$ ]]; then
    echo -e "${RED}❌ Invalid release type: $RELEASE_TYPE${NC}"
    echo "Usage: $0 [patch|minor|major]"
    echo "Examples:"
    echo "  $0           # Patch release (default)"
    echo "  $0 patch     # Patch release (2.1.0 -> 2.1.1)"
    echo "  $0 minor     # Minor release (2.1.0 -> 2.2.0)" 
    echo "  $0 major     # Major release (2.1.0 -> 3.0.0)"
    exit 1
fi

echo -e "${YELLOW}📋 Release Type: $RELEASE_TYPE${NC}"
echo ""

# Create build directory if needed
if [ ! -d "build" ]; then
    echo -e "${BLUE}🏗️  Creating build directory...${NC}"
    mkdir build
fi

cd build

# Run the automated release process
echo -e "${BLUE}🔄 Running automated release process...${NC}"
echo ""

echo -e "${YELLOW}Step 1: Version Bump${NC}"
cmake --build . --target "bump-$RELEASE_TYPE"

echo ""
echo -e "${YELLOW}Step 2: Testing${NC}"
cmake --build . --target test-release

echo ""
echo -e "${YELLOW}Step 3: Building${NC}" 
cmake --build . --target build-release

echo ""
echo -e "${YELLOW}Step 4: APT Repository Update${NC}"
cmake --build . --target update-apt-repo

echo ""
echo -e "${YELLOW}Step 5: Git Commit & Push${NC}"
cmake --build . --target commit-and-push

echo ""
echo -e "${YELLOW}Step 6: PPA Upload${NC}"
# Attempt to automatically detect GPG key ID if not set
if [ -z "${DEBSIGN_KEYID}" ]; then
    DETECTED_KEYID="$(gpg --list-secret-keys --with-colons 2>/dev/null | awk -F: '$1=="fpr"{print $10; exit}')"
    if [ -n "${DETECTED_KEYID}" ]; then
        export DEBSIGN_KEYID="${DETECTED_KEYID}"
        echo -e "${GREEN}✅ Automatically detected GPG key ID: ${DEBSIGN_KEYID}${NC}"
    else
        echo -e "${RED}❌ Error: DEBSIGN_KEYID is not set and could not be automatically detected.${NC}"
        echo -e "${RED}Please set it manually: export DEBSIGN_KEYID=YOUR_GPG_KEY_ID${NC}"
        read -p "Continue with PPA upload anyway? (y/N): " -r
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            echo "PPA upload skipped."
            exit 0 # Exit successfully if PPA upload is skipped
        fi
    fi
fi

if [ -n "${DEBSIGN_KEYID}" ]; then
    echo -e "${BLUE}⬆️  Uploading to PPA: ppa:apertacodex/ak (series: noble) with key ${DEBSIGN_KEYID}${NC}"
    ../ppa-upload.sh -s noble -k "${DEBSIGN_KEYID}"
else
    echo -e "${RED}❌ PPA upload skipped due to missing GPG key ID.${NC}"
fi

echo ""
echo -e "${GREEN}🎉 Release completed successfully!${NC}"
echo ""
echo -e "${BLUE}📦 Users can now install with:${NC}"
echo "  curl -fsSL https://apertacodex.github.io/ak/setup-repository.sh | bash"
echo "  sudo apt install ak"
echo ""
echo -e "${BLUE}📍 GitHub Pages Repository: https://apertacodex.github.io/ak/ak-apt-repo${NC}"
echo -e "${BLUE}📍 Launchpad PPA: https://launchpad.net/~apertacodex/+archive/ubuntu/ak${NC}"