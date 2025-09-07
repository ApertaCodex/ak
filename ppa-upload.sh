#!/usr/bin/env bash
set -euo pipefail

# AK PPA upload helper: builds signed source package and uploads to Launchpad
# Usage: bash ppa-upload.sh [-p|--ppa ppa:apertacodex/ak] [-s|--series noble] [-k|--key KEYID] [--simulate]

PPA="${PPA:-ppa:apertacodex/ak}"
# Default series to changelog Distribution if available, else noble
SERIES_DEFAULT="$(dpkg-parsechangelog -S Distribution 2>/dev/null || true)"
SERIES="${SERIES:-${SERIES_DEFAULT:-noble}}"
KEYID="${DEBSIGN_KEYID:-${KEYID:-}}"
DPUT_FLAGS="${DPUT_FLAGS:-}"

usage() {
  cat <<EOF
Usage: $0 [options]
  -p, --ppa PPA           Launchpad PPA target (default: $PPA)
  -s, --series SERIES     Ubuntu series/codename for changelog (default: $SERIES)
  -k, --key KEYID         GPG key ID to sign with (defaults to \$DEBSIGN_KEYID or first secret key)
      --simulate          Use dput --simulate (dry run)
  -h, --help              Show this help

Environment:
  PPA, SERIES, DEBSIGN_KEYID, DPUT_FLAGS

Prereqs: devscripts (debuild, dput), dpkg-dev, debhelper, gnupg
EOF
}

# Parse args
while [[ $# -gt 0 ]]; do
  case "$1" in
    -p|--ppa) PPA="$2"; shift 2;;
    -s|--series) SERIES="$2"; shift 2;;
    -k|--key) KEYID="$2"; shift 2;;
    --simulate) DPUT_FLAGS="--simulate ${DPUT_FLAGS}"; shift;;
    -h|--help) usage; exit 0;;
    *) echo "Unknown option: $1"; usage; exit 1;;
  esac
done

require_cmd() { command -v "$1" >/dev/null 2>&1 || { echo "Missing command: $1" >&2; exit 1; }; }

echo "Checking prerequisites..."
require_cmd debuild
require_cmd dput
require_cmd dpkg-parsechangelog
require_cmd gpg
require_cmd git
require_cmd xz

# Determine signing key (prefer full fingerprint)
if [[ -z "${KEYID:-}" ]]; then
  KEYID="$(gpg --list-secret-keys --with-colons 2>/dev/null | awk -F: '$1=="fpr"{print $10; exit}')"
fi
if [[ -z "${KEYID}" ]]; then
  echo "No GPG secret key found. Set DEBSIGN_KEYID or create/import a key."
  echo "Example: gpg --full-generate-key"
  exit 1
fi

echo "Using PPA: ${PPA}"
echo "Target series: ${SERIES}"
echo "Signing key: ${KEYID}"

# Verify and align changelog distribution
DIST="$(dpkg-parsechangelog -S Distribution || true)"
if [[ -z "${DIST}" ]]; then
  echo "Could not read debian/changelog distribution."
  exit 1
fi
echo "Changelog distribution: ${DIST}"
if [[ -z "${SERIES}" ]]; then
  SERIES="${DIST}"
elif [[ "${DIST}" != "${SERIES}" ]]; then
  echo "Warning: series '${SERIES}' differs from changelog distribution '${DIST}'. Using '${DIST}'."
  SERIES="${DIST}"
fi

# Compute version
VERSION="$(dpkg-parsechangelog -S Version)"
PKG_NAME="$(dpkg-parsechangelog -S Source || echo ak)"

# Handle orig tarball for quilt format
FORMAT="$(tr -d ' 	\r' < debian/source/format 2>/dev/null || echo '')"
if [[ "${FORMAT}" == "3.0(quilt)" ]]; then
  UPSTREAM="${VERSION%%-*}"
  ORIG="../${PKG_NAME}_${UPSTREAM}.orig.tar.xz"
  if [[ ! -f "${ORIG}" ]]; then
    echo "Creating orig tarball: ${ORIG}"
    git archive --format=tar --prefix="${PKG_NAME}-${UPSTREAM}/" HEAD | xz -9e > "${ORIG}"
  else
    echo "Orig tarball already exists: ${ORIG}"
  fi
fi

echo "Building signed source package for ${PKG_NAME} ${VERSION}..."
# Clean previous source changes
rm -f ../${PKG_NAME}_*_source.changes || true

