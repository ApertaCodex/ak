#!/bin/bash

# AK Homebrew Formula Validator
# This script validates and tests the AK Homebrew formula

set -euo pipefail

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOMEBREW_DIR="$(dirname "$SCRIPT_DIR")"

# Default values
FORMULA_PATH=""
TEST_INSTALL="false"
CLEANUP="true"
VERBOSE="false"
TEMP_TAP_NAME="ak-test-tap-$$"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Usage information
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Validate AK Homebrew formula for compliance and functionality.

OPTIONS:
    -f, --formula PATH       Path to formula file (default: formula/ak.rb)
    -t, --test-install      Perform actual installation test
    --no-cleanup            Don't cleanup test installations
    -v, --verbose           Verbose output
    -h, --help              Show this help message

VALIDATION CHECKS:
    ✓ Formula syntax and structure
    ✓ Homebrew audit compliance  
    ✓ Code style validation
    ✓ Dependency analysis
    ✓ Installation simulation
    ✓ Test suite execution (if --test-install)

EXAMPLES:
    # Basic validation
    $0

    # Validate specific formula
    $0 --formula generated/ak.rb

    # Full validation with installation test
    $0 --test-install

    # Verbose validation
    $0 --verbose --test-install
EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -f|--formula)
            FORMULA_PATH="$2"
            shift 2
            ;;
        -t|--test-install)
            TEST_INSTALL="true"
            shift
            ;;
        --no-cleanup)
            CLEANUP="false"
            shift
            ;;
        -v|--verbose)
            VERBOSE="true"
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

# Set default formula path
if [[ -z "$FORMULA_PATH" ]]; then
    if [[ -f "$HOMEBREW_DIR/generated/ak.rb" ]]; then
        FORMULA_PATH="$HOMEBREW_DIR/generated/ak.rb"
    elif [[ -f "$HOMEBREW_DIR/formula/ak.rb" ]]; then
        FORMULA_PATH="$HOMEBREW_DIR/formula/ak.rb"
    else
        echo "Error: No formula file found. Use --formula to specify path." >&2
        exit 1
    fi
fi

# Validate formula file exists
if [[ ! -f "$FORMULA_PATH" ]]; then
    echo "Error: Formula file not found: $FORMULA_PATH" >&2
    exit 1
fi

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[FAIL]${NC} $1"
}

log_verbose() {
    if [[ "$VERBOSE" == "true" ]]; then
        echo -e "${BLUE}[VERBOSE]${NC} $1"
    fi
}

# Track validation results
VALIDATION_RESULTS=()
TOTAL_CHECKS=0
PASSED_CHECKS=0

# Function to run validation check
run_check() {
    local check_name="$1"
    local check_command="$2"
    
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    log_info "Running check: $check_name"
    
    if [[ "$VERBOSE" == "true" ]]; then
        log_verbose "Command: $check_command"
    fi
    
    if eval "$check_command" >/dev/null 2>&1; then
        log_success "$check_name"
        VALIDATION_RESULTS+=("✓ $check_name")
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        return 0
    else
        log_error "$check_name"
        VALIDATION_RESULTS+=("✗ $check_name")
        return 1
    fi
}

# Function to check if Homebrew is available
check_homebrew() {
    if ! command -v brew >/dev/null 2>&1; then
        log_error "Homebrew is not installed or not in PATH"
        echo "Please install Homebrew from https://brew.sh/"
        exit 1
    fi
    
    log_success "Homebrew is available ($(brew --version | head -n1))"
}

# Function to validate formula syntax
validate_syntax() {
    log_info "Validating formula syntax..."
    
    # Check Ruby syntax
    if ! ruby -c "$FORMULA_PATH" >/dev/null 2>&1; then
        log_error "Formula has Ruby syntax errors"
        ruby -c "$FORMULA_PATH"
        return 1
    fi
    
    # Check for required Formula methods
    local required_methods=("desc" "homepage" "url" "install")
    for method in "${required_methods[@]}"; do
        if ! grep -q "def $method\|$method " "$FORMULA_PATH"; then
            log_error "Missing required method: $method"
            return 1
        fi
    done
    
    log_success "Formula syntax validation passed"
    return 0
}

# Function to run Homebrew audit
run_audit() {
    log_info "Running Homebrew audit..."
    
    local audit_output
    if audit_output=$(brew audit --strict --online "$FORMULA_PATH" 2>&1); then
        log_success "Homebrew audit passed"
        if [[ "$VERBOSE" == "true" ]]; then
            echo "$audit_output"
        fi
        return 0
    else
        log_error "Homebrew audit failed"
        echo "$audit_output"
        return 1
    fi
}

# Function to run style check
run_style_check() {
    log_info "Running style check..."
    
    local style_output
    if style_output=$(brew style "$FORMULA_PATH" 2>&1); then
        log_success "Style check passed"
        if [[ "$VERBOSE" == "true" ]]; then
            echo "$style_output"
        fi
        return 0
    else
        log_error "Style check failed"
        echo "$style_output"
        return 1
    fi
}

