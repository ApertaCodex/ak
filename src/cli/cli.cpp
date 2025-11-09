#include "cli/cli.hpp"
#include "ui/ui.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

namespace ak {
namespace cli {

// Flag expansion
std::vector<std::string> expandShortFlags(const std::vector<std::string>& args) {
    std::vector<std::string> expanded;
    expanded.reserve(args.size() * 2); // Reserve extra space for expanded flags
    
    for (const auto& arg : args) {
        if (arg.size() >= 2 && arg[0] == '-' && arg[1] != '-') {
            // Short flag like -pf becomes --profile --format
            for (size_t i = 1; i < arg.size(); ++i) {
                char c = arg[i];
                if (c == 'p') {
                    expanded.push_back("--profile");
                } else if (c == 'f') {
                    expanded.push_back("--format");
                } else if (c == 'o') {
                    expanded.push_back("--output");
                } else if (c == 'i') {
                    expanded.push_back("--file");
                } else if (c == 'h') {
                    expanded.push_back("--help");
                } else if (c == 'v') {
                    expanded.push_back("--version");
                } else {
                    // Unknown short flag, keep as is
                    expanded.push_back(std::string("-") + c);
                }
            }
        } else {
            expanded.push_back(arg);
        }
    }
    
    return expanded;
}

// Help system
void showLogo() {
    if (!ui::isColorSupported()) {
        std::cout << "AK - Secret Management CLI\n\n";
        return;
    }
    
    std::cout << ui::colorize(R"(
            ████████╗  ██╗  ██╗
            ██╔═══██║  ██║ ██╔╝
            ██║█████║  ████╔╝
            ██║   ██║  ██╔═██╗
            ██║   ██║  ██║  ██╗
            ╚═╝   ╚═╝  ╚═╝  ╚═╝
)", ui::Colors::BRIGHT_CYAN) << "\n";

    std::cout << ui::colorize("    🔐 ", "") << ui::colorize("Secure Secret Management", ui::Colors::BRIGHT_WHITE + ui::Colors::BOLD) << "\n";
    std::cout << ui::colorize("    ⚡ ", "") << ui::colorize("Fast • Secure • Human-Friendly", ui::Colors::BRIGHT_GREEN) << "\n\n";
}

void showTips() {
    std::cout << ui::colorize("Tips for getting started:", ui::Colors::BRIGHT_YELLOW + ui::Colors::BOLD) << "\n";
    std::cout << ui::colorize("1. ", ui::Colors::BRIGHT_WHITE) << ui::colorize("Set your first secret: ", ui::Colors::WHITE)
             << ui::colorize("ak secret set API_KEY", ui::Colors::BRIGHT_CYAN) << ui::colorize(" (or ", ui::Colors::DIM)
             << ui::colorize("ak set API_KEY", ui::Colors::DIM) << ui::colorize(")", ui::Colors::DIM) << "\n";
    std::cout << ui::colorize("2. ", ui::Colors::BRIGHT_WHITE) << ui::colorize("Create profiles to organize secrets: ", ui::Colors::WHITE)
             << ui::colorize("ak profile save prod API_KEY DB_URL", ui::Colors::BRIGHT_CYAN) << "\n";
    std::cout << ui::colorize("3. ", ui::Colors::BRIGHT_WHITE) << ui::colorize("Load secrets into your shell: ", ui::Colors::WHITE)
             << ui::colorize("ak profile load prod", ui::Colors::BRIGHT_CYAN) << "\n";
    std::cout << ui::colorize("4. ", ui::Colors::BRIGHT_WHITE) << ui::colorize("Import from .env files: ", ui::Colors::WHITE)
             << ui::colorize("ak import -p dev -f env -i .env --keys", ui::Colors::BRIGHT_CYAN) << "\n";
    std::cout << ui::colorize("5. ", ui::Colors::BRIGHT_WHITE) << ui::colorize("Run ", ui::Colors::WHITE)
             << ui::colorize("ak help", ui::Colors::BRIGHT_MAGENTA) << ui::colorize(" for detailed documentation", ui::Colors::WHITE) << "\n\n";
}

void cmd_help() {
    showLogo();
    
    std::cout << ui::colorize("USAGE:", ui::Colors::BRIGHT_WHITE + ui::Colors::BOLD) << "\n";
    std::cout << "  " << ui::colorize("ak", ui::Colors::BRIGHT_CYAN) << ui::colorize(" <command> [options] [arguments]", ui::Colors::WHITE) << "\n\n";

    std::cout << ui::colorize("SECRETS:", ui::Colors::BRIGHT_GREEN + ui::Colors::BOLD) << "\n";
    std::cout << "  " << ui::colorize("ak secret add <NAME> <VALUE>", ui::Colors::BRIGHT_CYAN) << "       Add a secret with value directly\n";
    std::cout << "  " << ui::colorize("ak secret add <NAME=VALUE>", ui::Colors::BRIGHT_CYAN) << "         Add a secret using NAME=VALUE format\n";
    std::cout << "  " << ui::colorize("ak secret set <NAME>", ui::Colors::BRIGHT_CYAN) << "               Set a secret (prompts for value)\n";
    std::cout << "  " << ui::colorize("ak secret get <NAME> [--full|--reveal]", ui::Colors::BRIGHT_CYAN) << " Get a secret value\n";
    std::cout << "  " << ui::colorize("ak secret ls [--json|--quiet]", ui::Colors::BRIGHT_CYAN) << "       List all secret names\n";
    std::cout << "  " << ui::colorize("ak secret rm <NAME>", ui::Colors::BRIGHT_CYAN) << "                Remove a secret\n";
    std::cout << "  " << ui::colorize("ak secret search <PATTERN>", ui::Colors::BRIGHT_CYAN) << "         Search for secrets by name pattern\n";
    std::cout << "  " << ui::colorize("ak secret cp <NAME> [--clear 20s]", ui::Colors::BRIGHT_CYAN) << "   Copy secret to clipboard (auto-clear)\n\n";

    std::cout << ui::colorize("PROFILES:", ui::Colors::BRIGHT_BLUE + ui::Colors::BOLD) << "\n";
    std::cout << "  " << ui::colorize("ak profile save <profile> [NAMES...]", ui::Colors::BRIGHT_CYAN) << "  Save secrets to profile\n";
    std::cout << "  " << ui::colorize("ak profile load <profile> [--persist]", ui::Colors::BRIGHT_CYAN) << " Load profile as environment variables\n";
    std::cout << "  " << ui::colorize("ak profile unload [<profile>] [--persist]", ui::Colors::BRIGHT_CYAN) << " Unload profile environment variables\n";
    std::cout << "  " << ui::colorize("ak profile ls", ui::Colors::BRIGHT_CYAN) << "                       List all available profiles\n";
    std::cout << "  " << ui::colorize("ak profile duplicate <src> <dest>", ui::Colors::BRIGHT_CYAN) << "   Duplicate a profile\n";
    std::cout << "  " << ui::colorize("ak profile rm <name> [--force]", ui::Colors::BRIGHT_CYAN) << "      Remove a profile (requires confirmation)\n\n";

    std::cout << ui::colorize("SERVICES:", ui::Colors::BRIGHT_MAGENTA + ui::Colors::BOLD) << "\n";
    std::cout << "  " << ui::colorize("ak service add", ui::Colors::BRIGHT_CYAN) << "                     Create a new custom API service\n";
    std::cout << "  " << ui::colorize("ak service ls", ui::Colors::BRIGHT_CYAN) << "                      List all services\n";
    std::cout << "  " << ui::colorize("ak service show <name>", ui::Colors::BRIGHT_CYAN) << "            Show service details\n";
    std::cout << "  " << ui::colorize("ak service edit <name>", ui::Colors::BRIGHT_CYAN) << "            Edit an existing service\n";
    std::cout << "  " << ui::colorize("ak service rm <name>", ui::Colors::BRIGHT_CYAN) << "              Delete a custom service\n\n";

    std::cout << ui::colorize("IMPORT/EXPORT:", ui::Colors::BRIGHT_MAGENTA + ui::Colors::BOLD) << "\n";
    std::cout << "  " << ui::colorize("ak export -p <profile> -f <format> -o <file>", ui::Colors::BRIGHT_CYAN) << " Export profile to file\n";
    std::cout << "  " << ui::colorize("ak import -p <profile> -f <format> -i <file>", ui::Colors::BRIGHT_CYAN) << " Import secrets from file\n";
    std::cout << "                                        " << ui::colorize("Formats: env, dotenv, json, yaml", ui::Colors::DIM) << "\n";
    std::cout << "                                        " << ui::colorize("Use --keys to import known service keys only", ui::Colors::DIM) << "\n\n";

    std::cout << ui::colorize("UTILITIES:", ui::Colors::BRIGHT_YELLOW + ui::Colors::BOLD) << "\n";
    std::cout << "  " << ui::colorize("ak run -p <profile> -- <cmd>", ui::Colors::BRIGHT_CYAN) << "       Run command with profile loaded\n";
    std::cout << "  " << ui::colorize("ak test [<service>|--all] [--json] [--quiet]", ui::Colors::BRIGHT_CYAN) << " Test API connectivity\n";
    std::cout << "  " << ui::colorize("ak guard enable|disable|status", ui::Colors::BRIGHT_CYAN) << "    Shell guard for secret protection\n";
    std::cout << "  " << ui::colorize("ak doctor", ui::Colors::BRIGHT_CYAN) << "                         Check system configuration\n";
    std::cout << "  " << ui::colorize("ak audit [N]", ui::Colors::BRIGHT_CYAN) << "                      Show audit log (last N entries)\n";
    std::cout << "  " << ui::colorize("ak env -p <profile>", ui::Colors::BRIGHT_CYAN) << "               Show profile as export statements\n\n";
    
    std::cout << ui::colorize("SYSTEM:", ui::Colors::BRIGHT_RED + ui::Colors::BOLD) << "\n";
    std::cout << "  " << ui::colorize("ak help", ui::Colors::BRIGHT_CYAN) << "                           Show this help message\n";
    std::cout << "  " << ui::colorize("ak version", ui::Colors::BRIGHT_CYAN) << "                        Show version information\n";
    std::cout << "  " << ui::colorize("ak backend", ui::Colors::BRIGHT_CYAN) << "                        Show backend information\n";
    std::cout << "  " << ui::colorize("ak purge [--no-backup] [--force]", ui::Colors::BRIGHT_CYAN) << "      Remove all secrets and profiles\n";
    std::cout << "  " << ui::colorize("ak install-shell", ui::Colors::BRIGHT_CYAN) << "                  Install shell integration\n";
    std::cout << "  " << ui::colorize("ak completion <shell>", ui::Colors::BRIGHT_CYAN) << "             Generate shell completion script\n\n";

    std::cout << ui::colorize("LEGACY ALIASES:", ui::Colors::DIM + ui::Colors::BOLD) << "\n";
    std::cout << "  " << ui::colorize("Legacy commands (ak add, ak get, ak ls, etc.) are still supported", ui::Colors::DIM) << "\n";
    std::cout << "  " << ui::colorize("Use namespaced commands (ak secret, ak profile, ak service) for new scripts", ui::Colors::DIM) << "\n\n";

    showTips();
    
    std::cout << ui::colorize("For detailed documentation, visit: ", ui::Colors::WHITE)
             << ui::colorize("https://github.com/apertacodex/ak", ui::Colors::BRIGHT_BLUE + ui::Colors::BOLD) << "\n";
}

void showWelcome() {
    showLogo();
    showTips();
    std::cout << ui::colorize("Ready to manage your secrets securely! 🚀", ui::Colors::BRIGHT_GREEN + ui::Colors::BOLD) << "\n\n";
}

// Shell completion generation
void generateBashCompletion() {
    std::cout << R"(#!/bin/bash
# Bash completion for ak

_ak_completion() {
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    # Main commands (namespaced + legacy)
    local commands="secret profile service add set get ls rm search cp purge save load unload profiles duplicate env export import migrate run guard test doctor audit install-shell uninstall completion version backend help welcome"

    # Handle multi-level completions
    case "${COMP_WORDS[1]}" in
        secret)
            case "${prev}" in
                secret)
                    COMPREPLY=($(compgen -W "add set get ls rm search cp" -- ${cur}))
                    return 0
                    ;;
                get)
                    local secrets=$(ak ls 2>/dev/null | awk '{print $1}')
                    COMPREPLY=($(compgen -W "${secrets} --full --reveal" -- ${cur}))
                    return 0
                    ;;
                ls)
                    COMPREPLY=($(compgen -W "--json --quiet" -- ${cur}))
                    return 0
                    ;;
                cp)
                    local secrets=$(ak ls 2>/dev/null | awk '{print $1}')
                    COMPREPLY=($(compgen -W "${secrets} --clear" -- ${cur}))
                    return 0
                    ;;
                add|set|rm|search)
                    local secrets=$(ak ls 2>/dev/null | awk '{print $1}')
                    COMPREPLY=($(compgen -W "${secrets}" -- ${cur}))
                    return 0
                    ;;
            esac
            ;;
        profile)
            case "${prev}" in
                profile)
                    COMPREPLY=($(compgen -W "save load unload ls duplicate rm" -- ${cur}))
                    return 0
                    ;;
                load|unload|duplicate|rm)
                    local profiles=$(ak profiles 2>/dev/null | tr '\n' ' ')
                    COMPREPLY=($(compgen -W "${profiles} --persist --force" -- ${cur}))
                    return 0
                    ;;
                save)
                    local profiles=$(ak profiles 2>/dev/null | tr '\n' ' ')
                    local secrets=$(ak ls 2>/dev/null | awk '{print $1}')
                    COMPREPLY=($(compgen -W "${profiles} ${secrets}" -- ${cur}))
                    return 0
                    ;;
            esac
            ;;
        service)
            case "${prev}" in
                service)
                    COMPREPLY=($(compgen -W "add ls show edit rm" -- ${cur}))
                    return 0
                    ;;
                show|edit|rm)
                    local services=$(ak service ls 2>/dev/null | grep -v "Built-in\|Custom\|Available" | awk '{print $1}')
                    COMPREPLY=($(compgen -W "${services}" -- ${cur}))
                    return 0
                    ;;
            esac
            ;;
    esac

    case "${prev}" in
        ak)
            COMPREPLY=($(compgen -W "${commands}" -- ${cur}))
            return 0
            ;;
        --profile|-p)
            # Complete with available profiles
            local profiles=$(ak profiles 2>/dev/null | tr '\n' ' ')
            COMPREPLY=($(compgen -W "${profiles}" -- ${cur}))
            return 0
            ;;
        --format|-f)
            COMPREPLY=($(compgen -W "env dotenv json yaml" -- ${cur}))
            return 0
            ;;
        completion)
            COMPREPLY=($(compgen -W "bash zsh fish" -- ${cur}))
            return 0
            ;;
        guard)
            COMPREPLY=($(compgen -W "enable disable status" -- ${cur}))
            return 0
            ;;
        service)
            COMPREPLY=($(compgen -W "add ls show edit rm" -- ${cur}))
            return 0
            ;;
        test)
            local services="anthropic azure_openai brave cohere deepseek exa fireworks gemini groq huggingface inference langchain continue composio hyperbolic logfire mistral openai openrouter perplexity sambanova tavily together xai"
            COMPREPLY=($(compgen -W "${services} --all --json --fail-fast --quiet" -- ${cur}))
            return 0
            ;;
        purge)
            COMPREPLY=($(compgen -W "--no-backup --force" -- ${cur}))
            return 0
            ;;
    esac

    # Global flags
    local global_flags="--json --quiet --help --version"
    COMPREPLY=($(compgen -W "${global_flags}" -- ${cur}))
}

