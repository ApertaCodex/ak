# AK Fish Completion
# This file provides fish completion for the ak command-line tool

# Main command completions
complete -c ak -f

# Global options
complete -c ak -s h -l help -d "Show help information"
complete -c ak -s v -l version -d "Show version information"
complete -c ak -l verbose -d "Enable verbose output"
complete -c ak -l quiet -d "Suppress non-error output"
complete -c ak -l config -d "Specify config file" -r -F
complete -c ak -l profile -d "Specify profile" -x -a "(__ak_profiles)"

# Main commands
complete -c ak -n "__fish_use_subcommand" -x -a "config" -d "Manage configuration settings"
complete -c ak -n "__fish_use_subcommand" -x -a "key" -d "Key management operations"
complete -c ak -n "__fish_use_subcommand" -x -a "profile" -d "Profile management"
complete -c ak -n "__fish_use_subcommand" -x -a "service" -d "Service configuration"
complete -c ak -n "__fish_use_subcommand" -x -a "vault" -d "Vault operations"
complete -c ak -n "__fish_use_subcommand" -x -a "help" -d "Show help information"
complete -c ak -n "__fish_use_subcommand" -x -a "version" -d "Show version information"

# Config subcommands
complete -c ak -n "__fish_seen_subcommand_from config" -x -a "init" -d "Initialize configuration"
complete -c ak -n "__fish_seen_subcommand_from config" -x -a "list" -d "List all configuration values"
complete -c ak -n "__fish_seen_subcommand_from config" -x -a "show" -d "Show current configuration"
complete -c ak -n "__fish_seen_subcommand_from config" -x -a "set" -d "Set configuration value"
complete -c ak -n "__fish_seen_subcommand_from config" -x -a "get" -d "Get configuration value"
complete -c ak -n "__fish_seen_subcommand_from config" -x -a "reset" -d "Reset configuration to defaults"
complete -c ak -n "__fish_seen_subcommand_from config" -x -a "doctor" -d "Diagnose configuration issues"

# Config keys for set/get commands
complete -c ak -n "__fish_seen_subcommand_from config; and __fish_seen_subcommand_from set get" -x -a "storage.backend" -d "Storage backend type"
complete -c ak -n "__fish_seen_subcommand_from config; and __fish_seen_subcommand_from set get" -x -a "storage.path" -d "Storage path"
complete -c ak -n "__fish_seen_subcommand_from config; and __fish_seen_subcommand_from set get" -x -a "crypto.algorithm" -d "Default crypto algorithm"
complete -c ak -n "__fish_seen_subcommand_from config; and __fish_seen_subcommand_from set get" -x -a "crypto.key_size" -d "Default key size"
complete -c ak -n "__fish_seen_subcommand_from config; and __fish_seen_subcommand_from set get" -x -a "security.timeout" -d "Security timeout"
complete -c ak -n "__fish_seen_subcommand_from config; and __fish_seen_subcommand_from set get" -x -a "security.auto_lock" -d "Auto lock enabled"
complete -c ak -n "__fish_seen_subcommand_from config; and __fish_seen_subcommand_from set get" -x -a "ui.theme" -d "UI theme"
complete -c ak -n "__fish_seen_subcommand_from config; and __fish_seen_subcommand_from set get" -x -a "ui.language" -d "UI language"
complete -c ak -n "__fish_seen_subcommand_from config; and __fish_seen_subcommand_from set get" -x -a "logging.level" -d "Logging level"
complete -c ak -n "__fish_seen_subcommand_from config; and __fish_seen_subcommand_from set get" -x -a "logging.file" -d "Log file path"

# Key subcommands
complete -c ak -n "__fish_seen_subcommand_from key" -x -a "generate" -d "Generate a new key"
complete -c ak -n "__fish_seen_subcommand_from key" -x -a "import" -d "Import key from file"
complete -c ak -n "__fish_seen_subcommand_from key" -x -a "export" -d "Export key to file"
complete -c ak -n "__fish_seen_subcommand_from key" -x -a "list" -d "List all keys"
complete -c ak -n "__fish_seen_subcommand_from key" -x -a "show" -d "Show key details"
complete -c ak -n "__fish_seen_subcommand_from key" -x -a "delete" -d "Delete a key"
complete -c ak -n "__fish_seen_subcommand_from key" -x -a "sign" -d "Sign data with key"
complete -c ak -n "__fish_seen_subcommand_from key" -x -a "verify" -d "Verify signature"
complete -c ak -n "__fish_seen_subcommand_from key" -x -a "encrypt" -d "Encrypt data"
complete -c ak -n "__fish_seen_subcommand_from key" -x -a "decrypt" -d "Decrypt data"

