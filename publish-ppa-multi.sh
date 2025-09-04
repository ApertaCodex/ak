#!/usr/bin/env bash
set -euo pipefail

# Multi-distribution publisher for Launchpad PPA
# Uploads the same version to multiple Ubuntu distributions
# Usage: ./publish-ppa-multi.sh [--dry-run]

PPA="${PPA:-ppa:apertacodex/ak}"
KEYID="${DEBSIGN_KEYID:-$(gpg --list-secret-keys --with-colons 2>/dev/null | awk -F: '$1=="fpr"{print $10; exit}' || echo "")}"
DPUT_FLAGS="${DPUT_FLAGS:-}"

# Major Ubuntu distributions to support
DISTRIBUTIONS=("noble" "jammy" "focal" "oracular" "plucky")

# Handle dry-run flag
if [[ "${1:-}" == "--dry-run" ]]; then
    DPUT_FLAGS="--simulate ${DPUT_FLAGS}"
    echo "🧪 DRY RUN MODE - No actual uploads will be performed"
    echo ""
fi

# Ensure tools exist
need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing command: $1" >&2; exit 1; }; }
need debuild
need dput
need dpkg-parsechangelog
need gpg
need git

# Get current version from changelog
CURRENT_VERSION="$(dpkg-parsechangelog -S Version)"
BASE_VERSION="${CURRENT_VERSION%%~*}"  # Remove any ~distro suffix
BASE_VERSION="${BASE_VERSION%%-*}"     # Remove any -ubuntu suffix

if [[ -z "${KEYID}" ]]; then
    echo "❌ No GPG secret key found. Set DEBSIGN_KEYID or create/import a key."
    exit 1
fi

echo "🚀 Multi-distribution PPA publisher for AK"
echo "📦 Base version: ${BASE_VERSION}"
echo "🎯 PPA: ${PPA}"
echo "🔑 Signing key: ${KEYID}"
echo "📋 Distributions: ${DISTRIBUTIONS[*]}"
echo ""

# Backup original changelog
cp debian/changelog debian/changelog.backup

# Track successful uploads
SUCCESSFUL_UPLOADS=()
FAILED_UPLOADS=()

# Upload to each distribution
for DIST in "${DISTRIBUTIONS[@]}"; do
    echo "📦 Processing ${DIST}..."
    
    # Create distribution-specific version
    DIST_VERSION="${BASE_VERSION}-1ubuntu1~${DIST}1"
    
    # Create temporary changelog for this distribution
    cat > debian/changelog.tmp << EOF
ak (${DIST_VERSION}) ${DIST}; urgency=medium

  * Multi-distribution build for Ubuntu ${DIST}
  * Updated copyright to ApertaCodex with open source MIT license
  * Restored beautiful homepage design with TailwindCSS
  * Added full automation for APT repository publishing
  * Enhanced release process with automated version management
  * Fixed repository metadata and package paths
  * Attribution required for derivative works

 -- ApertaCodex <developers@apertacodex.ai>  $(date -R)

EOF
    
    # Append rest of original changelog (skip first entry)
    tail -n +$(grep -n "^ak (" debian/changelog.backup | head -2 | tail -1 | cut -d: -f1) debian/changelog.backup >> debian/changelog.tmp 2>/dev/null || true
    
    # Replace changelog temporarily
    mv debian/changelog debian/changelog.original
    mv debian/changelog.tmp debian/changelog
    
    echo "  📋 Building source package for ${DIST} (${DIST_VERSION})..."
    
    # Clean previous builds
    rm -f ../ak_${DIST_VERSION}* || true
    
    # Build source package
    if debuild -S -sa -k"${KEYID}" > /dev/null 2>&1; then
        echo "  ✅ Source package built successfully"
        
        # Upload to PPA
        echo "  ⬆️  Uploading to PPA..."
        if dput ${DPUT_FLAGS} "${PPA}" "../ak_${DIST_VERSION}_source.changes" > /dev/null 2>&1; then
            echo "  ✅ Upload successful for ${DIST}"
            SUCCESSFUL_UPLOADS+=("${DIST}")
        else
            echo "  ❌ Upload failed for ${DIST}"
            FAILED_UPLOADS+=("${DIST}")
        fi
    else
        echo "  ❌ Source package build failed for ${DIST}"
        FAILED_UPLOADS+=("${DIST}")
    fi
    
    # Restore original changelog
    mv debian/changelog.original debian/changelog
    
    echo ""
done

# Restore backup
mv debian/changelog.backup debian/changelog

# Summary
echo "📊 Upload Summary:"
if [[ ${#SUCCESSFUL_UPLOADS[@]} -gt 0 ]]; then
    echo "✅ Successful uploads: ${SUCCESSFUL_UPLOADS[*]}"
fi

if [[ ${#FAILED_UPLOADS[@]} -gt 0 ]]; then
    echo "❌ Failed uploads: ${FAILED_UPLOADS[*]}"
fi

echo ""
echo "🔗 Monitor builds at: https://launchpad.net/~apertacodex/+archive/ubuntu/ak"

if [[ ${#FAILED_UPLOADS[@]} -gt 0 ]]; then
    exit 1
fi

echo "🎉 Multi-distribution upload completed successfully!"