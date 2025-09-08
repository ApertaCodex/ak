# Debian Publishing Guide for AK

This document outlines the process for publishing AK to Debian-based repositories.

## Publishing to PPA

AK is published to the Launchpad PPA for easy installation on Ubuntu and compatible distributions.

```bash
# Add the repository
sudo add-apt-repository ppa:apertacodex/ak

# Update package list
sudo apt update

# Install AK
sudo apt install ak
```

## Publishing to GitHub APT Repository

AK is also published to a GitHub Pages-based APT repository for distributions that don't support PPAs.

```bash
# Download and run the setup script
curl -fsSL https://apertacodex.github.io/ak/setup-repository.sh | bash

# Install AK
sudo apt install ak
```

## Release Process

The release process is automated through the `release.sh` script, which:

1. Bumps the version number
2. Builds the binary and packages
3. Updates the APT repository
4. Commits and pushes changes to GitHub
5. Publishes to the PPA

## Repository Structure

- `ak-apt-repo/`: GitHub Pages APT repository
- `debian/`: Debian packaging files
- `Formula/`: Homebrew formula