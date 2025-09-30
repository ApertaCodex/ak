#!/bin/bash

# AK Bash Completion
# This file provides bash completion for the ak command-line tool

_ak_completion() {
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    
    # Main commands (from main.cpp)
    local main_commands="welcome help version backend secret profile service add set get ls rm search cp purge save load unload profiles env duplicate export import migrate run guard test doctor audit install-shell uninstall completion serve gui"
    
    # Secret subcommands
    local secret_subcommands="add set get ls rm search cp"
    
    # Profile subcommands
    local profile_subcommands="create list show activate delete export import"
    
    # Service subcommands
    local service_subcommands="list add remove test configure"
    
    # Global options
    local global_opts="--help --version --json"

    # Helper function to get available keys/variables
    _get_keys() {
        if command -v ak >/dev/null 2>&1; then
            ak secret ls --json 2>/dev/null | grep -o '"name":"[^"]*"' | cut -d'"' -f4 2>/dev/null || ak secret ls 2>/dev/null | awk '{print $1}' | grep -v "Available\|Total\|📂" | grep -v "^$"
        fi
    }
    
    # Helper function to get available profiles
    _get_profiles() {
        if command -v ak >/dev/null 2>&1; then
            ak profiles 2>/dev/null | grep -v "Available profiles:" | grep -v "^$" | sed 's/^[[:space:]]*//'
        fi
    }

    # Complete based on position and previous words
    case "${COMP_CWORD}" in
        1)
            # First argument - main commands
            COMPREPLY=( $(compgen -W "${main_commands} ${global_opts}" -- ${cur}) )
            return 0
            ;;
        2)
            # Second argument - subcommands or arguments
            case "${COMP_WORDS[1]}" in
                secret)
                    COMPREPLY=( $(compgen -W "${secret_subcommands}" -- ${cur}) )
                    return 0
                    ;;
                profile)
                    COMPREPLY=( $(compgen -W "${profile_subcommands}" -- ${cur}) )
                    return 0
                    ;;
                service)
                    COMPREPLY=( $(compgen -W "${service_subcommands}" -- ${cur}) )
                    return 0
                    ;;
                get|cp|rm)
                    # Commands that take key names
                    COMPREPLY=( $(compgen -W "$(_get_keys)" -- ${cur}) )
                    return 0
                    ;;
                load)
                    # Load can take either profiles or individual keys
                    local profiles keys
                    profiles=$(_get_profiles)
                    keys=$(_get_keys)
                    COMPREPLY=( $(compgen -W "${profiles} ${keys}" -- ${cur}) )
                    return 0
                    ;;
                save)
                    # Save takes profile name, then optionally key names
                    COMPREPLY=( $(compgen -W "$(_get_profiles)" -- ${cur}) )
                    return 0
                    ;;
                unload|duplicate)
                    # Commands that take profile names
                    COMPREPLY=( $(compgen -W "$(_get_profiles)" -- ${cur}) )
                    return 0
                    ;;
                set|add)
                    # These can complete with existing keys or allow new ones
                    COMPREPLY=( $(compgen -W "$(_get_keys)" -- ${cur}) )
                    return 0
                    ;;
                test)
                    # Test can take service names or profile names with -p flag
                    local services="openai anthropic google github gitlab bitbucket azure aws gcp digitalocean linode vultr hetzner"
                    local profiles keys
                    profiles=$(_get_profiles)
                    keys=$(_get_keys)
                    COMPREPLY=( $(compgen -W "${services} ${profiles} ${keys} -p --profile --fail-fast --json --quiet" -- ${cur}) )
                    return 0
                    ;;
            esac
            ;;
        3)
            # Third argument
            case "${COMP_WORDS[1]}" in
                secret)
                    case "${COMP_WORDS[2]}" in
                        get|cp|rm|set)
                            COMPREPLY=( $(compgen -W "$(_get_keys)" -- ${cur}) )
                            return 0
                            ;;
                        add)
                            # For add, third argument could be a key name
                            COMPREPLY=( $(compgen -W "$(_get_keys)" -- ${cur}) )
                            return 0
                            ;;
                    esac
                    ;;
                profile)
                    case "${COMP_WORDS[2]}" in
                        show|activate|delete|export)
                            COMPREPLY=( $(compgen -W "$(_get_profiles)" -- ${cur}) )
                            return 0
                            ;;
                    esac
                    ;;
                save)
                    # Third argument for save can be key names
                    COMPREPLY=( $(compgen -W "$(_get_keys)" -- ${cur}) )
                    return 0
                    ;;
                load)
                    # Third argument for load could be --persist
                    COMPREPLY=( $(compgen -W "--persist" -- ${cur}) )
                    return 0
                    ;;
                cp)
                    # Third argument for cp could be --clear
                    COMPREPLY=( $(compgen -W "--clear" -- ${cur}) )
                    return 0
                    ;;
                test)
                    # Handle test with -p flag
                    if [[ "${COMP_WORDS[2]}" == "-p" || "${COMP_WORDS[2]}" == "--profile" ]]; then
                        COMPREPLY=( $(compgen -W "$(_get_profiles)" -- ${cur}) )
                        return 0
                    fi
                    ;;
            esac
            ;;
        *)
            # Fourth argument and beyond
            case "${COMP_WORDS[1]}" in
                save)
                    # Additional key names for save command
                    COMPREPLY=( $(compgen -W "$(_get_keys)" -- ${cur}) )
                    return 0
                    ;;
                secret)
                    case "${COMP_WORDS[2]}" in
                        cp)
                            # Handle --clear flag for secret cp
                            if [[ "${prev}" == "--clear" ]]; then
                                COMPREPLY=( $(compgen -W "10s 20s 30s 60s 120s" -- ${cur}) )
                                return 0
                            fi
                            ;;
                    esac
                    ;;
                cp)
                    # Handle --clear flag for cp
                    if [[ "${prev}" == "--clear" ]]; then
                        COMPREPLY=( $(compgen -W "10s 20s 30s 60s 120s" -- ${cur}) )
                        return 0
                    fi
                    ;;
            esac
            ;;
    esac

    # Default completion
    COMPREPLY=( $(compgen -W "${global_opts}" -- ${cur}) )
    return 0
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
                    profiles=$(ak profiles 2>/dev/null | grep -v "Available profiles:" | grep -v "^$" | sed 's/^[[:space:]]*//')
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