complete -F _ak_completion ak
)";
}

void generateZshCompletion() {
    std::cout << R"ZSH(#compdef ak
# Zsh completion for ak

_arguments -C \
  '(-h --help)'{-h,--help}'[Show help information]' \
  '(-v --version)'{-v,--version}'[Show version information]' \
  '--json[Enable JSON output]' \
  '--quiet[Minimal output for scripting]' \
  '1:command:(secret profile service add set get ls rm search cp purge save load unload profiles duplicate env export import migrate run guard test doctor audit install-shell uninstall completion version backend help welcome gui)' \
  '*::arg:->args'

case $state in
  args)
    case $words[2] in
      secret)
        case $words[3] in
          '')
            _values 'secret subcommand' \
              'add[Add a secret]' \
              'set[Set a secret with prompt]' \
              'get[Get a secret value]' \
              'ls[List secrets]' \
              'rm[Remove a secret]' \
              'search[Search secrets by pattern]' \
              'cp[Copy secret to clipboard]'
            ;;
          get)
            _arguments \
              '--full[Show unmasked value]' \
              '--reveal[Show unmasked value]' \
              '*:secret name:'
            ;;
          ls)
            _arguments \
              '--json[JSON output]' \
              '--quiet[Show names only]'
            ;;
          cp)
            _arguments \
              '--clear[Auto-clear time]:time:(20s 30s 60s)' \
              '*:secret name:'
            ;;
        esac
        ;;
      profile)
        case $words[3] in
          '')
            _values 'profile subcommand' \
              'save[Save secrets to profile]' \
              'load[Load profile as environment]' \
              'unload[Unload profile environment]' \
              'ls[List profiles]' \
              'duplicate[Duplicate a profile]' \
              'rm[Remove a profile]'
            ;;
          load|unload)
            _arguments \
              '--persist[Remember for current directory]' \
              '*:profile name:'
            ;;
          rm)
            _arguments \
              '--force[Skip confirmation]' \
              '*:profile name:'
            ;;
        esac
        ;;
      service)
        case $words[3] in
          '')
            _values 'service subcommand' \
              'add[Create a custom service]' \
              'ls[List services]' \
              'show[Show service details]' \
              'edit[Edit a service]' \
              'rm[Remove a service]'
            ;;
        esac
        ;;
      completion)
        _values 'shell' bash zsh fish
        ;;
      guard)
        _values 'action' enable disable status
        ;;
      test)
        _arguments \
          '--all[Test all services]' \
          '--json[JSON output]' \
          '--quiet[Minimal output]' \
          '--fail-fast[Stop on first failure]' \
          '*:service name:'
        ;;
      purge)
        _arguments \
          '--no-backup[Skip backup creation]' \
          '--force[Skip confirmation]'
        ;;
      load|unload)
        _arguments \
          '--persist[Remember for current directory]' \
          '*:profile name:'
        ;;
      env)
        _arguments \
          '--profile[Profile name]:profile name:' '-p+[Profile name]:profile name:'
        ;;
      import)
        _arguments \
          '--profile[Profile name]:profile name:' '-p+[Profile name]:profile name:' \
          '--format[Format]:format:(env dotenv json yaml)' '-f+[Format]:format:(env dotenv json yaml)' \
          '--file[File path]:file:_files' '-i+[File path]:file:_files' \
          '--keys[Only import known service provider keys]'
        ;;
      export)
        _arguments \
          '--profile[Profile name]:profile name:' '-p+[Profile name]:profile name:' \
          '--format[Format]:format:(env dotenv json yaml)' '-f+[Format]:format:(env dotenv json yaml)' \
          '--output[Output file]:file:_files' '-o+[Output file]:file:_files'
        ;;
    esac
  ;;
