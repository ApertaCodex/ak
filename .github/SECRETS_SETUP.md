# GitHub Actions Secrets Configuration Guide

This document explains which secrets need to be configured in GitHub Actions for the release workflow to function properly.

## Required Secrets

### 1. `GITHUB_TOKEN` 
**Status:** ? Automatically provided  
**Required:** No action needed - GitHub provides this automatically  
**Purpose:** Used for creating releases, downloading artifacts, and pushing commits

## Optional Secrets (for GPG signing)

The following secrets are **optional** but recommended for proper package signing:

### 2. `AK_REPO_GPG_PRIVKEY`
**Status:** ?? Optional but recommended  
**Required:** Only if you want to sign APT repository Release files  
**Purpose:** Signs APT repository metadata for security

**How to set it up:**
```bash
# Export your GPG private key (base64 encoded)
gpg --export-secret-keys --armor YOUR_KEY_ID | base64 -w 0 > gpg_private_key.txt
# Copy the contents of gpg_private_key.txt to GitHub Secrets
```

**Where it's used:** APT repository Release file signing

### 3. `AK_REPO_GPG_PASSPHRASE`
**Status:** ?? Optional but recommended  
**Required:** Only if `AK_REPO_GPG_PRIVKEY` is set  
**Purpose:** Passphrase to unlock the GPG private key

**How to set it up:**
- Simply add your GPG key passphrase as a secret (plain text)

**Where it's used:** APT repository Release file signing

### 4. `DEBSIGN_KEYID`
**Status:** ?? Optional but recommended  
**Required:** Only if you want to publish to Launchpad PPA  
**Purpose:** GPG key ID/fingerprint for signing Debian source packages

**How to find your key ID:**
```bash
# List your GPG keys
gpg --list-secret-keys --keyid-format=long

# Look for a line like:
# sec   rsa4096/ABCD1234EFGH5678 2024-01-01 [SC]
#                               ^^^^^^^^^^^^^^^^
# This is your key ID (full fingerprint)

# Or get just the short ID:
gpg --list-secret-keys --keyid-format=short | grep "^sec" | awk '{print $2}' | cut -d'/' -f2
```

**How to set it up:**
- Add the full fingerprint (40 characters) or short key ID (16 characters) as a secret

**Where it's used:** Launchpad PPA package signing

### 5. `GPG_PRIVATE_KEY`
**Status:** ?? Optional  
**Required:** Only if you want to publish to Launchpad PPA and need to import the key  
**Purpose:** Full GPG private key for PPA signing (alternative to using a pre-imported key)

**How to set it up:**
```bash
# Export your GPG private key (base64 encoded)
gpg --export-secret-keys --armor YOUR_KEY_ID | base64 -w 0 > gpg_ppa_key.txt
# Copy the contents of gpg_ppa_key.txt to GitHub Secrets
```

**Where it's used:** Launchpad PPA package signing (if key needs to be imported)

## Summary

### Minimum Setup (workflow will run but without signing):
- ? **No secrets needed** - `GITHUB_TOKEN` is automatic
- ?? APT repository won't be signed (but will still work)
- ?? PPA publishing will be skipped

### Recommended Setup (full functionality):
1. `AK_REPO_GPG_PRIVKEY` - Base64-encoded GPG private key
2. `AK_REPO_GPG_PASSPHRASE` - GPG key passphrase
3. `DEBSIGN_KEYID` - GPG key ID/fingerprint
4. `GPG_PRIVATE_KEY` - Base64-encoded GPG private key (if different from AK_REPO_GPG_PRIVKEY)

## How to Add Secrets to GitHub

1. Go to your repository on GitHub
2. Click **Settings** ? **Secrets and variables** ? **Actions**
3. Click **New repository secret**
4. Enter the secret name (e.g., `AK_REPO_GPG_PRIVKEY`)
5. Paste the secret value
6. Click **Add secret**

## Testing Your Setup

After adding secrets, you can test the workflow by:

1. **Manual trigger:**
   - Go to **Actions** ? **Release AK** ? **Run workflow**
   - Select version bump type and run

2. **Push to main:**
   - Make a commit to the `main` branch
   - The workflow will automatically trigger

## Troubleshooting

### If PPA publishing fails:
- Check that `DEBSIGN_KEYID` is set correctly
- Verify the key ID matches your Launchpad account
- Ensure the GPG key is uploaded to Launchpad: https://launchpad.net/~your-username/+editpgpkeys

### If APT repository signing fails:
- Verify `AK_REPO_GPG_PRIVKEY` is base64-encoded correctly
- Check that `AK_REPO_GPG_PASSPHRASE` matches your key
- The workflow will continue without signing if secrets are missing

### If version bumping fails:
- Check that CMake is installing correctly
- Verify the `cmake/version_bump.cmake` script exists and is correct

## Notes

- All secrets are encrypted and only accessible during workflow runs
- Secrets never appear in logs (GitHub automatically masks them)
- You can use the same GPG key for both APT repo and PPA, or different keys
- The workflow gracefully handles missing secrets and will skip optional steps