# Key generate options
complete -c ak -n "__fish_seen_subcommand_from key; and __fish_seen_subcommand_from generate" -l name -d "Key name" -x
complete -c ak -n "__fish_seen_subcommand_from key; and __fish_seen_subcommand_from generate" -l type -d "Key type" -x -a "rsa ecdsa ed25519 aes"
complete -c ak -n "__fish_seen_subcommand_from key; and __fish_seen_subcommand_from generate" -l size -d "Key size" -x -a "2048 3072 4096 256 384 521"
complete -c ak -n "__fish_seen_subcommand_from key; and __fish_seen_subcommand_from generate" -l output -d "Output file" -r -F

# Key operations that need key names
complete -c ak -n "__fish_seen_subcommand_from key; and __fish_seen_subcommand_from show delete export" -x -a "(__ak_keys)"

# Key operations that need files
complete -c ak -n "__fish_seen_subcommand_from key; and __fish_seen_subcommand_from import sign verify encrypt decrypt" -r -F

# Profile subcommands
complete -c ak -n "__fish_seen_subcommand_from profile" -x -a "create" -d "Create new profile"
complete -c ak -n "__fish_seen_subcommand_from profile" -x -a "list" -d "List all profiles"
complete -c ak -n "__fish_seen_subcommand_from profile" -x -a "show" -d "Show profile details"
complete -c ak -n "__fish_seen_subcommand_from profile" -x -a "activate" -d "Activate profile"
complete -c ak -n "__fish_seen_subcommand_from profile" -x -a "delete" -d "Delete profile"
complete -c ak -n "__fish_seen_subcommand_from profile" -x -a "export" -d "Export profile"
complete -c ak -n "__fish_seen_subcommand_from profile" -x -a "import" -d "Import profile"

# Profile operations that need profile names
complete -c ak -n "__fish_seen_subcommand_from profile; and __fish_seen_subcommand_from show activate delete export" -x -a "(__ak_profiles)"

# Profile import needs files
complete -c ak -n "__fish_seen_subcommand_from profile; and __fish_seen_subcommand_from import" -r -F

# Service subcommands
complete -c ak -n "__fish_seen_subcommand_from service" -x -a "list" -d "List available services"
complete -c ak -n "__fish_seen_subcommand_from service" -x -a "add" -d "Add new service"
complete -c ak -n "__fish_seen_subcommand_from service" -x -a "remove" -d "Remove service"
complete -c ak -n "__fish_seen_subcommand_from service" -x -a "test" -d "Test service connection"
complete -c ak -n "__fish_seen_subcommand_from service" -x -a "configure" -d "Configure service"

# Service operations that need service names
complete -c ak -n "__fish_seen_subcommand_from service; and __fish_seen_subcommand_from remove test configure" -x -a "(__ak_services)"

# Vault subcommands
complete -c ak -n "__fish_seen_subcommand_from vault" -x -a "create" -d "Create new vault"
complete -c ak -n "__fish_seen_subcommand_from vault" -x -a "open" -d "Open vault"
complete -c ak -n "__fish_seen_subcommand_from vault" -x -a "close" -d "Close vault"
complete -c ak -n "__fish_seen_subcommand_from vault" -x -a "lock" -d "Lock vault"
complete -c ak -n "__fish_seen_subcommand_from vault" -x -a "unlock" -d "Unlock vault"
complete -c ak -n "__fish_seen_subcommand_from vault" -x -a "status" -d "Show vault status"
complete -c ak -n "__fish_seen_subcommand_from vault" -x -a "backup" -d "Backup vault"
complete -c ak -n "__fish_seen_subcommand_from vault" -x -a "restore" -d "Restore vault"

# Vault operations that need files
complete -c ak -n "__fish_seen_subcommand_from vault; and __fish_seen_subcommand_from backup restore" -r -F

# AK GUI completions
complete -c ak-gui -f
complete -c ak-gui -s h -l help -d "Show help information"
complete -c ak-gui -s v -l version -d "Show version information"
complete -c ak-gui -l profile -d "Specify profile" -x -a "(__ak_profiles)"
complete -c ak-gui -l config -d "Specify config file" -r -F
complete -c ak-gui -l theme -d "UI theme" -x -a "light dark auto"
complete -c ak-gui -l debug -d "Enable debug mode"

# Helper functions
function __ak_profiles
    if command -v ak >/dev/null 2>&1
        ak profile list --names-only 2>/dev/null
    end
end

function __ak_keys
    if command -v ak >/dev/null 2>&1
        ak key list --names-only 2>/dev/null
    end
end

function __ak_services
    if command -v ak >/dev/null 2>&1
        ak service list --names-only 2>/dev/null
    end
end