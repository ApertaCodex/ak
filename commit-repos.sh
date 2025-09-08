#!/bin/bash
# Script to specifically commit repository directories to GitHub
# This is separate from the main release process to avoid conflicts with Debian packaging

set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}📦 Committing repository directories to GitHub...${NC}"

# Stage the repository directories
git add ak-apt-repo/ ak-macos-repo/

# Check if there are changes to commit
if git diff --cached --quiet ak-apt-repo/ ak-macos-repo/; then
    echo -e "${BLUE}No changes to repository directories.${NC}"
    exit 0
fi

# Commit the changes
git commit -m "📦 Update repository packages and metadata" ak-apt-repo/ ak-macos-repo/

# Push to GitHub if origin exists
if git remote get-url origin >/dev/null 2>&1; then
    echo -e "${BLUE}Pushing repository changes to GitHub...${NC}"
    git push origin $(git branch --show-current)
    echo -e "${GREEN}✅ Successfully pushed repository updates to GitHub${NC}"
else
    echo -e "${BLUE}No remote 'origin' found. Skipping push to GitHub.${NC}"
    echo -e "${GREEN}✅ Repository changes committed locally${NC}"
fi