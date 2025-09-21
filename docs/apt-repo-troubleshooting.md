# AK APT Repository Troubleshooting

This guide fixes two common issues when using the AK APT repo at:
https://apertacodex.github.io/ak/ak-apt-repo

1) NO_PUBKEY A49D42072C7F5498
2) 404 Not Found when installing `ak` from the `plucky` distribution

---

End users: quick fix
- Symptom: `NO_PUBKEY ...` or `404 Not Found` for `ak_2.11.2_amd64.deb`
- Cause: Missing keyring and/or `plucky` metadata pointing to an old package that doesn’t exist in the pool. The `stable` distribution has the current 4.x packages.

Fix: switch to `stable` and install the keyring

- sudo install -d -m 0755 /usr/share/keyrings
- curl -fsSL https://apertacodex.github.io/ak/ak-repository-key.gpg | sudo tee /usr/share/keyrings/ak-archive-keyring.gpg >/dev/null
- echo "deb [arch=amd64 signed-by=/usr/share/keyrings/ak-archive-keyring.gpg] https://apertacodex.github.io/ak/ak-apt-repo/ stable main" | sudo tee /etc/apt/sources.list.d/ak-stable.list >/dev/null
- sudo rm -f /etc/apt/sources.list.d/ak-plucky.list
- sudo apt update
- sudo apt install ak

Alternative: use the helper script in this repo (defaults to `stable`)
- bash scripts/fix-ak-apt-repo.sh
- sudo apt install ak

Verification
- apt policy ak
- sudo apt update  # should not show signature/404 errors
- gpg --show-keys /usr/share/keyrings/ak-archive-keyring.gpg | grep -E 'A49D42072C7F5498|pub'

---

Maintainers: fix `plucky` 404 (regenerate indices)

Symptom
- Clients using `plucky` fetch metadata that references `ak_2.11.2_amd64.deb`:
  - dists/plucky/main/binary-amd64/Packages contains:
    - Version: 2.11.2
    - Filename: pool/main/ak_2.11.2_amd64.deb
- That .deb is not present in `ak-apt-repo/pool/main/`, causing `404`.

Goal
- Regenerate `dists/plucky/*` to reflect the actual contents of `pool/main/` (4.x series), then re-sign Release/InRelease.

Prerequisites
- Tools: `dpkg-scanpackages`, `apt-ftparchive`, `gpg` (with a signing key configured).
- The repo root on your local machine (not GitHub Pages):
  - ak-apt-repo/
    - pool/main/*.deb
    - dists/plucky/main/binary-amd64/ (Packages, Packages.gz)
    - dists/plucky/{InRelease,Release,Release.gpg}

Steps

1) Rebuild Packages files
- cd ak-apt-repo/dists/plucky/main/binary-amd64
- dpkg-scanpackages --multiversion ../../../../pool/main /dev/null > Packages
- gzip -fk Packages

2) Generate a new Release
Option A: with apt-ftparchive (recommended)
- cd ../../../..
- apt-ftparchive release plucky > dists/plucky/Release

Optional: provide a minimal configuration to set fields like Origin/Label:
Example release.conf (if you want custom headers)
  APT::FTPArchive::Release {
    Origin "ApertaCodex";
    Label "AK";
    Suite "plucky";
    Codename "plucky";
    Architectures "amd64";
    Components "main";
    Description "AK APT repository (plucky)";
  }
Then:
- apt-ftparchive -c release.conf release dists/plucky > dists/plucky/Release

Option B: manual checksum update (not recommended)
- Use `sha256sum`, `sha1sum`, `md5sum` to compute checksums for:
  - dists/plucky/main/binary-amd64/Packages
  - dists/plucky/main/binary-amd64/Packages.gz
- Populate MD5Sum/SHA1/SHA256 sections in Release accordingly, mirroring the format under `dists/stable/Release`.

3) Sign Release
- cd dists/plucky
- gpg --yes --clearsign -o InRelease Release
- gpg --yes --armor --detach-sig -o Release.gpg Release

4) Publish
- Commit updated `dists/plucky/*` and push to the hosting branch that backs GitHub Pages.

5) Validate
- From a client using `plucky`:
  - sudo apt update  # should succeed without errors
  - apt policy ak    # should list 4.x versions
  - sudo apt install ak

Notes
- `stable` updates are already automated in cmake/apt_publish.cmake and GitHub workflows; ensure `plucky` gets similar automation if you intend to keep it supported.
- If `plucky` is deprecated, prefer documenting `stable` usage and/or providing a transitional `Release` that clearly indicates deprecation.

Appendix: i386 notice
- If a client system is multi-arch, apt may warn: repo doesn’t support architecture i386.
- Adding `arch=amd64` in the source entry suppresses that harmless warning:
  deb [arch=amd64 signed-by=/usr/share/keyrings/ak-archive-keyring.gpg] https://apertacodex.github.io/ak/ak-apt-repo/ stable main
