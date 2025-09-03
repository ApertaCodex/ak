#!/bin/bash

# AK APT Repository Account Setup Script
# This script helps you create and configure accounts for publishing AK packages

set -e

echo "🚀 AK APT Repository Account Setup"
echo "=================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_step() {
    echo -e "${BLUE}📋 $1${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}ℹ️  $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

# Check dependencies
check_dependencies() {
    print_step "Checking dependencies..."
    
    local missing_deps=()
    
    command -v curl >/dev/null 2>&1 || missing_deps+=("curl")
    command -v gpg >/dev/null 2>&1 || missing_deps+=("gnupg")
    command -v git >/dev/null 2>&1 || missing_deps+=("git")
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing dependencies: ${missing_deps[*]}"
        print_info "Install with: sudo apt install ${missing_deps[*]}"
        exit 1
    fi
    
    print_success "All dependencies available"
}

# Generate GPG key for package signing
setup_gpg_key() {
    print_step "Setting up GPG key for package signing..."
    
    if gpg --list-secret-keys | grep -q sec; then
        print_info "GPG key already exists. Using existing key."
        return
    fi
    
    print_info "No GPG key found. Let's create one..."
    echo
    echo "Please provide the following information for your GPG key:"
    read -p "Full Name: " gpg_name
    read -p "Email Address: " gpg_email
    
    # Generate GPG key non-interactively
    cat > gpg_key_config <<EOF
Key-Type: RSA
Key-Length: 4096
Subkey-Type: RSA
Subkey-Length: 4096
Name-Real: $gpg_name
Name-Email: $gpg_email
Expire-Date: 0
EOF

    print_info "Generating GPG key (this may take a while)..."
    gpg --batch --generate-key gpg_key_config
    rm gpg_key_config
    
    # Get key ID
    GPG_KEY_ID=$(gpg --list-secret-keys --keyid-format=long | grep sec | head -1 | sed 's|.*/||' | sed 's| .*||')
    
    print_success "GPG key generated: $GPG_KEY_ID"
    
    # Export public key
    gpg --armor --export $GPG_KEY_ID > ak-repository-key.gpg
    print_success "Public key exported to ak-repository-key.gpg"
    
    # Set as default signing key
    echo "default-key $GPG_KEY_ID" >> ~/.gnupg/gpg.conf
    
    echo
    print_info "IMPORTANT: Keep your GPG key backed up!"
    print_info "Export your private key: gpg --armor --export-secret-keys $GPG_KEY_ID > private-key.gpg"
}

# Open account signup pages
open_account_pages() {
    print_step "Opening account signup pages..."
    
    local urls=(
        "https://launchpad.net/"
        "https://github.com/"
    )
    
    if command -v xdg-open >/dev/null 2>&1; then
        for url in "${urls[@]}"; do
            print_info "Opening: $url"
            xdg-open "$url" 2>/dev/null &
            sleep 2
        done
        print_success "Account signup pages opened in browser"
    else
        print_info "Please manually visit these URLs to create accounts:"
        for url in "${urls[@]}"; do
            echo "  - $url"
        done
    fi
}

# Create repository setup script
create_repo_setup_script() {
    print_step "Creating repository setup script..."
    
    cat > setup-ak-repository.sh <<'EOF'
#!/bin/bash
# AK API Key Manager Repository Setup
# Run this script on user machines to add the AK repository

set -e

echo "🔧 Setting up AK API Key Manager repository..."

# Add GPG key
if [ -f "ak-repository-key.gpg" ]; then
    sudo apt-key add ak-repository-key.gpg
    echo "✅ GPG key added"
else
    echo "❌ GPG key file not found. Please ensure ak-repository-key.gpg is in the current directory."
    exit 1
fi

# Add repository (you'll need to update this URL)
echo "deb https://your-username.github.io/ak-apt-repo stable main" | sudo tee /etc/apt/sources.list.d/ak.list

# Update package list
sudo apt update

echo "✅ Repository setup complete!"
echo "📦 Install AK with: sudo apt install ak"
EOF

    chmod +x setup-ak-repository.sh
    print_success "Repository setup script created: setup-ak-repository.sh"
}

