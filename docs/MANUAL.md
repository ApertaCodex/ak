# AK Manual

## NAME

**ak** - secure API key manager for developers

## SYNOPSIS

**ak** [OPTION]... [COMMAND] [ARGS]...

## DESCRIPTION

**ak** is a cross-platform command-line tool for managing API keys securely across different services. It provides encrypted storage, service integration, and comprehensive testing capabilities for developers working with multiple APIs.

**ak** stores API keys in an encrypted local vault using industry-standard cryptographic algorithms, ensuring your sensitive credentials remain secure while providing easy access when needed.

## COMMANDS

### Key Management

**ak add** *name* *key*
: Add a new API key with the specified name. The key will be encrypted and stored securely in the local vault.

**ak get** *name*
: Retrieve and display the API key for the specified name. Use with caution as this will display the key in plain text.

**ak get** *name* **--copy**
: Retrieve the API key and copy it directly to the system clipboard without displaying it on screen.

**ak list**
: List all stored API key names without displaying the actual keys.

**ak remove** *name*
: Remove the specified API key from the vault permanently. This action cannot be undone.

**ak update** *name* *key*
: Update an existing API key with a new value.

### Service Testing

**ak test** *service*
: Test the API connection for a specific service. Supported services include openai, github, aws, and others.

**ak test --all**
: Test all configured API keys against their respective services.

**ak test --list**
: List all available service test configurations.

### Shell Integration

**ak completion bash**
: Generate bash completion script for the ak command.

**ak completion zsh**
: Generate zsh completion script for the ak command.

**ak completion fish**
: Generate fish shell completion script for the ak command.

### Vault Management

**ak vault init**
: Initialize a new encrypted vault. This is automatically done on first use.

**ak vault status**
: Display information about the current vault, including location and encryption status.

**ak vault backup** *path*
: Create a backup of the encrypted vault to the specified path.

**ak vault restore** *path*
: Restore vault from a backup file. This will overwrite the current vault.

**ak vault export** *path*
: Export all keys to a JSON file (encrypted). Useful for migration or backup.

**ak vault import** *path*
: Import keys from an exported JSON file.

### Configuration

**ak config set** *key* *value*
: Set a configuration option. Common options include vault_path, default_service, and timeout.

**ak config get** *key*
: Get the current value of a configuration option.

**ak config list**
: List all current configuration options and their values.

**ak config reset**
: Reset all configuration to default values.

## OPTIONS

**-h, --help**
: Display help information and exit.

**-v, --version**
: Display version information and exit.

**--verbose**
: Enable verbose output for debugging.

**--quiet**
: Suppress all non-essential output.

**--vault-path** *PATH*
: Specify a custom path for the vault file. Overrides the default or configured path.

**--timeout** *SECONDS*
: Set timeout for API test operations (default: 30 seconds).

**--no-color**
: Disable colored output.

**--format** *FORMAT*
: Output format for list and status commands. Options: table, json, yaml (default: table).

## SUPPORTED SERVICES

**ak** includes built-in support for testing the following services:

**openai**
: OpenAI API - Tests authentication and basic API access

**github**
: GitHub API - Validates token and user access

**aws**
: Amazon Web Services - Tests AWS credentials and permissions

**google**
: Google Cloud Platform - Validates service account or OAuth credentials

**stripe**
: Stripe Payment API - Tests API key validity

**sendgrid**
: SendGrid Email API - Validates API key and account status

**twilio**
: Twilio Communication API - Tests account SID and auth token

**slack**
: Slack API - Validates bot or user tokens

**discord**
: Discord Bot API - Tests bot token validity

**heroku**
: Heroku Platform API - Validates API key and app access

## CONFIGURATION FILES

**~/.config/ak/config.yaml**
: Main configuration file containing user preferences and default settings.

**~/.config/ak/vault.enc**
: Encrypted vault file containing all stored API keys.

**~/.config/ak/services.yaml**
: Custom service definitions for testing additional APIs.

## ENVIRONMENT VARIABLES

**AK_VAULT_PATH**
: Override the default vault file location.

**AK_CONFIG_PATH**
: Override the default configuration directory.

**AK_NO_COLOR**
: Disable colored output (equivalent to --no-color).

**AK_VERBOSE**
: Enable verbose output (equivalent to --verbose).

**AK_TIMEOUT**
: Default timeout for API operations in seconds.

## EXAMPLES

Add an OpenAI API key:
```bash
ak add openai sk-1234567890abcdef1234567890abcdef
```

Test GitHub API connection:
```bash
ak test github
```

List all stored keys:
```bash
ak list
```

Get a key and copy to clipboard:
```bash
ak get openai --copy
```

Install bash completions:
```bash
ak completion bash | sudo tee /etc/bash_completion.d/ak
```

Backup your vault:
```bash
ak vault backup ~/ak-backup.enc
```

Test all configured services:
```bash
ak test --all
```

## SECURITY CONSIDERATIONS

**Encryption**
: All API keys are encrypted using AES-256-GCM with a key derived from a master password using PBKDF2.

**Storage**
: The vault file is stored with restricted permissions (600) to prevent unauthorized access.

**Memory**
: Keys are cleared from memory immediately after use to minimize exposure time.

**Network**
: All service tests use HTTPS connections. No keys are transmitted in plain text.

**Backup**
: Always backup your vault file and store it securely. Lost master passwords cannot be recovered.

## FILES

**~/.config/ak/**
: Default configuration directory

**~/.config/ak/vault.enc**
: Encrypted key storage

**~/.config/ak/config.yaml**
: Configuration file

**/etc/ak/**
: System-wide configuration directory

**/usr/share/ak/**
: Shared application data

## DIAGNOSTICS

| Exit Status | Meaning |
|-------------|---------|
| 0 | Success |
| 1 | General error |
| 2 | Invalid command or arguments |
| 3 | Vault access error (permissions, corruption, wrong password) |
| 4 | Network error during service testing |
| 5 | Service authentication failed |
| 6 | Configuration error |

## BUGS

Report bugs at: https://github.com/apertacodex/ak/issues

Please include the following information in bug reports:

- Operating system and version
- AK version (`ak --version`)
- Command that triggered the error
- Error message (use `--verbose` for detailed output)
- Steps to reproduce the issue

## AUTHOR

AK development team

## COPYRIGHT

Copyright (C) 2024 apertacodex. This software is licensed under the terms of the End-User License Agreement (EULA). See the EULA.txt file for complete license terms. There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

## SEE ALSO

- **gpg**(1)
- **pass**(1)
- **keyring**(1)
- **ssh-agent**(1)

For complete documentation, visit: https://github.com/apertacodex/ak

## VERSION

This manual page corresponds to AK version 4.0.4.