esac
)ZSH";
}

void generateFishCompletion() {
    std::cout << R"FISH(# Fish completion for ak

# Base command and global options
complete -c ak -f
complete -c ak -s h -l help -d "Show help information"
complete -c ak -s v -l version -d "Show version information"
complete -c ak -l json -d "Enable JSON output"
complete -c ak -l quiet -d "Minimal output for scripting"

# Main commands (namespaced + legacy)
complete -c ak -n "__fish_use_subcommand" -a "secret profile service add set get ls rm search cp purge save load unload profiles duplicate env export import migrate run guard test doctor audit install-shell uninstall completion version backend help welcome gui"

# Secret namespace
complete -c ak -n "__fish_seen_subcommand_from secret; and not __fish_seen_subcommand_from add set get ls rm search cp" -a "add set get ls rm search cp" -d "Secret commands"
complete -c ak -n "__fish_seen_subcommand_from secret; and __fish_seen_subcommand_from get" -l full -d "Show unmasked value"
complete -c ak -n "__fish_seen_subcommand_from secret; and __fish_seen_subcommand_from get" -l reveal -d "Show unmasked value"
complete -c ak -n "__fish_seen_subcommand_from secret; and __fish_seen_subcommand_from ls" -l json -d "JSON output"
complete -c ak -n "__fish_seen_subcommand_from secret; and __fish_seen_subcommand_from ls" -l quiet -d "Show names only"
complete -c ak -n "__fish_seen_subcommand_from secret; and __fish_seen_subcommand_from cp" -l clear -r -d "Auto-clear time (e.g. 20s)"

# Profile namespace
complete -c ak -n "__fish_seen_subcommand_from profile; and not __fish_seen_subcommand_from save load unload ls duplicate rm" -a "save load unload ls duplicate rm" -d "Profile commands"
complete -c ak -n "__fish_seen_subcommand_from profile; and __fish_seen_subcommand_from load unload" -l persist -d "Remember for current directory"
complete -c ak -n "__fish_seen_subcommand_from profile; and __fish_seen_subcommand_from rm" -l force -d "Skip confirmation"

# Service namespace
complete -c ak -n "__fish_seen_subcommand_from service; and not __fish_seen_subcommand_from add ls show edit rm" -a "add ls show edit rm" -d "Service commands"

# completion subcommand
complete -c ak -n "__fish_seen_subcommand_from completion" -a "bash zsh fish" -d "Shell"

# guard subcommand
complete -c ak -n "__fish_seen_subcommand_from guard" -a "enable disable status" -d "Shell guard actions"

# test options
complete -c ak -n "__fish_seen_subcommand_from test" -l all -d "Test all configured providers"
complete -c ak -n "__fish_seen_subcommand_from test" -l json -d "JSON output"
complete -c ak -n "__fish_seen_subcommand_from test" -l quiet -d "Minimal output"
complete -c ak -n "__fish_seen_subcommand_from test" -l fail-fast -d "Stop on first failure"

# purge options
complete -c ak -n "__fish_seen_subcommand_from purge" -l no-backup -d "Skip backup creation"
complete -c ak -n "__fish_seen_subcommand_from purge" -l force -d "Skip confirmation"

# load/unload options (legacy)
complete -c ak -n "__fish_seen_subcommand_from load" -l persist -d "Persist profile for this directory"
complete -c ak -n "__fish_seen_subcommand_from unload" -l persist -d "Remove persisted profile for this directory"

# env options
complete -c ak -n "__fish_seen_subcommand_from env" -s p -l profile -r -d "Profile name"

# import options
complete -c ak -n "__fish_seen_subcommand_from import" -s p -l profile -r -d "Profile name"
complete -c ak -n "__fish_seen_subcommand_from import" -s f -l format -a "env dotenv json yaml" -d "Input format"
complete -c ak -n "__fish_seen_subcommand_from import" -s i -l file -r -d "Input file"
complete -c ak -n "__fish_seen_subcommand_from import" -l keys -d "Only import known service keys"

# export options
complete -c ak -n "__fish_seen_subcommand_from export" -s p -l profile -r -d "Profile name"
complete -c ak -n "__fish_seen_subcommand_from export" -s f -l format -a "env dotenv json yaml" -d "Output format"
complete -c ak -n "__fish_seen_subcommand_from export" -s o -l output -r -d "Output file"

)FISH";
}