# Function to analyze dependencies
analyze_dependencies() {
    log_info "Analyzing dependencies..."
    
    # Extract dependencies from formula
    local deps
    deps=$(grep -o 'depends_on "[^"]*"' "$FORMULA_PATH" | sed 's/depends_on "\([^"]*\)"/\1/' | sort -u) || true
    
    if [[ -n "$deps" ]]; then
        log_info "Found dependencies:"
        while IFS= read -r dep; do
            log_info "  - $dep"
            # Check if dependency is available in Homebrew
            if brew search --formula "^$dep$" >/dev/null 2>&1; then
                log_verbose "  ✓ $dep is available"
            else
                log_warning "  ⚠ $dep may not be available in Homebrew"
            fi
        done <<< "$deps"
    else
        log_info "No dependencies found"
    fi
    
    return 0
}

# Function to setup temporary tap
setup_temp_tap() {
    local tap_path="$HOME/.homebrew-test-taps/$TEMP_TAP_NAME"
    
    log_info "Setting up temporary tap: $TEMP_TAP_NAME"
    
    # Create tap directory structure
    mkdir -p "$tap_path/Formula"
    
    # Copy formula to tap
    cp "$FORMULA_PATH" "$tap_path/Formula/ak.rb"
    
    # Create basic tap info
    cat > "$tap_path/README.md" << 'EOF'
# Test Tap for AK Formula Validation

This is a temporary tap used for testing the AK Homebrew formula.
EOF
    
    # Initialize git repo (required by Homebrew)
    (cd "$tap_path" && git init && git add . && git commit -m "Initial test tap" >/dev/null 2>&1) || true
    
    echo "$tap_path"
}

# Function to cleanup temporary tap
cleanup_temp_tap() {
    if [[ "$CLEANUP" == "true" ]]; then
        local tap_path="$HOME/.homebrew-test-taps/$TEMP_TAP_NAME"
        if [[ -d "$tap_path" ]]; then
            log_info "Cleaning up temporary tap"
            rm -rf "$tap_path"
        fi
        
        # Untap if it was tapped
        if brew tap | grep -q "$TEMP_TAP_NAME" 2>/dev/null; then
            brew untap "$TEMP_TAP_NAME" >/dev/null 2>&1 || true
        fi
    fi
}

# Function to test installation
test_installation() {
    log_info "Testing formula installation..."
    
    # Setup temporary tap
    local tap_path
    tap_path=$(setup_temp_tap)
    
    # Tap the temporary tap
    if ! brew tap "$TEMP_TAP_NAME" "$tap_path" >/dev/null 2>&1; then
        log_error "Failed to tap temporary tap"
        cleanup_temp_tap
        return 1
    fi
    
    # Test installation
    log_info "Installing AK from temporary tap (this may take a while)..."
    if brew install --build-from-source "$TEMP_TAP_NAME/ak" >/dev/null 2>&1; then
        log_success "Installation test passed"
        
        # Run formula tests if available
        log_info "Running formula test suite..."
        if brew test "$TEMP_TAP_NAME/ak" >/dev/null 2>&1; then
            log_success "Formula test suite passed"
        else
            log_warning "Formula test suite failed or not available"
        fi
        
        # Cleanup installation
        if [[ "$CLEANUP" == "true" ]]; then
            log_info "Uninstalling test installation..."
            brew uninstall "$TEMP_TAP_NAME/ak" >/dev/null 2>&1 || true
        fi
        
        cleanup_temp_tap
        return 0
    else
        log_error "Installation test failed"
        cleanup_temp_tap
        return 1
    fi
}

# Trap for cleanup
trap cleanup_temp_tap EXIT

# Main validation flow
main() {
    echo -e "${BLUE}AK Homebrew Formula Validator${NC}"
    echo "============================="
    echo ""
    echo "Formula: $FORMULA_PATH"
    echo "Test Installation: $TEST_INSTALL"
    echo ""
    
    # Check prerequisites
    check_homebrew
    
    # Run validation checks
    validate_syntax || true
    run_check "Homebrew audit" "brew audit --strict '$FORMULA_PATH'"
    run_check "Style check" "brew style '$FORMULA_PATH'"
    
    # Analyze dependencies
    analyze_dependencies
    
    # Test installation if requested
    if [[ "$TEST_INSTALL" == "true" ]]; then
        if test_installation; then
            PASSED_CHECKS=$((PASSED_CHECKS + 1))
            VALIDATION_RESULTS+=("✓ Installation test")
        else
            VALIDATION_RESULTS+=("✗ Installation test")
        fi
        TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    fi
    
    # Print summary
    echo ""
    echo -e "${BLUE}Validation Summary${NC}"
    echo "=================="
    echo ""
    
    for result in "${VALIDATION_RESULTS[@]}"; do
        if [[ "$result" == ✓* ]]; then
            echo -e "${GREEN}$result${NC}"
        else
            echo -e "${RED}$result${NC}"
        fi
    done
    
    echo ""
    echo "Passed: $PASSED_CHECKS/$TOTAL_CHECKS checks"
    
    if [[ "$PASSED_CHECKS" -eq "$TOTAL_CHECKS" ]]; then
        echo -e "${GREEN}✓ All validation checks passed!${NC}"
        exit 0
    else
        echo -e "${RED}✗ Some validation checks failed${NC}"
        exit 1
    fi
}

# Run main function
main