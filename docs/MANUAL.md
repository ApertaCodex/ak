# AK Manual

## NAME
**ak** — Secure API Key Manager (CLI)

## SYNOPSIS
- `ak [--json] <command> [options] [arguments]`
- `ak help`
- `ak completion {bash|zsh|fish}`
- `ak serve [--host HOST] [--port PORT]`

## DESCRIPTION
**ak** is a cross‑platform command‑line tool for managing API keys securely across services. It provides:
- Encrypted storage and profile‑scoped key management
- Service definitions with connectivity testing
- Shell integration and completions (bash/zsh/fish)
- Lightweight HTTP server for the web interface

## COMMANDS

### Secret Management
- `ak add <NAME> <VALUE>`  
  Add a secret with value directly (also supports `NAME=VALUE`). Use `-p|--profile` to add to a profile.

- `ak set <NAME>`  
  Prompt to set a secret value interactively.

- `ak get <NAME> [--full]`  
  Get a secret value (`--full` shows unmasked).

- `ak ls [--json]`  
  List secret names (masked values in non‑JSON mode).

- `ak rm <NAME>`  
  Remove a secret from the vault.

- `ak rm --profile <NAME>`  
  Remove an entire profile.

- `ak search <PATTERN>`  
  Search for secrets by name (case‑insensitive).

- `ak cp <NAME>`  
  Copy secret value to clipboard (pbcopy/wl‑copy/xclip).

- `ak purge [--no-backup]`  
  Remove all secrets and profiles (creates backup by default unless `--no-backup`).

### Profile Management
- `ak save <PROFILE> [NAMES...]`  
  Save named secrets to a profile (all secrets if none given).

- `ak load <PROFILE|KEY> [--persist]`  
  Print export statements to load env vars; `--persist` remembers the profile for the current directory.

- `ak unload [<PROFILE> ...] [--persist]`  
  Print unset statements for profile keys; with `--persist` remove remembered profile(s) for the current directory.

- `ak profiles`  
  List available profiles with key counts.

- `ak duplicate <SOURCE> <DEST>`  
  Duplicate a profile.

- `ak env --profile|-p <NAME>`  
  Show profile as export statements.

### Export / Import
- `ak export --profile|-p <PROFILE> --format|-f <FORMAT> --output|-o <FILE>`  
  Export profile to file. Formats: `env`, `dotenv`, `json`, `yaml`.

- `ak import --profile|-p <PROFILE> --format|-f <FORMAT> --file|-i <FILE> [--keys]`  
  Import secrets from file to profile. `--keys` imports only known service provider keys.

### Service Management
- `ak service add`  
  Create a new custom API service (interactive).

- `ak service list`  
  List built‑in and custom services.

- `ak service edit <NAME>`  
  Edit an existing custom service (interactive).

- `ak service delete <NAME>`  
  Delete a custom service.

### Testing and Utilities
- `ak run --profile|-p <PROFILE> -- <CMD...>`  
  Run command with profile environment loaded.

- `ak test [<SERVICE> ... | --all] [--json] [--fail-fast] [--profile|-p <NAME>]`  
  Test connectivity for configured providers or specific services/keys.

- `ak guard enable|disable`  
  Enable or disable shell guard for secret protection.

- `ak doctor`  
  Show system configuration/dependencies summary.

- `ak audit [N]`  
  Show audit log (last N entries, default 10).

### System
- `ak help`  
  Show detailed help.

- `ak version`  
  Show version information.

- `ak backend`  
  Show backend info (`gpg` or `plain`).

- `ak serve [--host HOST] [--port PORT]`  
  Start HTTP server for the web interface (serves `/web-app.html` and related assets).

- `ak install-shell`  
  Install shell integration for auto‑loading.

- `ak uninstall`  
  Remove shell integration.

- `ak completion <bash|zsh|fish>`  
  Generate completion script for the specified shell.

## OPTIONS (Global)
- `-h, --help` — Show help and exit  
- `-v, --version` — Show version and exit  
- `--json` — Enable JSON output for supported commands

## ENVIRONMENT
- `AK_DISABLE_GPG` — If set, forces plain storage even if `gpg` is available  
- `AK_PASSPHRASE` — Preset passphrase for `gpg` operations (non‑interactive)

## FILES
- `~/.config/ak/` — Default configuration directory  
- `~/.config/ak/keys.env` — Unencrypted vault (when GPG unavailable/disabled)  
- `~/.config/ak/keys.env.gpg` — Encrypted vault (when GPG available)  
- `~/.config/ak/profiles/` — Profile definitions  
- `~/.config/ak/persist/` — Directory‑profile persistence metadata  
- `~/.config/ak/audit.log` — Audit log file

## EXAMPLES

### Add / Set / Get / List / Remove
```bash
ak add API_KEY "sk-..."
ak set API_KEY
ak get API_KEY --full
ak ls
ak rm API_KEY
```

### Profiles
```bash
ak save dev API_KEY DB_URL
ak load dev --persist
eval "$(ak load dev)"
ak profiles
```

### Export / Import
```bash
ak export -p dev -f dotenv -o dev.env
ak import -p dev -f env -i .env --keys
```

### Service testing
```bash
ak test --all
ak test openai --fail-fast
```

### Completions
```bash
ak completion bash | sudo tee /etc/bash_completion.d/ak
ak completion zsh | sudo tee /usr/share/zsh/site-functions/_ak
ak completion fish > ~/.config/fish/completions/ak.fish
```

### HTTP server
```bash
ak serve --host 127.0.0.1 --port 8027
# Open http://127.0.0.1:8027/web-app.html in a browser
```

## DIAGNOSTICS
Exit status is 0 on success, non‑zero on error.

## BUGS
Report issues at: https://github.com/apertacodex/ak/issues

## COPYRIGHT
Copyright (C) 2024–2025 ApertaCodex.
License: MIT with Attribution (see LICENSE).

## SEE ALSO
`gpg(1)`, `pass(1)`

## VERSION
This manual corresponds to AK version 4.4.0.
