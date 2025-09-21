#!/usr/bin/env bash
# Fix AK apt repository NO_PUBKEY error by installing the correct keyring
# and writing a proper sources.list entry that matches the signed-by path.
#
# Usage:
#   bash scripts/fix-ak-apt-repo.sh [plucky|stable]
# Default series: stable

set -euo pipefail

SERIES="${1:-stable}"
case "$SERIES" in
  plucky|stable) ;;
  *)
    echo "Invalid series: $SERIES. Use 'plucky' or 'stable'." >&2
    exit 1
    ;;
esac

if [ "$SERIES" = "plucky" ]; then
  echo "WARNING: 'plucky' distribution metadata may be outdated. Prefer 'stable' for current packages." >&2
fi

KEYRING="/usr/share/keyrings/ak-archive-keyring.gpg"
SOURCE_FILE="/etc/apt/sources.list.d/ak-${SERIES}.list"
REPO_URL="https://apertacodex.github.io/ak/ak-apt-repo/"

echo "==> Ensuring keyrings directory exists"
sudo install -d -m 0755 /usr/share/keyrings

echo "==> Installing repository keyring to: ${KEYRING}"
# Prefer local key if available; otherwise, download
if [ -f "$(pwd)/ak-repository-key.gpg" ]; then
  echo "    Using local ./ak-repository-key.gpg"
  sudo install -m 0644 "$(pwd)/ak-repository-key.gpg" "$KEYRING"
else
  echo "    Downloading key from ${REPO_URL%/}/../ak-repository-key.gpg"
  curl -fsSL https://apertacodex.github.io/ak/ak-repository-key.gpg | sudo tee "$KEYRING" >/dev/null
fi

# Optional: show key information for verification (non-fatal if gpg not present)
if command -v gpg >/dev/null 2>&1; then
  echo "==> Key details:"
  gpg --show-keys "$KEYRING" || true
fi

echo "==> Writing apt source to: ${SOURCE_FILE}"
# Use exact signed-by path that matches KEYRING
echo "deb [arch=amd64 signed-by=${KEYRING}] ${REPO_URL} ${SERIES} main" | sudo tee "${SOURCE_FILE}" >/dev/null

echo "==> Backing up any conflicting entries"
for f in \
  /etc/apt/sources.list.d/ak-plucky.list \
  /etc/apt/sources.list.d/ak.list \
  /etc/apt/sources.list.d/apertacodex-ubuntu-ak-plucky.sources
do
  if [ -f "$f" ] && [ "$f" != "$SOURCE_FILE" ]; then
    sudo mv "$f" "$f.bak.$(date +%s)" || true
    echo "    Backed up: $f"
  fi
done

echo "==> Running apt update"
sudo apt update

echo "==> Done."
echo "You can now install AK with:"
echo "  sudo apt install ak"
