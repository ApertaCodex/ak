# AK Fish Completion
# This file provides fish completion for the ak command-line tool

# Main command completions
complete -c ak -f

# Global options
complete -c ak -s h -l help -d "Show help information"
complete -c ak -s v -l version -d "Show version information"
complete -c ak -l json -d "Output in JSON format"

# Main commands (from main.cpp command map)
complete -c ak -n "__fish_use_subcommand" -x -a "welcome" -d "Show welcome message"
complete -c ak -n "__fish_use_subcommand" -x -a "help" -d "Show help information"
complete -c ak -n "__fish_use_subcommand" -x -a "version" -d "Show version information"
complete -c ak -n "__fish_use_subcommand" -x -a "backend" -d "Backend information"

# Namespaced commands (canonical)
complete -c ak -n "__fish_use_subcommand" -x -a "secret" -d "Secret/API key management"
complete -c ak -n "__fish_use_subcommand" -x -a "profile" -d "Profile management"
complete -c ak -n "__fish_use_subcommand" -x -a "service" -d "Service configuration and testing"

# Legacy/Alias commands (backward compatibility)
complete -c ak -n "__fish_use_subcommand" -x -a "add" -d "Add a new secret"
complete -c ak -n "__fish_use_subcommand" -x -a "set" -d "Set a secret value"
complete -c ak -n "__fish_use_subcommand" -x -a "get" -d "Get a secret value"
complete -c ak -n "__fish_use_subcommand" -x -a "ls" -d "List secrets"
complete -c ak -n "__fish_use_subcommand" -x -a "rm" -d "Remove a secret"
complete -c ak -n "__fish_use_subcommand" -x -a "search" -d "Search secrets"
complete -c ak -n "__fish_use_subcommand" -x -a "cp" -d "Copy secret to clipboard"
complete -c ak -n "__fish_use_subcommand" -x -a "purge" -d "Purge all secrets"

# Profile management (legacy aliases)
complete -c ak -n "__fish_use_subcommand" -x -a "save" -d "Save current profile"
complete -c ak -n "__fish_use_subcommand" -x -a "load" -d "Load profile or individual key"
complete -c ak -n "__fish_use_subcommand" -x -a "unload" -d "Unload profile"
complete -c ak -n "__fish_use_subcommand" -x -a "profiles" -d "List available profiles"
complete -c ak -n "__fish_use_subcommand" -x -a "env" -d "Show environment variables"
complete -c ak -n "__fish_use_subcommand" -x -a "duplicate" -d "Duplicate a profile"

# Export/Import
complete -c ak -n "__fish_use_subcommand" -x -a "export" -d "Export data"
complete -c ak -n "__fish_use_subcommand" -x -a "import" -d "Import data"
complete -c ak -n "__fish_use_subcommand" -x -a "migrate" -d "Migrate data"

# Utilities
complete -c ak -n "__fish_use_subcommand" -x -a "run" -d "Run command with environment"
complete -c ak -n "__fish_use_subcommand" -x -a "guard" -d "Guard command execution"
complete -c ak -n "__fish_use_subcommand" -x -a "test" -d "Test API connections"
complete -c ak -n "__fish_use_subcommand" -x -a "doctor" -d "Diagnose configuration issues"
complete -c ak -n "__fish_use_subcommand" -x -a "audit" -d "View audit log"

# System
complete -c ak -n "__fish_use_subcommand" -x -a "install-shell" -d "Install shell integration"
complete -c ak -n "__fish_use_subcommand" -x -a "uninstall" -d "Uninstall ak"
complete -c ak -n "__fish_use_subcommand" -x -a "completion" -d "Generate completion scripts"
complete -c ak -n "__fish_use_subcommand" -x -a "serve" -d "Start HTTP server"
complete -c ak -n "__fish_use_subcommand" -x -a "gui" -d "Launch GUI interface"