void writeBashCompletionToFile(const std::string& path) {
    std::ofstream file(path);
    file << R"(#!/bin/bash
# Bash completion for ak

_ak_completion() {
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    # Main commands (namespaced + legacy)
    local commands="secret profile service add set get ls rm search cp purge save load unload profiles duplicate env export import migrate run guard test doctor audit install-shell uninstall completion version backend help welcome"

    # Handle multi-level completions
    case "${COMP_WORDS[1]}" in
        secret)
            case "${prev}" in
                secret)
                    COMPREPLY=($(compgen -W "add set get ls rm search cp" -- ${cur}))
                    return 0
                    ;;
                get)
                    local secrets=$(ak ls 2>/dev/null | awk '{print $1}')
                    COMPREPLY=($(compgen -W "${secrets} --full --reveal" -- ${cur}))
                    return 0
                    ;;
                ls)
                    COMPREPLY=($(compgen -W "--json --quiet" -- ${cur}))
                    return 0
                    ;;
                cp)
                    local secrets=$(ak ls 2>/dev/null | awk '{print $1}')
                    COMPREPLY=($(compgen -W "${secrets} --clear" -- ${cur}))
                    return 0
                    ;;
                add|set|rm|search)
                    local secrets=$(ak ls 2>/dev/null | awk '{print $1}')
                    COMPREPLY=($(compgen -W "${secrets}" -- ${cur}))
                    return 0
                    ;;
            esac
            ;;
        profile)
            case "${prev}" in
                profile)
                    COMPREPLY=($(compgen -W "save load unload ls duplicate rm" -- ${cur}))
                    return 0
                    ;;
                load|unload|duplicate|rm)
                    local profiles=$(ak profiles 2>/dev/null | tr '\n' ' ')
                    COMPREPLY=($(compgen -W "${profiles} --persist --force" -- ${cur}))
                    return 0
                    ;;
                save)
                    local profiles=$(ak profiles 2>/dev/null | tr '\n' ' ')
                    local secrets=$(ak ls 2>/dev/null | awk '{print $1}')
                    COMPREPLY=($(compgen -W "${profiles} ${secrets}" -- ${cur}))
                    return 0
                    ;;
            esac
            ;;
        service)
            case "${prev}" in
                service)
                    COMPREPLY=($(compgen -W "add ls show edit rm" -- ${cur}))
                    return 0
                    ;;
                show|edit|rm)
                    local services=$(ak service ls 2>/dev/null | grep -v "Built-in\|Custom\|Available" | awk '{print $1}')
                    COMPREPLY=($(compgen -W "${services}" -- ${cur}))
                    return 0
                    ;;
            esac
            ;;
    esac

    case "${prev}" in
        ak)
            COMPREPLY=($(compgen -W "${commands}" -- ${cur}))
            return 0
            ;;
        --profile|-p)
            # Complete with available profiles
            local profiles=$(ak profiles 2>/dev/null | tr '\n' ' ')
            COMPREPLY=($(compgen -W "${profiles}" -- ${cur}))
            return 0
            ;;
        --format|-f)
            COMPREPLY=($(compgen -W "env dotenv json yaml" -- ${cur}))
            return 0
            ;;
        completion)
            COMPREPLY=($(compgen -W "bash zsh fish" -- ${cur}))
            return 0
            ;;
        guard)
            COMPREPLY=($(compgen -W "enable disable status" -- ${cur}))
            return 0
            ;;
        service)
            COMPREPLY=($(compgen -W "add ls show edit rm" -- ${cur}))
            return 0
            ;;
        test)
            local services="anthropic azure_openai brave cohere deepseek exa fireworks gemini groq huggingface inference langchain continue composio hyperbolic logfire mistral openai openrouter perplexity sambanova tavily together xai"
            COMPREPLY=($(compgen -W "${services} --all --json --fail-fast --quiet" -- ${cur}))
            return 0
            ;;
        env)
            COMPREPLY=($(compgen -W "--profile -p" -- ${cur}))
            return 0
            ;;
        import)
            COMPREPLY=($(compgen -W "--profile -p --format -f --file -i --keys" -- ${cur}))
            return 0
            ;;
        export)
            COMPREPLY=($(compgen -W "--profile -p --format -f --output -o" -- ${cur}))
            return 0
            ;;
    esac

    # Global flags
    local global_flags="--json --help --version"
    COMPREPLY=($(compgen -W "${global_flags}" -- ${cur}))
}

