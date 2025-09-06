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
             << ui::colorize("ak set API_KEY", ui::Colors::BRIGHT_CYAN) << "\n";
    std::cout << ui::colorize("2. ", ui::Colors::BRIGHT_WHITE) << ui::colorize("Create profiles to organize secrets: ", ui::Colors::WHITE)
             << ui::colorize("ak save prod API_KEY DB_URL", ui::Colors::BRIGHT_CYAN) << "\n";
    std::cout << ui::colorize("3. ", ui::Colors::BRIGHT_WHITE) << ui::colorize("Load secrets into your shell: ", ui::Colors::WHITE)
             << ui::colorize("ak load prod", ui::Colors::BRIGHT_CYAN) << "\n";
    std::cout << ui::colorize("4. ", ui::Colors::BRIGHT_WHITE) << ui::colorize("Import from .env files: ", ui::Colors::WHITE)
             << ui::colorize("ak import -p dev -f env -i .env --keys", ui::Colors::BRIGHT_CYAN) << "\n";
    std::cout << ui::colorize("5. ", ui::Colors::BRIGHT_WHITE) << ui::colorize("Run ", ui::Colors::WHITE)
             << ui::colorize("ak help", ui::Colors::BRIGHT_MAGENTA) << ui::colorize(" for detailed documentation", ui::Colors::WHITE) << "\n\n";
}