# Secret subcommands
complete -c ak -n "__fish_seen_subcommand_from secret" -x -a "add" -d "Add a new secret"
complete -c ak -n "__fish_seen_subcommand_from secret" -x -a "set" -d "Set a secret value"
complete -c ak -n "__fish_seen_subcommand_from secret" -x -a "get" -d "Get a secret value"
complete -c ak -n "__fish_seen_subcommand_from secret" -x -a "ls" -d "List all secrets"
complete -c ak -n "__fish_seen_subcommand_from secret" -x -a "rm" -d "Remove a secret"
complete -c ak -n "__fish_seen_subcommand_from secret" -x -a "search" -d "Search secrets by name pattern"
complete -c ak -n "__fish_seen_subcommand_from secret" -x -a "cp" -d "Copy secret to clipboard"

# Secret operations that need key names
complete -c ak -n "__fish_seen_subcommand_from secret; and __fish_seen_subcommand_from get rm cp set" -x -a "(__ak_keys)"

# Direct commands that need key names
complete -c ak -n "__fish_seen_subcommand_from get rm cp set add" -x -a "(__ak_keys)"

# Load command can take profiles or keys
complete -c ak -n "__fish_seen_subcommand_from load" -x -a "(__ak_profiles_and_keys)"

# Profile commands that need profile names
complete -c ak -n "__fish_seen_subcommand_from save unload duplicate" -x -a "(__ak_profiles)"

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

# Service subcommands
complete -c ak -n "__fish_seen_subcommand_from service" -x -a "list" -d "List available services"
complete -c ak -n "__fish_seen_subcommand_from service" -x -a "add" -d "Add new service"
complete -c ak -n "__fish_seen_subcommand_from service" -x -a "remove" -d "Remove service"
complete -c ak -n "__fish_seen_subcommand_from service" -x -a "test" -d "Test service connection"
complete -c ak -n "__fish_seen_subcommand_from service" -x -a "configure" -d "Configure service"

# Test command completions
complete -c ak -n "__fish_seen_subcommand_from test" -l fail-fast -d "Stop on first failure"
complete -c ak -n "__fish_seen_subcommand_from test" -l json -d "Output in JSON format"
complete -c ak -n "__fish_seen_subcommand_from test" -l quiet -d "Suppress output"
complete -c ak -n "__fish_seen_subcommand_from test" -s p -l profile -d "Specify profile" -x -a "(__ak_profiles)"
complete -c ak -n "__fish_seen_subcommand_from test" -x -a "(__ak_test_services)"
complete -c ak -n "__fish_seen_subcommand_from test" -x -a "(__ak_profiles)"
complete -c ak -n "__fish_seen_subcommand_from test" -x -a "(__ak_keys)"

# CP command --clear flag
complete -c ak -n "__fish_seen_subcommand_from cp" -l clear -d "Auto-clear clipboard" -x -a "10s 20s 30s 60s 120s"
complete -c ak -n "__fish_seen_subcommand_from secret; and __fish_seen_subcommand_from cp" -l clear -d "Auto-clear clipboard" -x -a "10s 20s 30s 60s 120s"

# Load command --persist flag
complete -c ak -n "__fish_seen_subcommand_from load" -l persist -d "Persist profile for directory"

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
        ak profiles 2>/dev/null | grep -v "Available profiles:" | grep -v "^\$" | sed 's/^[[:space:]]*//'
    end
end

function __ak_keys
    if command -v ak >/dev/null 2>&1
        # Try JSON output first, fall back to plain text
        ak secret ls --json 2>/dev/null | grep -o '"name":"[^"]*"' | cut -d'"' -f4 2>/dev/null; or ak secret ls 2>/dev/null | awk '{print $1}' | grep -v "Available\|Total\|📂" | grep -v "^\$"
    end
end

function __ak_profiles_and_keys
    __ak_profiles
    __ak_keys
end

function __ak_test_services
    echo "openai"
    echo "anthropic"
    echo "google"
    echo "github"
    echo "gitlab"
    echo "bitbucket"
    echo "azure"
    echo "aws"
    echo "gcp"
    echo "digitalocean"
    echo "linode"
    echo "vultr"
    echo "hetzner"
end