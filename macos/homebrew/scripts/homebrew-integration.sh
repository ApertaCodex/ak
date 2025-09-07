#!/bin/bash

# AK Homebrew Integration Script
# This script provides integration hooks for the main AK repository to trigger Homebrew updates

set -euo pipefail

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOMEBREW_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$(dirname "$HOMEBREW_DIR")")"

# Configuration
GITHUB_API_URL="https://api.github.com"
TAP_OWNER="apertacodex"
TAP_REPO="homebrew-ak"
MAIN_REPO_OWNER="apertacodex"
MAIN_REPO="ak"

# Usage information
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Integration script for triggering Homebrew formula updates from the main AK repository.

OPTIONS:
    --trigger-update         Trigger Homebrew formula update
    --version VERSION        Specify version for update
    --github-token TOKEN     GitHub token for API access
    --release-url URL        Release download URL
    --sha256 CHECKSUM        Archive SHA256 checksum
    --dry-run                Show what would be done without making changes
    -h, --help              Show this help message

EXAMPLES:
    # Trigger update from CI/CD pipeline
    $0 --trigger-update --version 3.3.0 --github-token \$GITHUB_TOKEN

    # Manual update with specific parameters
    $0 --trigger-update --version 3.3.0 --release-url https://... --sha256 abc123...

ENVIRONMENT VARIABLES:
    GITHUB_TOKEN             GitHub personal access token
    AK_VERSION              Version to release
    RELEASE_URL             Download URL for release
    SHA256_CHECKSUM         Archive checksum
EOF
}

# Default values
ACTION=""
VERSION=""
GITHUB_TOKEN="${GITHUB_TOKEN:-}"
RELEASE_URL=""
SHA256_CHECKSUM=""
DRY_RUN="false"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --trigger-update)
            ACTION="trigger-update"
            shift
            ;;
        --version)
            VERSION="$2"
            shift 2
            ;;
        --github-token)
            GITHUB_TOKEN="$2"
            shift 2
            ;;
        --release-url)
            RELEASE_URL="$2"
            shift 2
            ;;
        --sha256)
            SHA256_CHECKSUM="$2"
            shift 2
            ;;
        --dry-run)
            DRY_RUN="true"
            shift
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

# Function to trigger repository dispatch
trigger_homebrew_update() {
    local version="$1"
    local release_url="$2"
    local sha256="$3"
    
    if [[ -z "$GITHUB_TOKEN" ]]; then
        echo "Error: GitHub token is required for API access" >&2
        return 1
    fi
    
    # Construct API payload
    local payload
    payload=$(cat << EOF
{
  "event_type": "ak-release",
  "client_payload": {
    "version": "$version",
    "release_url": "$release_url",
    "sha256": "$sha256"
  }
}
EOF
)
    
    local api_url="$GITHUB_API_URL/repos/$TAP_OWNER/$TAP_REPO/dispatches"
    
    if [[ "$DRY_RUN" == "true" ]]; then
        echo "[DRY RUN] Would send repository dispatch to $api_url"
        echo "[DRY RUN] Payload: $payload"
        return 0
    fi
    
    echo "Triggering Homebrew formula update..."
    echo "  Version: $version"
    echo "  Release URL: $release_url"
    echo "  SHA256: $sha256"
    
    local response
    response=$(curl -s -X POST \
        -H "Accept: application/vnd.github.v3+json" \
        -H "Authorization: token $GITHUB_TOKEN" \
        -d "$payload" \
        "$api_url")
    
    if [[ $? -eq 0 ]]; then
        echo "✓ Successfully triggered Homebrew formula update"
        return 0
    else
        echo "✗ Failed to trigger Homebrew formula update"
        echo "Response: $response"
        return 1
    fi
}

# Function to auto-generate missing parameters
auto_generate_params() {
    # Use environment variables if available
    VERSION="${VERSION:-${AK_VERSION:-}}"
    RELEASE_URL="${RELEASE_URL:-${RELEASE_URL:-}}"
    SHA256_CHECKSUM="${SHA256_CHECKSUM:-${SHA256_CHECKSUM:-}}"
    
    # Auto-generate release URL if not provided
    if [[ -z "$RELEASE_URL" ]] && [[ -n "$VERSION" ]]; then
        RELEASE_URL="https://github.com/$MAIN_REPO_OWNER/$MAIN_REPO/archive/v$VERSION.tar.gz"
        echo "Info: Auto-generated release URL: $RELEASE_URL"
    fi
    
    # Calculate SHA256 if not provided
    if [[ -z "$SHA256_CHECKSUM" ]] && [[ -n "$RELEASE_URL" ]]; then
        echo "Info: Calculating SHA256 checksum for $RELEASE_URL"
        local temp_file
        temp_file=$(mktemp)
        
        if curl -fsSL "$RELEASE_URL" -o "$temp_file"; then
            SHA256_CHECKSUM=$(shasum -a 256 "$temp_file" | cut -d' ' -f1)
            rm "$temp_file"
            echo "Info: Calculated SHA256: $SHA256_CHECKSUM"
        else
            echo "Error: Failed to download release archive for checksum calculation" >&2
            rm "$temp_file" 2>/dev/null || true
            return 1
        fi
    fi
}

# Function to validate parameters
validate_params() {
    if [[ -z "$VERSION" ]]; then
        echo "Error: Version is required" >&2
        return 1
    fi
    
    if [[ -z "$RELEASE_URL" ]]; then
        echo "Error: Release URL is required or could not be auto-generated" >&2
        return 1
    fi
    
    if [[ -z "$SHA256_CHECKSUM" ]]; then
        echo "Error: SHA256 checksum is required or could not be calculated" >&2
        return 1
    fi
    
    if [[ -z "$GITHUB_TOKEN" ]] && [[ "$DRY_RUN" == "false" ]]; then
        echo "Error: GitHub token is required for API access" >&2
        return 1
    fi
    
    return 0
}

# Main execution
main() {
    echo "AK Homebrew Integration Script"
    echo "=============================="
    echo ""
    
    case "$ACTION" in
        trigger-update)
            auto_generate_params
            
            if ! validate_params; then
                echo ""
                echo "Use --help for usage information"
                exit 1
            fi
            
            echo "Configuration:"
            echo "  Version: $VERSION"
            echo "  Release URL: $RELEASE_URL"
            echo "  SHA256: ${SHA256_CHECKSUM:0:16}..."
            echo "  Dry Run: $DRY_RUN"
            echo ""
            
            if trigger_homebrew_update "$VERSION" "$RELEASE_URL" "$SHA256_CHECKSUM"; then
                echo ""
                echo "Homebrew formula update triggered successfully!"
                echo "Check the tap repository for the automated pull request:"
                echo "https://github.com/$TAP_OWNER/$TAP_REPO/pulls"
            else
                echo ""
                echo "Failed to trigger Homebrew formula update"
                exit 1
            fi
            ;;
        *)
            echo "Error: No action specified" >&2
            echo ""
            usage >&2
            exit 1
            ;;
    esac
}

# Run main function
main