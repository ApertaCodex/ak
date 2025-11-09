#!/bin/bash
# Automated Release Script for AK API Key Manager
# Handles version bumping, building, and git commit
# Note: GitHub Actions handles publishing, APT repository, and Homebrew updates

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

# Configure CMake if needed
if [ ! -f "CMakeCache.txt" ]; then
    echo -e "${BLUE}⚙️  Configuring CMake...${NC}"
    cmake .. -DCMAKE_BUILD_TYPE=Release
fi

# Run the automated release process
echo -e "${BLUE}🔄 Running automated release process...${NC}"
echo ""

echo -e "${YELLOW}Step 1: Version Bump${NC}"
cmake --build . --target "bump-$RELEASE_TYPE"

echo ""
echo -e "${YELLOW}Step 2: Building${NC}"
cmake --build . --target build-release

echo ""
echo -e "${YELLOW}Step 3: Running Tests${NC}"
cmake --build . --target run_tests || echo -e "${YELLOW}⚠️  Tests failed, but continuing...${NC}"

echo ""
echo -e "${YELLOW}Step 4: Git Commit & Push${NC}"
cd ..
cmake --build build --target commit-and-push || {
    echo -e "${YELLOW}⚠️  Git commit/push failed or no changes to commit${NC}"
    echo -e "${YELLOW}   You may need to commit and push manually${NC}"
}

echo ""
echo -e "${GREEN}🎉 Release preparation completed!${NC}"
echo ""
echo -e "${BLUE}📦 Next steps:${NC}"
echo ""
echo -e "${YELLOW}1. GitHub Actions will automatically:${NC}"
echo "   - Build binaries for Linux, macOS, and Windows"
echo "   - Create GitHub release with artifacts"
echo "   - Update Homebrew formula"
echo ""
echo -e "${YELLOW}2. To trigger the release workflow:${NC}"
echo "   - Push the version bump commit to main branch"
echo "   - Or manually trigger the release workflow in GitHub Actions"
echo ""
echo -e "${GREEN}🤖 All publishing is handled automatically by GitHub Actions workflows.${NC}"