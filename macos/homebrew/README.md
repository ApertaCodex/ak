# AK Homebrew Distribution

Complete Homebrew formula and tap infrastructure for distributing AK on macOS.

## Quick Start

### For Users
```bash
brew tap apertacodex/ak https://github.com/apertacodex/ak.git
brew install ak
```

### For Maintainers
```bash
# Generate and publish a new formula
./scripts/generate-formula.sh --version 3.3.0
./scripts/validate-formula.sh --test-install  
./scripts/publish-to-tap.sh --create-pr
```

## Directory Structure

```
macos/homebrew/
├── README.md                      # This file
├── formula/
│   └── ak.rb                     # Main Homebrew formula template
├── tap-template/                 # Template for homebrew-ak repository
│   ├── README.md                 # Tap repository documentation
│   └── .github/workflows/        # Automated testing and updates
│       ├── update-formula.yml    # Auto-update on releases
│       └── test-formula.yml      # Formula testing workflow
├── scripts/                      # Automation and maintenance scripts
│   ├── generate-formula.sh       # Generate formula from template
│   ├── publish-to-tap.sh         # Publish formula to tap repository
│   ├── validate-formula.sh       # Comprehensive formula validation
│   └── homebrew-integration.sh   # Integration hooks for main repo
├── completions/                  # Shell completion files
│   ├── ak.bash                   # Bash completion
│   ├── _ak                       # Zsh completion  
│   └── ak.fish                   # Fish completion
└── docs/                         # Documentation
    ├── README.md                 # Feature overview and links
    ├── SETUP_GUIDE.md            # Complete maintainer setup guide
    └── USER_INSTALLATION_GUIDE.md # User installation instructions
```

## Features

### ✅ Professional Homebrew Formula
- Follows all Homebrew best practices and conventions
- Supports both CLI (`ak`) and GUI (`ak-gui`) components
- Proper Qt6 dependency management with version constraints
- Shell completion integration for bash, zsh, and fish
- Comprehensive test suite including actual installation tests
- Passes `brew audit --strict` and `brew style` validation

### ✅ Complete Automation Pipeline
- **Formula Generation**: Automated formula creation with proper checksums
- **Validation Framework**: Multi-level testing including syntax, compliance, and installation
- **Publishing Automation**: Direct publishing or pull request creation
- **CI/CD Integration**: GitHub Actions for automated updates and testing

### ✅ Multi-Platform Testing  
- Tests on macOS 12 (Monterey), 13 (Ventura), and 14 (Sonoma)
- Intel and Apple Silicon compatibility
- Bottle building for faster installation
- Dependency linkage validation

### ✅ Shell Integration
- **Bash**: Complete command and option completion with dynamic suggestions
- **Zsh**: Advanced completions with descriptions and contextual help
- **Fish**: Native Fish shell completions with dynamic key/profile completion
- Automatic installation and configuration through formula

### ✅ Comprehensive Documentation
- **User Guide**: Step-by-step installation and usage instructions
- **Maintainer Guide**: Complete setup and maintenance procedures  
- **Integration Guide**: CI/CD pipeline integration examples
- **Troubleshooting**: Common issues and diagnostic procedures

## Repository Structure

This creates infrastructure for two repositories:

### Main Repository (apertacodex/ak)
Contains this homebrew infrastructure in `macos/homebrew/` directory.

### Tap Repository (apertacodex/homebrew-ak)  
Created from `tap-template/` containing:
- `Formula/ak.rb` - The actual Homebrew formula
- `README.md` - Tap documentation and usage instructions
- `.github/workflows/` - Automated testing and update workflows

## Workflow Integration

### Manual Release Process
1. **Build and Test**: Ensure AK builds and tests pass locally
2. **Generate Formula**: `./scripts/generate-formula.sh --version X.Y.Z`
3. **Validate Formula**: `./scripts/validate-formula.sh --test-install`
4. **Publish Update**: `./scripts/publish-to-tap.sh --create-pr`
5. **Review and Merge**: Review automated pull request and merge

### Automated Release Process
1. **Create Release**: Tag and create GitHub release in main repository
2. **Trigger Update**: GitHub Actions automatically calls homebrew integration
3. **Generate Formula**: CI generates updated formula with proper checksums  
4. **Create PR**: Automated pull request created in tap repository
5. **Test and Merge**: CI tests formula and merges on success

### CI/CD Integration Example
Add to your main repository's `.github/workflows/release.yml`:

```yaml
- name: Update Homebrew Formula
  if: github.event_name == 'release'
  run: |
    ./macos/homebrew/scripts/homebrew-integration.sh \
      --trigger-update \
      --version ${{ github.event.release.tag_name }} \
      --github-token ${{ secrets.HOMEBREW_TAP_TOKEN }}
```

## Quality Assurance

### Formula Validation
- ✅ Ruby syntax validation
- ✅ Homebrew audit compliance (`brew audit --strict`)
- ✅ Style guide compliance (`brew style`)
- ✅ Dependency analysis and verification
- ✅ Installation simulation and testing
- ✅ Test suite execution (`brew test`)

### Testing Matrix
- ✅ macOS 12.x (Monterey) - Intel and Apple Silicon
- ✅ macOS 13.x (Ventura) - Intel and Apple Silicon  
- ✅ macOS 14.x (Sonoma) - Intel and Apple Silicon
- ✅ Qt6 integration testing
- ✅ Shell completion functionality
- ✅ CLI and GUI component verification

### Security and Compliance
- ✅ SHA256 checksum verification for all downloads
- ✅ HTTPS-only URLs for security
- ✅ Code signing validation (when applicable)
- ✅ Dependency vulnerability scanning
- ✅ Reproducible builds

## Template System

Uses placeholder-based templating for maintainable formulas:

| Placeholder | Purpose | Example Value |
|-------------|---------|---------------|
| `{{VERSION}}` | Release version | `3.3.0` |
| `{{RELEASE_URL}}` | Download URL | `https://github.com/apertacodex/ak/archive/v3.3.0.tar.gz` |
| `{{SHA256}}` | Archive checksum | `abc123def456...` |
| `{{GITHUB_OWNER}}` | Repository owner | `apertacodex` |
| `{{GITHUB_REPO}}` | Repository name | `ak` |
| `{{LICENSE}}` | License type | `MIT` |

## Getting Started

### For New Maintainers
1. Read the [Setup Guide](docs/SETUP_GUIDE.md) for complete setup instructions
2. Create the homebrew-ak tap repository using the tap template
3. Configure GitHub secrets and permissions
4. Test the workflow with a formula generation and validation

### For Contributors  
1. Review the [User Installation Guide](docs/USER_INSTALLATION_GUIDE.md)
2. Test installation: `brew tap apertacodex/ak && brew install ak`
3. Report issues or suggest improvements via GitHub issues
4. Submit pull requests for formula or infrastructure improvements

### For Users
Simply install with: `brew tap apertacodex/ak https://github.com/apertacodex/ak.git && brew install ak`

See [User Installation Guide](docs/USER_INSTALLATION_GUIDE.md) for detailed instructions.

## Support and Resources

- **Main Repository**: https://github.com/apertacodex/ak
- **Homebrew Tap**: https://github.com/apertacodex/homebrew-ak  
- **Issues**: https://github.com/apertacodex/ak/issues
- **Discussions**: https://github.com/apertacodex/ak/discussions
- **Homebrew Documentation**: https://docs.brew.sh/

## License

This Homebrew infrastructure is part of the AK project and follows the same license terms.

---

*This professional Homebrew distribution provides a complete, maintainable solution for distributing AK to macOS users via the most popular macOS package manager.*