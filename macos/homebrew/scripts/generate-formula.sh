#!/bin/bash

# AK Homebrew Formula Generator
# This script generates a complete Homebrew formula from template with actual values

set -euo pipefail

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOMEBREW_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$(dirname "$HOMEBREW_DIR")")"

# Default values
VERSION=""
RELEASE_URL=""
SHA256=""
GITHUB_OWNER="apertacodex"
GITHUB_REPO="ak"
LICENSE="MIT"
OUTPUT_DIR="$HOMEBREW_DIR/generated"

# Usage information
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Generate Homebrew formula from template with actual values.

OPTIONS:
    -v, --version VERSION      Set version (e.g., 3.3.0)
    -u, --url URL             Set release URL
    -s, --sha256 CHECKSUM     Set SHA256 checksum
    -o, --owner OWNER         Set GitHub owner (default: ak-team)
    -r, --repo REPO           Set GitHub repository (default: ak)
    -l, --license LICENSE     Set license (default: MIT)
    --output DIR              Set output directory (default: generated/)
    --bottles FILE            Path to bottles JSON file
    -h, --help               Show this help message

EXAMPLES:
    # Generate formula with version and auto-calculate checksum
    $0 --version 3.3.0 --url https://github.com/ak-team/ak/archive/v3.3.0.tar.gz

    # Generate with all values specified
    $0 --version 3.3.0 --url https://example.com/ak-3.3.0.tar.gz --sha256 abc123...

    # Generate for custom repository
    $0 --version 3.3.0 --owner myorg --repo ak-fork
EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--version)
            VERSION="$2"
            shift 2
            ;;
        -u|--url)
            RELEASE_URL="$2"
            shift 2
            ;;
        -s|--sha256)
            SHA256="$2"
            shift 2
            ;;
        -o|--owner)
            GITHUB_OWNER="$2"
            shift 2
            ;;
        -r|--repo)
            GITHUB_REPO="$2"
            shift 2
            ;;
        -l|--license)
            LICENSE="$2"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --bottles)
            BOTTLES_FILE="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Error: Unknown option $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

# Validate required parameters
if [[ -z "$VERSION" ]]; then
    echo "Error: Version is required. Use --version to specify." >&2
    exit 1
fi

# Auto-generate release URL if not provided
if [[ -z "$RELEASE_URL" ]]; then
    RELEASE_URL="https://github.com/$GITHUB_OWNER/$GITHUB_REPO/archive/v$VERSION.tar.gz"
    echo "Info: Using auto-generated release URL: $RELEASE_URL"
fi

# Calculate SHA256 if not provided
if [[ -z "$SHA256" ]]; then
    echo "Info: Calculating SHA256 checksum for $RELEASE_URL"
    TEMP_FILE=$(mktemp)
    if curl -fsSL "$RELEASE_URL" -o "$TEMP_FILE"; then
        SHA256=$(shasum -a 256 "$TEMP_FILE" | cut -d' ' -f1)
        rm "$TEMP_FILE"
        echo "Info: Calculated SHA256: $SHA256"
    else
        echo "Error: Failed to download release archive for checksum calculation" >&2
        rm "$TEMP_FILE" 2>/dev/null || true
        exit 1
    fi
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Template file paths
FORMULA_TEMPLATE="$HOMEBREW_DIR/formula/ak.rb"
OUTPUT_FORMULA="$OUTPUT_DIR/ak.rb"

# Check if template exists
if [[ ! -f "$FORMULA_TEMPLATE" ]]; then
    echo "Error: Formula template not found: $FORMULA_TEMPLATE" >&2
    exit 1
fi

echo "Generating Homebrew formula..."
echo "  Version: $VERSION"
echo "  Release URL: $RELEASE_URL"
echo "  SHA256: $SHA256"
echo "  GitHub: $GITHUB_OWNER/$GITHUB_REPO"
echo "  License: $LICENSE"
echo ""

# Generate archive name from URL  
ARCHIVE_NAME=$(basename "$RELEASE_URL")

# Copy template and substitute placeholders
cp "$FORMULA_TEMPLATE" "$OUTPUT_FORMULA"

# Replace placeholders in formula
sed -i.bak \
    -e "s|{{VERSION}}|$VERSION|g" \
    -e "s|{{RELEASE_URL}}|$RELEASE_URL|g" \
    -e "s|{{SHA256}}|$SHA256|g" \
    -e "s|{{GITHUB_OWNER}}|$GITHUB_OWNER|g" \
    -e "s|{{GITHUB_REPO}}|$GITHUB_REPO|g" \
    -e "s|{{LICENSE}}|$LICENSE|g" \
    -e "s|{{ARCHIVE_NAME}}|$ARCHIVE_NAME|g" \
    "$OUTPUT_FORMULA"

# Handle bottle placeholders - either replace with actual values or remove bottle block
if [[ -n "${BOTTLES_FILE:-}" ]] && [[ -f "$BOTTLES_FILE" ]]; then
    echo "Info: Processing bottles from $BOTTLES_FILE"
    # TODO: Parse bottles JSON and replace placeholders
    # For now, we'll use placeholder values or remove the bottle block
    sed -i.bak2 '/bottle do/,/end/d' "$OUTPUT_FORMULA"
else
    # Remove bottle block since we don't have bottle info
    echo "Info: Removing bottle block (no bottles file provided)"
    sed -i.bak2 '/bottle do/,/end/d' "$OUTPUT_FORMULA"
fi

# Clean up backup files
rm -f "$OUTPUT_FORMULA.bak" "$OUTPUT_FORMULA.bak2"

echo "Formula generated successfully: $OUTPUT_FORMULA"

# Validate the generated formula if brew is available
if command -v brew >/dev/null 2>&1; then
    echo ""
    echo "Validating generated formula..."
    if brew audit --strict "$OUTPUT_FORMULA" 2>/dev/null; then
        echo "✓ Formula validation passed"
    else
        echo "⚠ Formula validation failed - manual review required"
    fi
    
    if brew style "$OUTPUT_FORMULA" 2>/dev/null; then
        echo "✓ Formula style check passed"
    else
        echo "⚠ Formula style check failed - manual review required"
    fi
else
    echo "Info: Homebrew not available - skipping validation"
fi

echo ""
echo "Generated formula ready for use!"
echo "Next steps:"
echo "  1. Review the generated formula: $OUTPUT_FORMULA"
echo "  2. Test locally: brew install --build-from-source $OUTPUT_FORMULA"
echo "  3. Publish to tap: ./scripts/publish-to-tap.sh --formula $OUTPUT_FORMULA"