# Create GitHub Pages APT repository
create_github_pages_repo() {
    print_step "Creating GitHub Pages APT repository structure..."
    
    mkdir -p ak-apt-repo/{dists/stable/main/binary-amd64,pool/main}
    cd ak-apt-repo
    
    # Copy the package
    if [ -f "../ak_2.0.0-1_amd64.deb" ]; then
        cp ../ak_2.0.0-1_amd64.deb pool/main/
        print_success "Package copied to repository"
    else
        print_info "Package not found. Please build the package first with: dpkg-buildpackage -us -uc -b"
    fi
    
    # Generate Packages files
    cd dists/stable/main/binary-amd64
    if [ -f "../../../../pool/main/ak_2.0.0-1_amd64.deb" ]; then
        dpkg-scanpackages ../../../../pool/main /dev/null | gzip -9c > Packages.gz
        dpkg-scanpackages ../../../../pool/main /dev/null > Packages
        print_success "Packages files generated"
    fi
    
    # Create Release file
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
EOF

    if [ -f "main/binary-amd64/Packages" ]; then
        echo "MD5Sum:" >> Release
        md5sum main/binary-amd64/Packages.gz main/binary-amd64/Packages | sed 's|main/binary-amd64/||' >> Release
        echo "SHA1:" >> Release
        sha1sum main/binary-amd64/Packages.gz main/binary-amd64/Packages | sed 's|main/binary-amd64/||' >> Release
        echo "SHA256:" >> Release
        sha256sum main/binary-amd64/Packages.gz main/binary-amd64/Packages | sed 's|main/binary-amd64/||' >> Release
        
        # Sign Release file if GPG key is available
        if gpg --list-secret-keys | grep -q sec; then
            gpg --clearsign -o InRelease Release
            gpg --armor --detach-sig -o Release.gpg Release
            print_success "Release file signed"
        fi
        
        print_success "Release file created"
    fi
    
    cd ..
    print_success "GitHub Pages APT repository created in ak-apt-repo/"
}

# Create GitHub repository
create_github_repo() {
    print_step "Checking GitHub CLI..."
    
    if command -v gh >/dev/null 2>&1; then
        print_info "GitHub CLI found. You can create the repository with:"
        echo "  gh repo create ak-apt-repo --public --description 'APT repository for AK API Key Manager'"
        echo "  cd ak-apt-repo"
        echo "  git init"
        echo "  git add ."
        echo "  git commit -m 'Initial APT repository setup'"
        echo "  git branch -M main"
        echo "  git remote add origin https://github.com/apertacodex/ak-apt-repo.git"
        echo "  git push -u origin main"
        echo "  # Enable GitHub Pages in repository settings"
    else
        print_info "GitHub CLI not found. Please:"
        echo "  1. Install GitHub CLI: sudo apt install gh"
        echo "  2. Or manually create repository at: https://github.com/new"
        echo "  3. Repository name: ak-apt-repo"
        echo "  4. Make it public"
        echo "  5. Enable GitHub Pages in repository settings"
    fi
}

# Show account setup instructions
show_account_instructions() {
    print_step "Account Setup Instructions"
    echo
    print_info "LAUNCHPAD PPA (Ubuntu/Debian official repositories):"
    echo "  1. Visit: https://launchpad.net/"
    echo "  2. Click 'Log in / Register'"
    echo "  3. Choose 'I am a new Ubuntu One user'"
    echo "  4. Fill in your details"
    echo "  5. Verify your email"
    echo "  6. Upload your GPG key (generated above)"
    echo "  7. Create a PPA for your packages"
    echo
    print_info "GITHUB (For GitHub Packages or Pages hosting):"
    echo "  1. Visit: https://github.com/"
    echo "  2. Sign up for free account"
    echo "  3. Create repository: ak-apt-repo"
    echo "  4. Enable GitHub Pages in repository settings"
    echo
    print_info "After creating accounts:"
    echo "  - Upload your GPG public key to services"
    echo "  - Configure your repositories"
    echo "  - Test package installation"
}

# Main execution
main() {
    print_step "Starting AK APT Repository Account Setup"
    echo
    
    check_dependencies
    echo
    
    # Ask user what they want to do
    echo "What would you like to do?"
    echo "1. Generate GPG key for package signing"
    echo "2. Create GitHub Pages APT repository"
    echo "3. Create repository setup script"
    echo "4. Open account signup pages"
    echo "5. Show account setup instructions"
    echo "6. Do everything (recommended)"
    echo
    read -p "Choose option (1-6): " choice
    
    case $choice in
        1)
            setup_gpg_key
            ;;
        2)
            create_github_pages_repo
            ;;
        3)
            create_repo_setup_script
            ;;
        4)
            open_account_pages
            ;;
        5)
            show_account_instructions
            ;;
        6)
            setup_gpg_key
            echo
            create_github_pages_repo
            echo
            create_repo_setup_script
            echo
            show_account_instructions
            echo
            open_account_pages
            ;;
        *)
            print_error "Invalid choice"
            exit 1
            ;;
    esac
    
    echo
    print_success "Setup completed!"
    print_info "Next steps:"
    echo "  1. Create accounts on Launchpad and/or GitHub"
    echo "  2. Upload your GPG public key (ak-repository-key.gpg)"
    echo "  3. Create your APT repository"
    echo "  4. Upload your packages"
    echo "  5. Share repository setup script with users"
}

# Run main function
main "$@"