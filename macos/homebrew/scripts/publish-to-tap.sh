#!/bin/bash

# AK Homebrew Tap Publisher
# This script publishes the generated formula to the homebrew-ak tap repository

set -euo pipefail

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOMEBREW_DIR="$(dirname "$SCRIPT_DIR")"

# Default values
FORMULA_PATH=""
TAP_REPO_URL=""
TAP_LOCAL_PATH=""
GITHUB_TOKEN="${GITHUB_TOKEN:-}"
COMMIT_MESSAGE=""
BRANCH_NAME=""
CREATE_PR="false"
DRY_RUN="false"

# Usage information
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Publish AK Homebrew formula to the tap repository.

OPTIONS:
    -f, --formula PATH        Path to generated formula file (required)
    -t, --tap-url URL         Tap repository URL (git)
    -l, --local-path PATH     Local tap repository path
    -m, --message TEXT        Commit message
    -b, --branch NAME         Branch name for changes
    --create-pr              Create pull request instead of direct push
    --dry-run                Show what would be done without making changes
    --token TOKEN            GitHub token for authentication
    -h, --help               Show this help message

EXAMPLES:
    # Publish to tap with auto-commit
    $0 --formula generated/ak.rb --tap-url https://github.com/ak-team/homebrew-ak.git

    # Create pull request instead of direct push
    $0 --formula generated/ak.rb --create-pr --branch update-ak-3.3.0

    # Dry run to see what would happen
    $0 --formula generated/ak.rb --dry-run

ENVIRONMENT VARIABLES:
    GITHUB_TOKEN             GitHub personal access token
    AK_TAP_REPO_URL         Default tap repository URL
    AK_TAP_LOCAL_PATH       Default local tap path
EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -f|--formula)
            FORMULA_PATH="$2"
            shift 2
            ;;
        -t|--tap-url)
            TAP_REPO_URL="$2"
            shift 2
            ;;
        -l|--local-path)
            TAP_LOCAL_PATH="$2"
            shift 2
            ;;
        -m|--message)
            COMMIT_MESSAGE="$2"
            shift 2
            ;;
        -b|--branch)
            BRANCH_NAME="$2"
            shift 2
            ;;
        --create-pr)
            CREATE_PR="true"
            shift
            ;;
        --dry-run)
            DRY_RUN="true"
            shift
            ;;
        --token)
            GITHUB_TOKEN="$2"
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
if [[ -z "$FORMULA_PATH" ]]; then
    echo "Error: Formula path is required. Use --formula to specify." >&2
    exit 1
fi

if [[ ! -f "$FORMULA_PATH" ]]; then
    echo "Error: Formula file not found: $FORMULA_PATH" >&2
    exit 1
fi

# Use environment defaults if not specified
TAP_REPO_URL="${TAP_REPO_URL:-${AK_TAP_REPO_URL:-}}"
TAP_LOCAL_PATH="${TAP_LOCAL_PATH:-${AK_TAP_LOCAL_PATH:-}}"

# Auto-generate values if not provided
if [[ -z "$TAP_LOCAL_PATH" ]]; then
    TAP_LOCAL_PATH="$HOMEBREW_DIR/tap-workspace"
fi

if [[ -z "$COMMIT_MESSAGE" ]]; then
    # Extract version from formula for commit message
    VERSION=$(grep -o 'version "[^"]*"' "$FORMULA_PATH" | sed 's/version "\(.*\)"/\1/')
    COMMIT_MESSAGE="Update AK formula to version $VERSION"
fi

if [[ -z "$BRANCH_NAME" ]] && [[ "$CREATE_PR" == "true" ]]; then
    VERSION=$(grep -o 'version "[^"]*"' "$FORMULA_PATH" | sed 's/version "\(.*\)"/\1/')
    BRANCH_NAME="update-ak-$VERSION"
fi

# Function to run commands with dry-run support
run_cmd() {
    if [[ "$DRY_RUN" == "true" ]]; then
        echo "[DRY RUN] $*"
    else
        echo "Running: $*"
        "$@"
    fi
}

# Function to clone or update tap repository
setup_tap_repo() {
    if [[ -z "$TAP_REPO_URL" ]]; then
        echo "Error: Tap repository URL not specified and AK_TAP_REPO_URL not set" >&2
        exit 1
    fi

    if [[ -d "$TAP_LOCAL_PATH" ]]; then
        echo "Info: Updating existing tap repository at $TAP_LOCAL_PATH"
        (cd "$TAP_LOCAL_PATH" && run_cmd git pull origin main)
    else
        echo "Info: Cloning tap repository to $TAP_LOCAL_PATH"
        run_cmd git clone "$TAP_REPO_URL" "$TAP_LOCAL_PATH"
    fi
}

