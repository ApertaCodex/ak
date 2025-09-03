# AK - Debian Package Publishing Guide

This guide covers how to publish the AK (API Key Manager) to Linux APT repositories for easy installation via `apt install ak`.

## Quick Start

The project now includes complete Debian packaging and APT repository. Users can install via:

### Method 1: Repository Setup (Recommended)
```bash
curl -fsSL https://apertacodex.github.io/ak/setup-repository.sh | bash
sudo apt install ak
```

### Method 2: Manual Repository Setup
```bash
# Add GPG key (modern method)
sudo mkdir -p /usr/share/keyrings
curl -fsSL https://apertacodex.github.io/ak/ak-repository-key.gpg | sudo tee /usr/share/keyrings/ak-archive-keyring.gpg > /dev/null

# Add repository with keyring specification
echo "deb [signed-by=/usr/share/keyrings/ak-archive-keyring.gpg] https://apertacodex.github.io/ak/ak-apt-repo stable main" | sudo tee /etc/apt/sources.list.d/ak.list

# Update and install
sudo apt update && sudo apt install ak
```

### Method 3: Direct Package Download
```bash
wget https://apertacodex.github.io/ak/ak-apt-repo/pool/main/ak_2.2.0-1_amd64.deb
sudo dpkg -i ak_2.2.0-1_amd64.deb
sudo apt-get install -f  # Fix dependencies if needed
```

## 📦 Package Details

- **Package Name**: `ak`
- **Version**: `2.0.0-1` 
- **Architecture**: `amd64` (can be built for other architectures)
- **Dependencies**: Minimal system dependencies only
- **Size**: ~68KB
- **Includes**: Binary, shell completions (bash/zsh/fish), documentation

## 🔧 Building the Package

### Prerequisites
```bash
# Install build dependencies
sudo apt update
sudo apt install build-essential debhelper-compat cmake git dpkg-dev

# Ensure C++17 compiler
sudo apt install g++-7 # or newer
```

### Build Commands
```bash
# Clean build (recommended)
rm -rf obj-x86_64-linux-gnu debian/.debhelper debian/debhelper-build-stamp

# Build unsigned package (for testing)
dpkg-buildpackage -us -uc -b

# Build signed package (for production)
dpkg-buildpackage -b
```

### Generated Files
- `../ak_2.0.0-1_amd64.deb` - Main package
- `../ak-dbgsym_2.0.0-1_amd64.ddeb` - Debug symbols
- `../ak_2.0.0-1_amd64.buildinfo` - Build information
- `../ak_2.0.0-1_amd64.changes` - Changes file

## 🔐 GPG Key Setup for Package Signing

### 1. Generate GPG Key
```bash
# Generate new GPG key (if you don't have one)
gpg --full-generate-key
# Choose: RSA and RSA, 4096 bits, no expiration
# Use your email as maintainer

# List keys to get Key ID
gpg --list-secret-keys --keyid-format=long
```

### 2. Export Public Key
```bash
# Replace YOUR_KEY_ID with your actual key ID
export GPG_KEY_ID="YOUR_KEY_ID"
gpg --armor --export $GPG_KEY_ID > ak-archive-key.gpg
```

### 3. Configure for Package Signing
```bash
# Set default signing key
echo "default-key $GPG_KEY_ID" >> ~/.gnupg/gpg.conf
```

## 🗂️ APT Repository Setup

### Option 1: GitHub Packages (Recommended)

1. **Create Repository Structure**
```bash
mkdir -p apt-repo/{dists/stable/main/binary-amd64,pool/main}
cd apt-repo
```

2. **Copy Package**
```bash
cp ../ak_2.0.0-1_amd64.deb pool/main/
```

3. **Generate Packages File**
```bash
cd dists/stable/main/binary-amd64
dpkg-scanpackages ../../../../pool/main /dev/null | gzip -9c > Packages.gz
dpkg-scanpackages ../../../../pool/main /dev/null > Packages
```

4. **Create Release File**
```bash
cd ../..
cat > Release <<EOF
Archive: stable
Codename: stable
Components: main
Date: $(date -Ru)
Description: AK API Key Manager Repository
Label: AK Repository
Origin: AK
Suite: stable
Version: 1.0
Architectures: amd64
MD5Sum:
$(md5sum main/binary-amd64/Packages.gz main/binary-amd64/Packages | sed 's|main/binary-amd64/||')
SHA1:
$(sha1sum main/binary-amd64/Packages.gz main/binary-amd64/Packages | sed 's|main/binary-amd64/||')
SHA256:
$(sha256sum main/binary-amd64/Packages.gz main/binary-amd64/Packages | sed 's|main/binary-amd64/||')
EOF
```

5. **Sign Release File**
```bash
gpg --clearsign -o InRelease Release
gpg --armor --detach-sig -o Release.gpg Release
```

### Option 2: Launchpad PPA

1. **Create Launchpad Account**: https://launchpad.net
2. **Upload GPG Key** to Launchpad
3. **Create PPA**: Your Name → Create PPA
4. **Build Source Package**:
```bash
# Build source package
dpkg-buildpackage -S -sa

# Upload to PPA
dput ppa:username/ak-ppa ../ak_2.0.0-1_source.changes
```