complete -F _ak_completion ak
)";
}

void writeZshCompletionToFile(const std::string& path) {
    std::ofstream file(path);
    file << R"ZSH(#compdef ak
# Zsh completion for ak

_arguments -C \
  '(-h --help)'{-h,--help}'[Show help information]' \
  '(-v --version)'{-v,--version}'[Show version information]' \
  '--json[Enable JSON output]' \
  '--quiet[Minimal output for scripting]' \
  '1:command:(secret profile service add set get ls rm search cp purge save load unload profiles duplicate env export import migrate run guard test doctor audit install-shell uninstall completion version backend help welcome gui)' \
  '*::arg:->args'

case $state in
  args)
    case $words[2] in
      secret)
        case $words[3] in
          '')
            _values 'secret subcommand' \
              'add[Add a secret]' \
              'set[Set a secret with prompt]' \
              'get[Get a secret value]' \
              'ls[List secrets]' \
              'rm[Remove a secret]' \
              'search[Search secrets by pattern]' \
              'cp[Copy secret to clipboard]'
            ;;
          get)
            _arguments \
              '--full[Show unmasked value]' \
              '--reveal[Show unmasked value]' \
              '*:secret name:'
            ;;
          ls)
            _arguments \
              '--json[JSON output]' \
              '--quiet[Show names only]'
            ;;
          cp)
            _arguments \
              '--clear[Auto-clear time]:time:(20s 30s 60s)' \
              '*:secret name:'
            ;;
        esac
        ;;
      profile)
        case $words[3] in
          '')
            _values 'profile subcommand' \
              'save[Save secrets to profile]' \
              'load[Load profile as environment]' \
              'unload[Unload profile environment]' \
              'ls[List profiles]' \
              'duplicate[Duplicate a profile]' \
              'rm[Remove a profile]'
            ;;
          load|unload)
            _arguments \
              '--persist[Remember for current directory]' \
              '*:profile name:'
            ;;
          rm)
            _arguments \
              '--force[Skip confirmation]' \
              '*:profile name:'
            ;;
        esac
        ;;
      service)
        case $words[3] in
          '')
            _values 'service subcommand' \
              'add[Create a custom service]' \
              'ls[List services]' \
              'show[Show service details]' \
              'edit[Edit a service]' \
              'rm[Remove a service]'
            ;;
        esac
        ;;
      completion)
        _values 'shell' bash zsh fish
        ;;
      guard)
        _values 'action' enable disable status
        ;;
      test)
        _arguments \
          '--all[Test all services]' \
          '--json[JSON output]' \
          '--quiet[Minimal output]' \
          '--fail-fast[Stop on first failure]' \
          '*:service name:'
        ;;
      purge)
        _arguments \
          '--no-backup[Skip backup creation]' \
          '--force[Skip confirmation]'
        ;;
      load|unload)
        _arguments \
          '--persist[Remember for current directory]' \
          '*:profile name:'
        ;;
      env)
        _arguments \
          '--profile[Profile name]:profile name:' '-p+[Profile name]:profile name:'
        ;;
      import)
        _arguments \
          '--profile[Profile name]:profile name:' '-p+[Profile name]:profile name:' \
          '--format[Format]:format:(env dotenv json yaml)' '-f+[Format]:format:(env dotenv json yaml)' \
          '--file[File path]:file:_files' '-i+[File path]:file:_files' \
          '--keys[Only import known service provider keys]'
        ;;
      export)
        _arguments \
          '--profile[Profile name]:profile name:' '-p+[Profile name]:profile name:' \
          '--format[Format]:format:(env dotenv json yaml)' '-f+[Format]:format:(env dotenv json yaml)' \
          '--output[Output file]:file:_files' '-o+[Output file]:file:_files'
        ;;
    esac
  ;;
