#!/usr/bin/env bash
# Regenerate APT repository metadata for a distribution (e.g., plucky or stable)
# - Rebuilds Packages and Packages.gz from pool/main
# - Generates Release using apt-ftparchive (preferred) or a minimal fallback
# - Signs Release to produce InRelease and Release.gpg using your default GPG key
#
# Usage:
#   bash scripts/regenerate-apt-dist.sh [distribution] [gpg_keyid (optional)]
# Examples:
#   bash scripts/regenerate-apt-dist.sh plucky
#   bash scripts/regenerate-apt-dist.sh stable "A49D42072C7F5498"
#
# Requirements (on maintainer machine):
#   - dpkg-scanpackages (from dpkg-dev)
#   - apt-ftparchive (from apt-utils or apt)
#   - gpg with your signing key available (the key that clients trust)

set -euo pipefail

DIST="${1:-plucky}"
GPG_KEY="${2:-}"

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/ak-apt-repo"
POOL_DIR="${REPO_DIR}/pool/main"
DIST_DIR="${REPO_DIR}/dists/${DIST}"
PKG_DIR="${DIST_DIR}/main/binary-amd64"

echo "==> Regenerating APT metadata for distribution: ${DIST}"
echo "    Repo: ${REPO_DIR}"

# Sanity checks
if ! command -v dpkg-scanpackages >/dev/null 2>&1; then
  echo "ERROR: dpkg-scanpackages not found. Install dpkg-dev." >&2
  exit 1
fi

APT_FTPARCHIVE_AVAILABLE=true
if ! command -v apt-ftparchive >/dev/null 2>&1; then
  echo "WARN: apt-ftparchive not found. Will use minimal Release fallback." >&2
  APT_FTPARCHIVE_AVAILABLE=false
fi

# Prepare directories
mkdir -p "${PKG_DIR}"

# 1) Rebuild Packages and Packages.gz
echo "==> Building Packages from ${POOL_DIR}"
pushd "${PKG_DIR}" >/dev/null
# Generate and fix relative paths
dpkg-scanpackages --multiversion ../../../../pool/main /dev/null \
  | sed 's|^Filename: ../../../../|Filename: |' > Packages
gzip -fk Packages
popd >/dev/null
echo "    Packages and Packages.gz written to ${PKG_DIR}"

# 2) Generate Release
echo "==> Generating Release file for ${DIST}"
if [ "${APT_FTPARCHIVE_AVAILABLE}" = true ]; then
  # Use a temporary apt-ftparchive config to set headers
  TMP_CONF="$(mktemp)"
  cat > "${TMP_CONF}" <<EOF
APT::FTPArchive::Release {
  Origin "ApertaCodex";
  Label "AK";
  Suite "${DIST}";
  Codename "${DIST}";
  Architectures "amd64";
  Components "main";
  Description "AK APT repository (${DIST})";
};
Dir {
  ArchiveDir "${REPO_DIR}";
};
Default {
  Packages::Compress ". gzip";
};
EOF
  # Generate Release (apt-ftparchive calculates checksums)
  apt-ftparchive -c "${TMP_CONF}" release "dists/${DIST}" > "${DIST_DIR}/Release"
  rm -f "${TMP_CONF}"
else
  # Minimal fallback: compute checksums for Packages files manually
  PKG_REL="main/binary-amd64/Packages"
  PKG_GZ_REL="main/binary-amd64/Packages.gz"
  PKG_PATH="${PKG_DIR}/Packages"
  PKG_GZ_PATH="${PKG_DIR}/Packages.gz"

  PKG_SIZE=$(stat -c%s "${PKG_PATH}")
  PKG_GZ_SIZE=$(stat -c%s "${PKG_GZ_PATH}")
  PKG_MD5=$(md5sum "${PKG_PATH}" | awk '{print $1}')
  PKG_GZ_MD5=$(md5sum "${PKG_GZ_PATH}" | awk '{print $1}')
  PKG_SHA1=$(sha1sum "${PKG_PATH}" | awk '{print $1}')
  PKG_GZ_SHA1=$(sha1sum "${PKG_GZ_PATH}" | awk '{print $1}')
  PKG_SHA256=$(sha256sum "${PKG_PATH}" | awk '{print $1}')
  PKG_GZ_SHA256=$(sha256sum "${PKG_GZ_PATH}" | awk '{print $1}')

  DATE_STR="$(date -u '+%a, %d %b %Y %H:%M:%S +0000')"
  cat > "${DIST_DIR}/Release" <<EOF
Archive: ${DIST}
Codename: ${DIST}
Components: main
Date: ${DATE_STR}
Description: AK API Key Manager Repository
Label: AK Repository
Origin: AK
Suite: ${DIST}
Version: 1.0
Architectures: amd64
MD5Sum:
 ${PKG_MD5} ${PKG_SIZE} ${PKG_REL}
 ${PKG_GZ_MD5} ${PKG_GZ_SIZE} ${PKG_GZ_REL}
SHA1:
 ${PKG_SHA1} ${PKG_SIZE} ${PKG_REL}
 ${PKG_GZ_SHA1} ${PKG_GZ_SIZE} ${PKG_GZ_REL}
SHA256:
 ${PKG_SHA256} ${PKG_SIZE} ${PKG_REL}
 ${PKG_GZ_SHA256} ${PKG_GZ_SIZE} ${PKG_GZ_REL}
EOF
fi
echo "    Release written to ${DIST_DIR}/Release"

# 3) Sign Release to produce InRelease and Release.gpg
echo "==> Signing Release"
export GPG_TTY=${GPG_TTY:-$(tty || true)}
SIGN_OPTS=(--yes)
if [ -n "${GPG_KEY}" ]; then
  SIGN_OPTS+=(--local-user "${GPG_KEY}")
fi

set +e
gpg "${SIGN_OPTS[@]}" --clearsign -o "${DIST_DIR}/InRelease" "${DIST_DIR}/Release"
SIGN1_STATUS=$?
gpg "${SIGN_OPTS[@]}" --armor --detach-sig -o "${DIST_DIR}/Release.gpg" "${DIST_DIR}/Release"
SIGN2_STATUS=$?
set -e

if [ $SIGN1_STATUS -ne 0 ] || [ $SIGN2_STATUS -ne 0 ]; then
  echo "ERROR: Signing failed. Ensure your GPG signing key is available and trusted." >&2
  echo "       The repository will be considered 'not signed' by apt without InRelease/Release.gpg." >&2
  exit 1
fi

echo "==> Done."
echo "Updated files:"
echo " - ${PKG_DIR}/Packages"
echo " - ${PKG_DIR}/Packages.gz"
echo " - ${DIST_DIR}/Release"
echo " - ${DIST_DIR}/InRelease"
echo " - ${DIST_DIR}/Release.gpg"

echo ""
echo "Next steps:"
echo " - Commit and push the changes backing your GitHub Pages site"
echo " - On clients: sudo apt update && apt policy ak"