### Option 3: Official Debian Repository

1. **Become Debian Maintainer**: https://www.debian.org/devel/
2. **Package Review Process**: Submit to debian-mentors
3. **Upload via sponsor** or as DD (Debian Developer)

## 📋 Repository Configuration Scripts

### Create APT Repository Setup Script
```bash
cat > setup-ak-repo.sh <<'EOF'
#!/bin/bash
set -e

echo "🔧 Setting up AK API Key Manager repository..."

# Add GPG key
curl -fsSL https://yourrepo.com/ak-archive-key.gpg | sudo apt-key add -

# Add repository
echo "deb https://yourrepo.com/apt-repo stable main" | sudo tee /etc/apt/sources.list.d/ak.list

# Update package list
sudo apt update

echo "✅ Repository setup complete!"
echo "📦 Install AK with: sudo apt install ak"
EOF

chmod +x setup-ak-repo.sh
```

### User Installation Instructions
```bash
# Method 1: Repository setup script
curl -fsSL https://yourrepo.com/setup-ak-repo.sh | bash
sudo apt install ak

# Method 2: Manual setup
curl -fsSL https://yourrepo.com/ak-archive-key.gpg | sudo apt-key add -
echo "deb https://yourrepo.com/apt-repo stable main" | sudo tee /etc/apt/sources.list.d/ak.list
sudo apt update
sudo apt install ak

# Method 3: Direct .deb download
wget https://yourrepo.com/ak_2.0.0-1_amd64.deb
sudo dpkg -i ak_2.0.0-1_amd64.deb
```

## 🚀 Publishing Automation

### GitHub Actions Workflow
```yaml
name: Build and Publish Debian Package
on:
  push:
    tags: ['v*']
    
jobs:
  build-deb:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    
    - name: Install build dependencies
      run: |
        sudo apt update
        sudo apt install build-essential debhelper-compat cmake git dpkg-dev
    
    - name: Build package
      run: dpkg-buildpackage -us -uc -b
    
    - name: Upload artifacts
      uses: actions/upload-artifact@v3
      with:
        name: debian-packages
        path: ../ak_*.deb
```

### Automated Repository Update Script
```bash
cat > update-repo.sh <<'EOF'
#!/bin/bash
set -e

REPO_DIR="apt-repo"
PACKAGE_FILE="../ak_2.0.0-1_amd64.deb"

# Copy new package
cp "$PACKAGE_FILE" "$REPO_DIR/pool/main/"

# Regenerate Packages files
cd "$REPO_DIR/dists/stable/main/binary-amd64"
dpkg-scanpackages ../../../../pool/main /dev/null | gzip -9c > Packages.gz
dpkg-scanpackages ../../../../pool/main /dev/null > Packages

# Update Release file
cd ../..
# ... (Release file generation code from above)

# Sign release
gpg --clearsign -o InRelease Release
gpg --armor --detach-sig -o Release.gpg Release

echo "✅ Repository updated successfully"
EOF

chmod +x update-repo.sh
```

## 📊 Package Information

### Installation Locations
- Binary: `/usr/bin/ak`
- Bash completion: `/etc/bash_completion.d/ak`
- Zsh completion: `/usr/share/zsh/site-functions/_ak`  
- Fish completion: `/usr/share/fish/completions/ak.fish`
- Documentation: `/usr/share/doc/ak/`

### Verification Commands
```bash
# Verify installation
dpkg -l ak
dpkg -L ak  # List installed files

# Test functionality
ak --version
ak --help

# Test completions work
# (Restart shell or source completion files)
```

## 🔍 Troubleshooting

### Common Issues

1. **Version Mismatch**
   - Ensure CMakeLists.txt and src/core/config.cpp versions match
   - Update debian/changelog accordingly

2. **GPG Signing Errors**
   ```bash
   # Check GPG setup
   gpg --list-secret-keys
   echo "test" | gpg --clearsign
   ```

3. **Dependencies**
   ```bash
   # Check package dependencies
   dpkg-deb -I ak_2.0.0-1_amd64.deb
   ```

4. **Architecture Issues**
   ```bash
   # Build for different architectures
   dpkg-buildpackage -a i386 -us -uc -b
   ```

## ⚡ Quick Commands Reference

```bash
# Build package
dpkg-buildpackage -us -uc -b

# Install locally
sudo dpkg -i ../ak_2.0.0-1_amd64.deb

# Remove package
sudo dpkg -r ak

# Check package contents
dpkg -c ../ak_2.0.0-1_amd64.deb

# Get package info
dpkg-deb -I ../ak_2.0.0-1_amd64.deb
```

## 📈 Next Steps

1. ✅ **Package Built**: Ready for distribution
2. 🔄 **Choose Repository**: GitHub/Launchpad/Official
3. 🔐 **Setup GPG**: For signed packages
4. 📤 **Upload**: To chosen repository
5. 📢 **Announce**: Update documentation with install instructions

---

**Ready for Production**: The AK package is fully prepared for APT repository distribution! 🎉