# Temporarily backup and remove ignored directories to avoid dpkg-source errors
TEMP_BACKUP_DIR="/tmp/ak_ppa_backup_$$"
mkdir -p "${TEMP_BACKUP_DIR}"
BACKED_UP_DIRS=()

if [ -d ".vscode" ]; then
    cp -r .vscode "${TEMP_BACKUP_DIR}/"
    rm -rf .vscode
    BACKED_UP_DIRS+=(".vscode")
fi
if [ -d "tests/googletest" ]; then
    cp -r tests/googletest "${TEMP_BACKUP_DIR}/"
    rm -rf tests/googletest
    BACKED_UP_DIRS+=("tests/googletest")
fi
if [ -d "ak-apt-repo" ]; then
    cp -r ak-apt-repo "${TEMP_BACKUP_DIR}/"
    rm -rf ak-apt-repo
    BACKED_UP_DIRS+=("ak-apt-repo")
fi

# Stash any uncommitted changes to create clean source package
STASH_CREATED=false
if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "Stashing uncommitted changes for clean source build..."
    git stash push -u -m "Temporary stash for PPA build"
    STASH_CREATED=true
fi

# Build source package (-S) with full orig upload (-sa)
BUILD_SUCCESS=false
if debuild --version >/dev/null 2>&1; then
  if debuild -S -sa -k"${KEYID}"; then
    BUILD_SUCCESS=true
  fi
else
  if dpkg-buildpackage -S -sa -k"${KEYID}"; then
    BUILD_SUCCESS=true
  fi
fi

# Restore stashed changes if any
if [ "$STASH_CREATED" = true ]; then
    echo "Restoring stashed changes..."
    git stash pop
fi

# Restore backed up directories
for dir in "${BACKED_UP_DIRS[@]}"; do
  if [ -d "${TEMP_BACKUP_DIR}/${dir}" ]; then
    cp -r "${TEMP_BACKUP_DIR}/${dir}" .
  fi
done

# Clean up temp backup
rm -rf "${TEMP_BACKUP_DIR}"

if [ "$BUILD_SUCCESS" = false ]; then
  echo "debuild failed."
  exit 1
fi

# Find the .changes file
CHANGES_FILE="$(ls -t ../${PKG_NAME}_*_source.changes 2>/dev/null | head -n1 || true)"
if [[ -z "${CHANGES_FILE}" ]]; then
  echo "Could not locate source .changes file."
  exit 1
fi

echo "Uploading to ${PPA}: ${CHANGES_FILE}"

# Check if PPA is accessible before attempting upload
if [[ -z "${DPUT_FLAGS}" ]] || [[ "${DPUT_FLAGS}" != *"--simulate"* ]]; then
  echo "Checking PPA accessibility..."
  PPA_URL="${PPA#ppa:}"
  PPA_URL="https://launchpad.net/~${PPA_URL/\/*/}/+archive/ubuntu/${PPA_URL/*\/}"
  
  if ! curl -sf "${PPA_URL}" >/dev/null 2>&1; then
    echo "Warning: Unable to verify PPA accessibility at ${PPA_URL}"
    echo "This may indicate:"
    echo "  - PPA doesn't exist or is not public"
    echo "  - Network connectivity issues"
    echo "  - Authentication problems"
    echo ""
    read -p "Continue with upload anyway? (y/N): " -r
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
      echo "Upload cancelled."
      exit 1
    fi
  fi
fi

# Attempt upload with better error handling
if ! dput ${DPUT_FLAGS} "${PPA}" "${CHANGES_FILE}"; then
  echo ""
  echo "Upload failed. Common causes and solutions:"
  echo "  1. PPA doesn't exist: Create PPA at https://launchpad.net/~apertacodex"
  echo "  2. Permission denied: Ensure you're a member of the PPA team"
  echo "  3. GPG key not uploaded: Upload your key at https://launchpad.net/~apertacodex"
  echo "  4. Network issues: Check connectivity and retry"
  echo "  5. Package already exists: Bump version in debian/changelog"
  echo ""
  echo "For detailed dput configuration, check ~/.dput.cf or /etc/dput.cf"
  exit 1
fi

echo "Upload submitted successfully."
echo "Monitor build at:"
echo "  https://launchpad.net/~apertacodex/+archive/ubuntu/ak"
echo ""
echo "If this was a dry run (--simulate), re-run without it to perform the upload."