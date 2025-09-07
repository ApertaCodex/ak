#!/bin/bash

# AK Bash Completion
# This file provides bash completion for the ak command-line tool

_ak_completion() {
    local cur prev opts subcommands
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    
    # Main commands
    subcommands="config key profile service vault help version"
    
    # Global options
    global_opts="--help --version --verbose --quiet --config --profile"
    
    # Config subcommands
    config_commands="init list show set get reset doctor"
    
    # Key subcommands  
    key_commands="generate import export list show delete sign verify encrypt decrypt"
    
    # Profile subcommands
    profile_commands="create list show activate delete export import"
    
    # Service subcommands
    service_commands="list add remove test configure"
    
    # Vault subcommands
    vault_commands="create open close lock unlock status backup restore"

    # Complete based on previous word
    case "${prev}" in
        ak)
            COMPREPLY=( $(compgen -W "${subcommands} ${global_opts}" -- ${cur}) )
            return 0
            ;;
        config)
            COMPREPLY=( $(compgen -W "${config_commands}" -- ${cur}) )
            return 0
            ;;
        key)
            COMPREPLY=( $(compgen -W "${key_commands}" -- ${cur}) )
            return 0
            ;;
        profile)
            COMPREPLY=( $(compgen -W "${profile_commands}" -- ${cur}) )
            return 0
            ;;
        service)
            COMPREPLY=( $(compgen -W "${service_commands}" -- ${cur}) )
            return 0
            ;;
        vault)
            COMPREPLY=( $(compgen -W "${vault_commands}" -- ${cur}) )
            return 0
            ;;
        --config)
            # Complete with config file paths
            COMPREPLY=( $(compgen -f -X '!*.conf' -- ${cur}) )
            return 0
            ;;
        --profile)
            # Complete with available profiles (if ak is available)
            if command -v ak >/dev/null 2>&1; then
                local profiles
                profiles=$(ak profile list --names-only 2>/dev/null)
                COMPREPLY=( $(compgen -W "${profiles}" -- ${cur}) )
            fi
            return 0
            ;;
        generate)
            local key_types="rsa ecdsa ed25519 aes"
            COMPREPLY=( $(compgen -W "${key_types} --name --type --size --output" -- ${cur}) )
            return 0
            ;;
        --type)
            local types="rsa ecdsa ed25519 aes"
            COMPREPLY=( $(compgen -W "${types}" -- ${cur}) )
            return 0
            ;;
        --size)
            local sizes="2048 3072 4096 256 384 521"
            COMPREPLY=( $(compgen -W "${sizes}" -- ${cur}) )
            return 0
            ;;
        import|export)
            # Complete with file paths
            COMPREPLY=( $(compgen -f -- ${cur}) )
            return 0
            ;;
        *)
            # Complete with global options if no match
            COMPREPLY=( $(compgen -W "${global_opts}" -- ${cur}) )
            return 0
            ;;
    esac
    
    # Handle multi-level completions
    case "${COMP_WORDS[1]}" in
        config)
            case "${COMP_WORDS[2]}" in
                set|get)
                    # Config keys
                    local config_keys="storage.backend crypto.algorithm security.timeout ui.theme"
                    COMPREPLY=( $(compgen -W "${config_keys}" -- ${cur}) )
                    ;;
            esac
            ;;
        key)
            case "${COMP_WORDS[2]}" in
                show|delete|export)
                    # Complete with key names if available
                    if command -v ak >/dev/null 2>&1; then
                        local keys
                        keys=$(ak key list --names-only 2>/dev/null)
                        COMPREPLY=( $(compgen -W "${keys}" -- ${cur}) )
                    fi
                    ;;
                sign|verify|encrypt|decrypt)
                    # Complete with files and keys
                    COMPREPLY=( $(compgen -f -- ${cur}) )
                    ;;
            esac
            ;;
    esac
}

# Register the completion function
complete -F _ak_completion ak

# Additional completions for ak-gui if present
if command -v ak-gui >/dev/null 2>&1; then
    _ak_gui_completion() {
        local cur prev opts
        COMPREPLY=()
        cur="${COMP_WORDS[COMP_CWORD]}"
        prev="${COMP_WORDS[COMP_CWORD-1]}"
        
        opts="--help --version --profile --config --theme --debug"
        
        case "${prev}" in
            --profile)
                if command -v ak >/dev/null 2>&1; then
                    local profiles
                    profiles=$(ak profile list --names-only 2>/dev/null)
                    COMPREPLY=( $(compgen -W "${profiles}" -- ${cur}) )
                fi
                return 0
                ;;
            --config)
                COMPREPLY=( $(compgen -f -X '!*.conf' -- ${cur}) )
                return 0
                ;;
            --theme)
                local themes="light dark auto"
                COMPREPLY=( $(compgen -W "${themes}" -- ${cur}) )
                return 0
                ;;
            *)
                COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
                return 0
                ;;
        esac
    }
    
    complete -F _ak_gui_completion ak-gui
fi