void cmd_help() {
    showLogo();
    
    std::cout << ui::colorize("USAGE:", ui::Colors::BRIGHT_WHITE + ui::Colors::BOLD) << "\n";
    std::cout << "  " << ui::colorize("ak", ui::Colors::BRIGHT_CYAN) << ui::colorize(" <command> [options] [arguments]", ui::Colors::WHITE) << "\n\n";

    std::cout << ui::colorize("SECRET MANAGEMENT:", ui::Colors::BRIGHT_GREEN + ui::Colors::BOLD) << "\n";
    std::cout << "  " << ui::colorize("ak add <NAME> <VALUE>", ui::Colors::BRIGHT_CYAN) << "           Add a secret with value directly\n";
    std::cout << "  " << ui::colorize("ak add <NAME=VALUE>", ui::Colors::BRIGHT_CYAN) << "             Add a secret using NAME=VALUE format\n";
    std::cout << "  " << ui::colorize("ak add -p <profile> <NAME> <VALUE>", ui::Colors::BRIGHT_CYAN) << " Add a secret and add to profile\n";
    std::cout << "  " << ui::colorize("ak set <NAME>", ui::Colors::BRIGHT_CYAN) << "                   Set a secret (prompts for value)\n";
    std::cout << "  " << ui::colorize("ak get <NAME> [--full]", ui::Colors::BRIGHT_CYAN) << "          Get a secret value (--full shows unmasked)\n";
    std::cout << "  " << ui::colorize("ak ls [--json]", ui::Colors::BRIGHT_CYAN) << "                  List all secret names (--json for JSON output)\n";
    std::cout << "  " << ui::colorize("ak rm <NAME>", ui::Colors::BRIGHT_CYAN) << "                    Remove a secret\n";
    std::cout << "  " << ui::colorize("ak rm --profile <NAME>", ui::Colors::BRIGHT_CYAN) << "          Remove a profile\n";
    std::cout << "  " << ui::colorize("ak search <PATTERN>", ui::Colors::BRIGHT_CYAN) << "             Search for secrets by name pattern (case-insensitive)\n";
    std::cout << "  " << ui::colorize("ak cp <NAME>", ui::Colors::BRIGHT_CYAN) << "                    Copy secret value to clipboard\n";
    std::cout << "  " << ui::colorize("ak purge [--no-backup]", ui::Colors::BRIGHT_CYAN) << "          Remove all secrets and profiles (creates backup by default)\n\n";

    std::cout << ui::colorize("PROFILE MANAGEMENT:", ui::Colors::BRIGHT_BLUE + ui::Colors::BOLD) << "\n";
    std::cout << "  " << ui::colorize("ak save <profile> [NAMES...]", ui::Colors::BRIGHT_CYAN) << "    Save secrets to a profile (all secrets if no names given)\n";
    std::cout << "  " << ui::colorize("ak load <profile> [--persist]", ui::Colors::BRIGHT_CYAN) << "   Load profile as environment variables\n";
    std::cout << "                                  " << ui::colorize("--persist: remember profile for current directory", ui::Colors::DIM) << "\n";
    std::cout << "  " << ui::colorize("ak unload [<profile>] [--persist]", ui::Colors::BRIGHT_CYAN) << " Unload profile environment variables\n";
    std::cout << "  " << ui::colorize("ak profiles", ui::Colors::BRIGHT_CYAN) << "                     List all available profiles\n";
    std::cout << "  " << ui::colorize("ak duplicate <src> <dest>", ui::Colors::BRIGHT_CYAN) << "      Duplicate a profile with a new name\n";
    std::cout << "  " << ui::colorize("ak env --profile|-p <name>", ui::Colors::BRIGHT_CYAN) << "      Show profile as export statements\n\n";

    std::cout << ui::colorize("EXPORT/IMPORT:", ui::Colors::BRIGHT_MAGENTA + ui::Colors::BOLD) << "\n";
    std::cout << "  " << ui::colorize("ak export --profile|-p <p> --format|-f <fmt> --output|-o <file>", ui::Colors::BRIGHT_CYAN) << "\n";
    std::cout << "                                  Export profile to file\n";
    std::cout << "  " << ui::colorize("ak import --profile|-p <p> --format|-f <fmt> --file|-i <file> [--keys]", ui::Colors::BRIGHT_CYAN) << "\n";
    std::cout << "                                  Import secrets from file to profile\n";
    std::cout << "                                  " << ui::colorize("--keys: only import known service provider keys", ui::Colors::BRIGHT_YELLOW) << "\n";
    std::cout << "                                  \n";
    std::cout << "  " << ui::colorize("Supported formats:", ui::Colors::WHITE) << " env, dotenv, json, yaml\n\n";

    std::cout << ui::colorize("SERVICE MANAGEMENT:", ui::Colors::BRIGHT_MAGENTA + ui::Colors::BOLD) << "\n";
    std::cout << "  " << ui::colorize("ak service add", ui::Colors::BRIGHT_CYAN) << "                   Create a new custom API service\n";
    std::cout << "  " << ui::colorize("ak service list", ui::Colors::BRIGHT_CYAN) << "                  List all custom services\n";
    std::cout << "  " << ui::colorize("ak service edit <name>", ui::Colors::BRIGHT_CYAN) << "          Edit an existing custom service\n";
    std::cout << "  " << ui::colorize("ak service delete <name>", ui::Colors::BRIGHT_CYAN) << "        Delete a custom service\n\n";

    std::cout << ui::colorize("UTILITIES:", ui::Colors::BRIGHT_YELLOW + ui::Colors::BOLD) << "\n";
    std::cout << "  " << ui::colorize("ak run --profile|-p <p> -- <cmd>", ui::Colors::BRIGHT_CYAN) << "  Run command with profile environment loaded\n";
    std::cout << "  " << ui::colorize("ak test [<service>|--all] [--json] [--fail-fast]", ui::Colors::BRIGHT_CYAN) << "  Test connectivity (defaults to configured providers)\n";
    std::cout << "  " << ui::colorize("ak guard enable|disable", ui::Colors::BRIGHT_CYAN) << "         Enable/disable shell guard for secret protection\n";
    std::cout << "  " << ui::colorize("ak doctor", ui::Colors::BRIGHT_CYAN) << "                       Check system configuration and dependencies\n";
    std::cout << "  " << ui::colorize("ak audit [N]", ui::Colors::BRIGHT_CYAN) << "                    Show audit log (last N entries, default: 10)\n\n";
    
    std::cout << ui::colorize("SYSTEM:", ui::Colors::BRIGHT_RED + ui::Colors::BOLD) << "\n";
    std::cout << "  " << ui::colorize("ak help", ui::Colors::BRIGHT_CYAN) << "                         Show this help message\n";
    std::cout << "  " << ui::colorize("ak version", ui::Colors::BRIGHT_CYAN) << "                      Show version information\n";
    std::cout << "  " << ui::colorize("ak backend", ui::Colors::BRIGHT_CYAN) << "                      Show backend information (GPG status, vault location)\n";
    std::cout << "  " << ui::colorize("ak install-shell", ui::Colors::BRIGHT_CYAN) << "                Install shell integration for auto-loading\n";
    std::cout << "  " << ui::colorize("ak uninstall", ui::Colors::BRIGHT_CYAN) << "                    Remove shell integration\n";
    std::cout << "  " << ui::colorize("ak completion <shell>", ui::Colors::BRIGHT_CYAN) << "           Generate completion script for bash, zsh, or fish\n\n";

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

    # Main commands
    local commands="add set get ls rm search cp purge save load unload profiles duplicate env export import migrate run guard test doctor audit service install-shell uninstall completion version backend help welcome"

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
            COMPREPLY=($(compgen -W "enable disable" -- ${cur}))
            return 0
            ;;
        service)
            COMPREPLY=($(compgen -W "add list edit delete" -- ${cur}))
            return 0
            ;;
        test)
            local services="anthropic azure_openai brave cohere deepseek exa fireworks gemini groq huggingface inference langchain continue composio hyperbolic logfire mistral openai openrouter perplexity sambanova tavily together xai"
            COMPREPLY=($(compgen -W "${services} --all --json --fail-fast" -- ${cur}))
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

void generateZshCompletion() {
    // Output zsh completion - simplified for now
    std::cout << "#compdef ak\n";
    std::cout << "# Zsh completion for ak\n";
    std::cout << "# Full completion script would go here\n";
}

void generateFishCompletion() {
    // Output fish completion - simplified for now
    std::cout << "# Fish completion for ak\n";
    std::cout << "# Full completion script would go here\n";
}

void writeBashCompletionToFile(const std::string& path) {
    std::ofstream file(path);
    file << R"(#!/bin/bash
# Bash completion for ak
)";
    // The full bash completion script would be written here
    // This is a simplified placeholder
}

void writeZshCompletionToFile(const std::string& path) {
    std::ofstream file(path);
    file << "#compdef ak\n";
    // The full zsh completion script would be written here
    // This is a simplified placeholder
}

void writeFishCompletionToFile(const std::string& path) {
    std::ofstream file(path);
    file << "# Fish completion for ak\n";
    // The full fish completion script would be written here
    // This is a simplified placeholder
}

} // namespace cli
} // namespace ak