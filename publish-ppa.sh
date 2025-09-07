#!/usr/bin/env bash
set -euo pipefail

# One-command publisher for Launchpad PPA
# Usage:
#   ./publish-ppa.sh            # auto-detects series from debian/changelog and signing key
#   DEBSIGN_KEYID=<fingerprint> ./publish-ppa.sh
#   SERIES=noble ./publish-ppa.sh
#
# Optional:
#   DPUT_FLAGS="--simulate" ./publish-ppa.sh   # dry run

PPA="${PPA:-ppa:apertacodex/ak}"

# Determine Ubuntu series (prefer changelog Distribution; fallback noble)
SERIES_DEFAULT="$(dpkg-parsechangelog -S Distribution 2>/dev/null || true)"
SERIES="${SERIES:-${SERIES_DEFAULT:-noble}}"

# Ensure tools exist
need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing command: $1" >&2; exit 1; }; }
need debuild
need dput
need dpkg-parsechangelog
need gpg
need git
need xz

# Determine signing key (prefer full fingerprint)
KEYID="${DEBSIGN_KEYID:-${KEYID:-}}"
if [[ -z "${KEYID}" ]]; then
  KEYID="$(gpg --list-secret-keys --with-colons 2>/dev/null | awk -F: '$1=="fpr"{print $10; exit}')"
fi
if [[ -z "${KEYID}" ]]; then
  echo "No GPG secret key found. Set DEBSIGN_KEYID or create/import a key (gpg --full-generate-key)." >&2
  exit 2
fi

# Print summary
SRC="$(dpkg-parsechangelog -S Source || echo ak)"
VER="$(dpkg-parsechangelog -S Version || echo unknown)"
DIST="$(dpkg-parsechangelog -S Distribution || echo unknown)"
echo "== AK PPA Publisher =="
echo "Package: ${SRC}"
echo "Version: ${VER}"
echo "Changelog Distribution: ${DIST}"
echo "Target Series: ${SERIES}"
echo "PPA: ${PPA}"
echo "Signing key (fingerprint): ${KEYID}"
echo ""

# Ensure changelog series matches target; warn if differs (ppa-upload will align)
if [[ "${DIST}" != "${SERIES}" ]]; then
  echo "Warning: debian/changelog Distribution='${DIST}' differs from target series='${SERIES}'."
fi

# If source format is quilt, prepare orig tarball (native does not need it)
FORMAT="$(tr -d ' 	\r' < debian/source/format 2>/dev/null || echo '')"
if [[ "${FORMAT}" == "3.0(quilt)" ]]; then
  # Create build directory for PPA artifacts
  BUILD_DIR="build/ppa"
  mkdir -p "${BUILD_DIR}"
  
  UPSTREAM="${VER%%-*}"
  ORIG="${BUILD_DIR}/${SRC}_${UPSTREAM}.orig.tar.xz"
  if [[ ! -f "${ORIG}" ]]; then
    echo "Creating orig tarball: ${ORIG}"
    git archive --format=tar --prefix="${SRC}-${UPSTREAM}/" HEAD | xz -9e > "${ORIG}"
  else
    echo "Orig tarball already exists: ${ORIG}"
  fi
fi

# Delegate build+upload to the helper (handles signing and upload)
echo "Delegating to ppa-upload.sh..."
if ! ./ppa-upload.sh -s "${SERIES}" -k "${KEYID}"; then
  echo ""
  echo "PPA upload failed. Please check the error messages above."
  echo "Common troubleshooting steps:"
  echo "  1. Verify PPA exists: https://launchpad.net/~apertacodex"
  echo "  2. Check GPG key is uploaded to Launchpad"
  echo "  3. Ensure you have upload permissions to the PPA"
  echo "  4. Verify package version is unique"
  echo ""
  exit 1
fi