esac
)ZSH";
}

void writeFishCompletionToFile(const std::string& path) {
    std::ofstream file(path);
    file << R"FISH(# Fish completion for ak

# Base command and global options
complete -c ak -f
complete -c ak -s h -l help -d "Show help information"
complete -c ak -s v -l version -d "Show version information"
complete -c ak -l json -d "Enable JSON output"
complete -c ak -l quiet -d "Minimal output for scripting"

# Main commands (namespaced + legacy)
complete -c ak -n "__fish_use_subcommand" -a "secret profile service add set get ls rm search cp purge save load unload profiles duplicate env export import migrate run guard test doctor audit install-shell uninstall completion version backend help welcome gui"

# Secret namespace
complete -c ak -n "__fish_seen_subcommand_from secret; and not __fish_seen_subcommand_from add set get ls rm search cp" -a "add set get ls rm search cp" -d "Secret commands"
complete -c ak -n "__fish_seen_subcommand_from secret; and __fish_seen_subcommand_from get" -l full -d "Show unmasked value"
complete -c ak -n "__fish_seen_subcommand_from secret; and __fish_seen_subcommand_from get" -l reveal -d "Show unmasked value"
complete -c ak -n "__fish_seen_subcommand_from secret; and __fish_seen_subcommand_from ls" -l json -d "JSON output"
complete -c ak -n "__fish_seen_subcommand_from secret; and __fish_seen_subcommand_from ls" -l quiet -d "Show names only"
complete -c ak -n "__fish_seen_subcommand_from secret; and __fish_seen_subcommand_from cp" -l clear -r -d "Auto-clear time (e.g. 20s)"

# Profile namespace
complete -c ak -n "__fish_seen_subcommand_from profile; and not __fish_seen_subcommand_from save load unload ls duplicate rm" -a "save load unload ls duplicate rm" -d "Profile commands"
complete -c ak -n "__fish_seen_subcommand_from profile; and __fish_seen_subcommand_from load unload" -l persist -d "Remember for current directory"
complete -c ak -n "__fish_seen_subcommand_from profile; and __fish_seen_subcommand_from rm" -l force -d "Skip confirmation"

# Service namespace
complete -c ak -n "__fish_seen_subcommand_from service; and not __fish_seen_subcommand_from add ls show edit rm" -a "add ls show edit rm" -d "Service commands"

# completion subcommand
complete -c ak -n "__fish_seen_subcommand_from completion" -a "bash zsh fish" -d "Shell"

# guard subcommand
complete -c ak -n "__fish_seen_subcommand_from guard" -a "enable disable status" -d "Shell guard actions"

# test options
complete -c ak -n "__fish_seen_subcommand_from test" -l all -d "Test all configured providers"
complete -c ak -n "__fish_seen_subcommand_from test" -l json -d "JSON output"
complete -c ak -n "__fish_seen_subcommand_from test" -l quiet -d "Minimal output"
complete -c ak -n "__fish_seen_subcommand_from test" -l fail-fast -d "Stop on first failure"

# purge options
complete -c ak -n "__fish_seen_subcommand_from purge" -l no-backup -d "Skip backup creation"
complete -c ak -n "__fish_seen_subcommand_from purge" -l force -d "Skip confirmation"

# load/unload options (legacy)
complete -c ak -n "__fish_seen_subcommand_from load" -l persist -d "Persist profile for this directory"
complete -c ak -n "__fish_seen_subcommand_from unload" -l persist -d "Remove persisted profile for this directory"

# env options
complete -c ak -n "__fish_seen_subcommand_from env" -s p -l profile -r -d "Profile name"

# import options
complete -c ak -n "__fish_seen_subcommand_from import" -s p -l profile -r -d "Profile name"
complete -c ak -n "__fish_seen_subcommand_from import" -s f -l format -a "env dotenv json yaml" -d "Input format"
complete -c ak -n "__fish_seen_subcommand_from import" -s i -l file -r -d "Input file"
complete -c ak -n "__fish_seen_subcommand_from import" -l keys -d "Only import known service keys"

# export options
complete -c ak -n "__fish_seen_subcommand_from export" -s p -l profile -r -d "Profile name"
complete -c ak -n "__fish_seen_subcommand_from export" -s f -l format -a "env dotenv json yaml" -d "Output format"
complete -c ak -n "__fish_seen_subcommand_from export" -s o -l output -r -d "Output file"

)FISH";
}

} // namespace cli
} // namespace ak