# Function to setup Git authentication
setup_git_auth() {
    if [[ -n "$GITHUB_TOKEN" ]]; then
        echo "Info: Setting up Git authentication with token"
        if [[ "$DRY_RUN" == "false" ]]; then
            git config --global credential.helper store
            echo "https://$GITHUB_TOKEN@github.com" > ~/.git-credentials
        fi
    fi
}

# Function to create tap structure if needed
setup_tap_structure() {
    local formula_dir="$TAP_LOCAL_PATH/Formula"
    
    if [[ ! -d "$formula_dir" ]]; then
        echo "Info: Creating Formula directory"
        run_cmd mkdir -p "$formula_dir"
    fi
    
    # Copy README and other tap files if they don't exist
    if [[ ! -f "$TAP_LOCAL_PATH/README.md" ]] && [[ -f "$HOMEBREW_DIR/tap-template/README.md" ]]; then
        echo "Info: Copying tap README"
        run_cmd cp "$HOMEBREW_DIR/tap-template/README.md" "$TAP_LOCAL_PATH/"
    fi
    
    # Copy GitHub workflows if they don't exist
    if [[ ! -d "$TAP_LOCAL_PATH/.github/workflows" ]] && [[ -d "$HOMEBREW_DIR/tap-template/.github" ]]; then
        echo "Info: Copying GitHub workflows"
        run_cmd cp -r "$HOMEBREW_DIR/tap-template/.github" "$TAP_LOCAL_PATH/"
    fi
}

# Function to publish formula
publish_formula() {
    local dest_formula="$TAP_LOCAL_PATH/Formula/ak.rb"
    
    echo "Info: Copying formula to tap repository"
    run_cmd cp "$FORMULA_PATH" "$dest_formula"
    
    if [[ "$DRY_RUN" == "false" ]]; then
        cd "$TAP_LOCAL_PATH"
        
        # Check if there are changes
        if git diff --quiet; then
            echo "Info: No changes detected in tap repository"
            return 0
        fi
        
        # Stage changes
        run_cmd git add -A
        
        # Create branch if requested
        if [[ -n "$BRANCH_NAME" ]]; then
            echo "Info: Creating branch $BRANCH_NAME"
            run_cmd git checkout -b "$BRANCH_NAME"
        fi
        
        # Commit changes
        run_cmd git commit -m "$COMMIT_MESSAGE"
        
        # Push changes
        if [[ "$CREATE_PR" == "true" ]] && [[ -n "$BRANCH_NAME" ]]; then
            echo "Info: Pushing branch for pull request"
            run_cmd git push origin "$BRANCH_NAME"
            echo ""
            echo "Branch pushed successfully!"
            echo "Create a pull request at: $TAP_REPO_URL/compare/$BRANCH_NAME"
        else
            echo "Info: Pushing directly to main branch"
            run_cmd git push origin main
        fi
    fi
}

# Function to validate tap after publishing
validate_tap() {
    if command -v brew >/dev/null 2>&1 && [[ "$DRY_RUN" == "false" ]]; then
        echo ""
        echo "Info: Validating tap installation"
        local tap_name
        tap_name=$(basename "$TAP_REPO_URL" .git)
        tap_name=${tap_name#homebrew-}
        
        # Try to tap and install locally
        if brew tap --repair; then
            echo "✓ Tap validation passed"
        else
            echo "⚠ Tap validation failed"
        fi
    else
        echo "Info: Skipping tap validation (Homebrew not available or dry run)"
    fi
}

echo "AK Homebrew Tap Publisher"
echo "========================="
echo ""
echo "Configuration:"
echo "  Formula: $FORMULA_PATH"
echo "  Tap URL: $TAP_REPO_URL"
echo "  Local Path: $TAP_LOCAL_PATH"
echo "  Commit Message: $COMMIT_MESSAGE"
echo "  Branch: ${BRANCH_NAME:-main}"
echo "  Create PR: $CREATE_PR"
echo "  Dry Run: $DRY_RUN"
echo ""

# Main execution flow
setup_git_auth
setup_tap_repo
setup_tap_structure
publish_formula
validate_tap

echo ""
echo "Publishing completed successfully!"

if [[ "$DRY_RUN" == "true" ]]; then
    echo ""
    echo "This was a dry run - no changes were made."
    echo "Remove --dry-run to publish for real."
fi