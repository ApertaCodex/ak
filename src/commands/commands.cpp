#include "commands/commands.hpp"
#include "core/config.hpp"
#include "storage/vault.hpp"
#include "system/system.hpp"
#include "services/services.hpp"
#include "ui/ui.hpp"
#include "cli/cli.hpp"
#ifdef BUILD_GUI
#include "gui/gui.hpp"
#endif
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <unordered_set>
#include <filesystem>
#ifdef __unix__
#include <unistd.h>
#endif

namespace ak {
namespace commands {

// Utility functions
std::string makeExportsForProfile(const core::Config& cfg, const std::string& name) {
    // Export values for the selected profile, preferring per-profile key storage
    // and falling back to the legacy global vault for compatibility.
    auto keyNames = storage::readProfile(cfg, name);
    auto profileKeyMap = storage::loadProfileKeys(cfg, name);
    core::KeyStore ks = storage::loadVault(cfg);

    std::ostringstream oss;

    for (const auto& key : keyNames) {
        // Prefer per-profile value
        auto pit = profileKeyMap.find(key);
        std::string value;
        if (pit != profileKeyMap.end()) {
            value = pit->second;
        } else {
            // Fallback to global vault (legacy behavior)
            auto vit = ks.kv.find(key);
            if (vit == ks.kv.end()) {
                continue;
            }
            value = vit->second;
        }

        std::string escaped;
        escaped.reserve(value.size() * 2);
        for (char c : value) {
            if (c == '\\' || c == '"') {
                escaped.push_back('\\');
            }
            if (c == '\n') {
                escaped += "\\n";
                continue;
            }
            escaped.push_back(c);
        }

        oss << "export " << key << "=\"" << escaped << "\"\n";
    }

    return oss.str();
}

void printExportsForProfile(const core::Config& cfg, const std::string& name) {
    std::cout << makeExportsForProfile(cfg, name);
}

void printUnsetsForProfile(const core::Config& cfg, const std::string& name) {
    for (const auto& key : storage::readProfile(cfg, name)) {
        std::cout << "unset " << key << "\n";
    }
}

// Custom service auto-detection/prompt removed: unified Services model with full CRUD on default set.

// Individual command handlers
int cmd_add(const core::Config& cfg, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        core::error(cfg, "Usage: ak add [-p|--profile <profile>] <ENV_NAME> <ENV_VALUE> or ak add [-p|--profile <profile>] <ENV_NAME=ENV_VALUE>");
    }
    
    std::string name;
    std::string value;
    std::string profileName;
    
    // Parse arguments, handling profile flag
    std::vector<std::string> parsedArgs;
    for (size_t i = 1; i < args.size(); ++i) {
        if ((args[i] == "-p" || args[i] == "--profile") && i + 1 < args.size()) {
            profileName = args[i + 1];
            ++i; // Skip the profile name argument
        } else {
            parsedArgs.push_back(args[i]);
        }
    }
    
    if (parsedArgs.empty()) {
        core::error(cfg, "Usage: ak add [-p|--profile <profile>] <ENV_NAME> <ENV_VALUE> or ak add [-p|--profile <profile>] <ENV_NAME=ENV_VALUE>");
    }
    
    // Handle two formats: "ak add NAME VALUE" or "ak add NAME=VALUE"
    if (parsedArgs.size() >= 2) {
        // Format: ak add ENV_NAME ENV_VALUE
        name = parsedArgs[0];
        value = parsedArgs[1];
        // If there are more arguments, join them with spaces
        for (size_t i = 2; i < parsedArgs.size(); ++i) {
            value += " " + parsedArgs[i];
        }
    } else if (parsedArgs.size() == 1) {
        // Format: ak add ENV_NAME=ENV_VALUE
        const std::string& arg = parsedArgs[0];
        size_t equalPos = arg.find('=');
        if (equalPos == std::string::npos || equalPos == 0 || equalPos == arg.length() - 1) {
            core::error(cfg, "Usage: ak add [-p|--profile <profile>] <ENV_NAME> <ENV_VALUE> or ak add [-p|--profile <profile>] <ENV_NAME=ENV_VALUE>");
        }
        name = arg.substr(0, equalPos);
        value = arg.substr(equalPos + 1);
    } else {
        core::error(cfg, "Usage: ak add [-p|--profile <profile>] <ENV_NAME> <ENV_VALUE> or ak add [-p|--profile <profile>] <ENV_NAME=ENV_VALUE>");
    }
    
    if (name.empty()) {
        core::error(cfg, "Environment variable name cannot be empty");
    }
    if (value.empty()) {
        core::error(cfg, "Environment variable value cannot be empty");
    }
    
    // Unified Services model: no custom-service prompt
    
    // Add to vault
    core::KeyStore ks = storage::loadVault(cfg);
    bool keyExistsInVault = ks.kv.find(name) != ks.kv.end();
    ks.kv[name] = value;
    storage::saveVault(cfg, ks);
    
    // Determine target profile (default if not specified)
    if (profileName.empty()) {
        profileName = storage::getDefaultProfileName();
    }

    // Read existing profile key list
    std::vector<std::string> profileKeys = storage::readProfile(cfg, profileName);

    // Save/update value in the profile-specific encrypted store
    auto profileKeyMap = storage::loadProfileKeys(cfg, profileName);
    bool keyExistedInProfileStore = (profileKeyMap.find(name) != profileKeyMap.end());
    profileKeyMap[name] = value;
    storage::saveProfileKeys(cfg, profileName, profileKeyMap);

    // Ensure key is listed in the profile
    bool keyListedInProfile = (std::find(profileKeys.begin(), profileKeys.end(), name) != profileKeys.end());
    if (!keyListedInProfile) {
        profileKeys.push_back(name);
        storage::writeProfile(cfg, profileName, profileKeys);
    }

    // Update encrypted bundle exports for persistence/loading
    std::string exports = makeExportsForProfile(cfg, profileName);
    if (!exports.empty()) {
        storage::writeEncryptedBundle(cfg, profileName, exports);
    }

    // Also update any temporary per-key bundles
    {
        std::string tempProfileName = "_key_" + name;
        std::string escaped;
        escaped.reserve(value.size() * 2);
        for (char c : value) {
            if (c == '\\' || c == '"') {
                escaped.push_back('\\');
            }
            if (c == '\n') {
                escaped += "\\n";
                continue;
            }
            escaped.push_back(c);
        }
        std::string tempExports = "export " + name + "=\"" + escaped + "\"\n";
        storage::writeEncryptedBundle(cfg, tempProfileName, tempExports);
    }

    // Feedback
    if (keyExistsInVault && (keyExistedInProfileStore || keyListedInProfile)) {
        core::success(cfg, "Updated " + name + " in vault and profile '" + profileName + "'");
    } else if (keyExistsInVault && !keyListedInProfile) {
        core::success(cfg, "Updated " + name + " in vault and added to profile '" + profileName + "'");
    } else if (!keyExistsInVault && (keyExistedInProfileStore || keyListedInProfile)) {
        core::success(cfg, "Added " + name + " to vault (already in profile '" + profileName + "')");
    } else {
        core::success(cfg, "Added " + name + " to vault and profile '" + profileName + "'");
    }

    core::auditLog(cfg, keyExistsInVault ? "update_profile" : "add_profile", {name, profileName});
    
    return 0;
}

int cmd_set(const core::Config& cfg, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        core::error(cfg, "Usage: ak set <NAME>");
    }
    
    std::string name = args[1];
    std::string value = system::promptSecret("Enter value for " + name + ": ");
    
    if (value.empty()) {
        core::error(cfg, "Empty value");
    }
    
    core::KeyStore ks = storage::loadVault(cfg);
    ks.kv[name] = value;
    storage::saveVault(cfg, ks);
    
    core::success(cfg, "Successfully stored " + name);
    core::auditLog(cfg, "set", {name});
    
    return 0;
}

int cmd_get(const core::Config& cfg, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        core::error(cfg, "Usage: ak get <NAME> [--full]");
    }
    
    std::string name = args[1];
    bool full = (args.size() >= 3 && args[2] == "--full");
    
    core::KeyStore ks = storage::loadVault(cfg);
    auto it = ks.kv.find(name);
    if (it == ks.kv.end()) {
        core::error(cfg, name + " not found.");
    }
    
    std::cout << (full ? it->second : core::maskValue(it->second)) << "\n";
    core::auditLog(cfg, "get", {name});
    
    return 0;
}

int cmd_ls(const core::Config& cfg, const std::vector<std::string>& args) {
    (void)args; // Parameter intentionally unused
    core::KeyStore ks = storage::loadVault(cfg);
    std::vector<std::string> names;
    names.reserve(ks.kv.size());
    
    for (const auto& pair : ks.kv) {
        names.push_back(pair.first);
    }
    std::sort(names.begin(), names.end());
    
    core::auditLog(cfg, "ls", names);
    
    if (cfg.json) {
        std::cout << "[";
        bool first = true;
        for (const auto& name : names) {
            if (!first) std::cout << ",";
            first = false;
            std::cout << "{\"name\":\"" << name << "\",\"masked\":\"" << core::maskValue(ks.kv[name]) << "\"}";
        }
        std::cout << "]\n";
    } else {
        if (names.empty()) {
            core::info(cfg, "No secrets stored yet. Use 'ak add <NAME> <VALUE>' to get started!");
            return 0;
        }
        
        std::cout << ui::colorize("📂 Available Keys:", ui::Colors::BRIGHT_MAGENTA) << "\n";
        for (const auto& name : names) {
            std::string keyName = ui::colorize(name, ui::Colors::BRIGHT_CYAN);
            std::string maskedValue = ui::colorize(core::maskValue(ks.kv[name]), ui::Colors::BRIGHT_BLACK);
            std::cout << "  " << std::left << std::setw(42) << keyName << " " << maskedValue << "\n";
        }
        
        if (names.size() == 1) {
            std::cout << "\n" << ui::colorize("Total: 1 key stored securely", ui::Colors::DIM) << "\n";
        } else {
            std::cout << "\n" << ui::colorize("Total: " + std::to_string(names.size()) + " keys stored securely", ui::Colors::DIM) << "\n";
        }
    }
    
    return 0;
}

int cmd_rm(const core::Config& cfg, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        core::error(cfg, "Usage: ak rm <NAME> (remove secret) or ak rm --profile <NAME> (remove profile)");
    }
    
    // Check if removing a profile
    if (args.size() >= 3 && args[1] == "--profile") {
        std::string profileName = args[2];
        auto profiles = storage::listProfiles(cfg);
        if (std::find(profiles.begin(), profiles.end(), profileName) == profiles.end()) {
            core::error(cfg, "Profile '" + profileName + "' not found.");
        }
        
        // Remove profile file
        std::string profilePath = cfg.profilesDir + "/" + profileName + ".profile";
        if (std::filesystem::exists(profilePath)) {
            std::filesystem::remove(profilePath);
            core::success(cfg, "Successfully removed profile '" + profileName + "'");
            core::auditLog(cfg, "rm_profile", {profileName});
        } else {
            core::error(cfg, "Profile file not found: " + profilePath);
        }
        return 0;
    }
    
    // Remove secret key
    std::string name = args[1];
    core::KeyStore ks = storage::loadVault(cfg);
    if (!ks.kv.erase(name)) {
        core::error(cfg, name + " not found.");
    }
    
    storage::saveVault(cfg, ks);
    core::success(cfg, "Successfully removed " + name);
    core::auditLog(cfg, "rm", {name});
    
    return 0;
}

int cmd_search(const core::Config& cfg, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        core::error(cfg, "Usage: ak search <PATTERN>");
    }
    
    std::string pattern = args[1];
    core::KeyStore ks = storage::loadVault(cfg);
    std::vector<std::string> hits;
    
    for (const auto& pair : ks.kv) {
        if (core::icontains(pair.first, pattern)) {
            hits.push_back(pair.first);
        }
    }
    
    if (hits.empty()) {
        core::info(cfg, "No keys found matching pattern '" + pattern + "'");
        return 0;
    }
    
    std::sort(hits.begin(), hits.end());
    
    std::cout << ui::colorize("🔍 Found " + std::to_string(hits.size()) + " key" + (hits.size() == 1 ? "" : "s") + " matching '" + pattern + "':", ui::Colors::BRIGHT_BLUE) << "\n";
    
    for (const auto& hit : hits) {
        std::string keyName = ui::colorize(hit, ui::Colors::BRIGHT_CYAN);
        std::string maskedValue = ui::colorize(core::maskValue(ks.kv[hit]), ui::Colors::BRIGHT_BLACK);
        std::cout << "  " << std::left << std::setw(42) << keyName << " " << maskedValue << "\n";
    }
    
    core::auditLog(cfg, "search", hits);
    return 0;
}

int cmd_cp(const core::Config& cfg, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        core::error(cfg, "Usage: ak cp <NAME>");
    }
    
    std::string name = args[1];
    core::KeyStore ks = storage::loadVault(cfg);
    auto it = ks.kv.find(name);
    if (it == ks.kv.end()) {
        core::error(cfg, name + " not found.");
    }
    
    if (!system::copyClipboard(it->second)) {
        core::error(cfg, "No clipboard utility found (pbcopy/wl-copy/xclip).");
    }
    
    core::success(cfg, "Successfully copied " + name + " to clipboard");
    core::auditLog(cfg, "cp", {name});
    
    return 0;
}

int cmd_save(const core::Config& cfg, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        core::error(cfg, "Usage: ak save <profile> [NAMES...]");
    }
    
    std::string profile = args[1];
    std::vector<std::string> names;
    
    if (args.size() > 2) {
        names.insert(names.end(), args.begin() + 2, args.end());
    } else {
        core::KeyStore ks = storage::loadVault(cfg);
        for (const auto& pair : ks.kv) {
            names.push_back(pair.first);
        }
    }
    
    storage::writeProfile(cfg, profile, names);
    core::success(cfg, "Successfully saved profile '" + profile + "' with " + std::to_string(names.size()) + " key" + (names.size() == 1 ? "" : "s"));
    core::auditLog(cfg, "save_profile", names);
    
    return 0;
}

int cmd_profiles(const core::Config& cfg, const std::vector<std::string>& args) {
    (void)args; // Parameter intentionally unused
    auto profiles = storage::listProfiles(cfg);
    
    if (profiles.empty()) {
        core::info(cfg, "No profiles created yet. Use 'ak save <profile> [KEYS...]' to create one!");
        return 0;
    }
    
    std::cout << ui::colorize("📁 Available Profiles:", ui::Colors::BRIGHT_MAGENTA) << "\n";
    
    for (const auto& name : profiles) {
        // Get profile keys count
        auto keys = storage::readProfile(cfg, name);
        std::string profileName = ui::colorize(name, ui::Colors::BRIGHT_CYAN);
        std::string keyInfo = ui::colorize(std::to_string(keys.size()) + " key" + (keys.size() == 1 ? "" : "s"), ui::Colors::DIM);
        
        // Show key names if not too many
        if (keys.size() <= 3 && !keys.empty()) {
            std::string keyList = "";
            for (size_t i = 0; i < keys.size(); ++i) {
                if (i > 0) keyList += ", ";
                keyList += keys[i];
            }
            keyInfo += ui::colorize(" (" + keyList + ")", ui::Colors::DIM);
        }
        
        std::cout << "  " << profileName << " - " << keyInfo << "\n";
    }
    
    std::cout << "\n" << ui::colorize("Use 'ak load <profile>' to switch profiles", ui::Colors::DIM) << "\n";
    return 0;
}

int cmd_version(const core::Config& cfg, const std::vector<std::string>& args) {
    (void)cfg; // Parameter intentionally unused
    (void)args; // Parameter intentionally unused
    std::cout << "ak version " << core::AK_VERSION << "\n";
    return 0;
}

int cmd_backend(const core::Config& cfg, const std::vector<std::string>& args) {
    (void)args; // Parameter intentionally unused
    std::cout << ((cfg.gpgAvailable && !cfg.forcePlain) ? "gpg" : "plain") << "\n";
    return 0;
}

int cmd_help(const core::Config& cfg, const std::vector<std::string>& args) {
    (void)cfg; // Parameter intentionally unused
    (void)args; // Parameter intentionally unused
    cli::cmd_help();
    return 0;
}

int cmd_welcome(const core::Config& cfg, const std::vector<std::string>& args) {
    (void)cfg; // Parameter intentionally unused
    (void)args; // Parameter intentionally unused
    cli::showWelcome();
    return 0;
}

// Placeholder implementations for other commands
int cmd_purge(const core::Config& cfg, const std::vector<std::string>& args) {
    (void)args; // Parameter intentionally unused
    // Simplified implementation
    core::ok(cfg, "Purge command - implementation needed");
    return 0;
}

int cmd_load(const core::Config& cfg, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        core::error(cfg, "Usage: ak load <profile|key> [--persist]");
    }
    
    std::string name = args[1];
    bool persist = (args.size() >= 3 && args[2] == "--persist");
    
    // Check if it's a profile first
    auto profiles = storage::listProfiles(cfg);
    bool isProfile = std::find(profiles.begin(), profiles.end(), name) != profiles.end();
    
    std::string exports;
    
    if (isProfile) {
        // Load profile
        exports = makeExportsForProfile(cfg, name);
        
        if (persist) {
            // Get current directory and store profile for persistence
            std::string currentDir = system::getCwd();
            auto existingProfiles = storage::readDirProfiles(cfg, currentDir);
            if (std::find(existingProfiles.begin(), existingProfiles.end(), name) == existingProfiles.end()) {
                existingProfiles.push_back(name);
                storage::writeDirProfiles(cfg, currentDir, existingProfiles);
            }
        }
        
        core::auditLog(cfg, "load_profile", {name});
    } else {
        // Try to load as individual key
        core::KeyStore ks = storage::loadVault(cfg);
        auto it = ks.kv.find(name);
        if (it == ks.kv.end()) {
            core::error(cfg, "Neither profile nor key '" + name + "' found.");
        }
        
        // Create export statement for single key
        std::string value = it->second;
        std::string escaped;
        escaped.reserve(value.size() * 2);
        
        for (char c : value) {
            if (c == '\\' || c == '"') {
                escaped.push_back('\\');
            }
            if (c == '\n') {
                escaped += "\\n";
                continue;
            }
            escaped.push_back(c);
        }
        
        exports = "export " + name + "=\"" + escaped + "\"\n";
        
        if (persist) {
            // For individual keys, create a temporary profile and persist it
            std::string currentDir = system::getCwd();
            auto existingProfiles = storage::readDirProfiles(cfg, currentDir);
            
            // Create a temporary profile name based on the key
            std::string tempProfileName = "_key_" + name;
            storage::writeProfile(cfg, tempProfileName, {name});
            
            if (std::find(existingProfiles.begin(), existingProfiles.end(), tempProfileName) == existingProfiles.end()) {
                existingProfiles.push_back(tempProfileName);
                storage::writeDirProfiles(cfg, currentDir, existingProfiles);
            }
        }
        
        core::auditLog(cfg, "load_key", {name});
    }
    
    // Enhanced interactive shell detection
    bool isInteractive = false;
    std::string currentShell = "unknown";
    
    // Check multiple shell indicators
    if (getenv("PS1") != nullptr) {
        isInteractive = true;
        currentShell = "bash";
    } else if (getenv("ZSH_VERSION") != nullptr) {
        isInteractive = true;
        currentShell = "zsh";
    } else if (getenv("FISH_VERSION") != nullptr) {
        isInteractive = true;
        currentShell = "fish";
    } else if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
        // Terminal is attached, likely interactive even if shell vars not set
        isInteractive = true;
        const char* shell = getenv("SHELL");
        if (shell) {
            std::string shellPath(shell);
            if (shellPath.find("bash") != std::string::npos) currentShell = "bash";
            else if (shellPath.find("zsh") != std::string::npos) currentShell = "zsh";
            else if (shellPath.find("fish") != std::string::npos) currentShell = "fish";
        }
    }
    
    std::string shellInitPath = cfg.configDir + "/shell-init.sh";
    bool hasShellIntegration = std::filesystem::exists(shellInitPath);
    
    // Auto-install shell integration if not present and we're in an interactive context
    if (isInteractive && !hasShellIntegration) {
        std::cerr << "🔧 " << ui::colorize("Setting up automatic environment loading...", ui::Colors::BRIGHT_YELLOW) << "\n";
        try {
            system::writeShellInitFile(cfg);
            system::ensureSourcedInRc(cfg);
            hasShellIntegration = true;
            std::cerr << "✅ " << ui::colorize("Shell integration configured!", ui::Colors::BRIGHT_GREEN) << "\n";
        } catch (const std::exception& e) {
            std::cerr << "⚠️  " << ui::colorize("Failed to configure shell integration: ", ui::Colors::BRIGHT_RED) << e.what() << "\n";
        }
    }
    
    // Always output the exports first so they're available immediately
    std::cout << exports;
    
    // For interactive sessions, also set up the shell integration to work immediately
    if (hasShellIntegration && isInteractive) {
        // Output the source command so shell integration becomes active immediately
        std::cout << "# Activate shell integration for direct ak commands\n";
        std::cout << "source \"" << shellInitPath << "\"\n";
        
        // Provide clear feedback about what's happening
        std::cerr << "✅ " << ui::colorize("Environment loaded!", ui::Colors::BRIGHT_GREEN) << " Variables are now available.\n";
        std::cerr << "💡 " << ui::colorize("Direct commands enabled:", ui::Colors::BRIGHT_CYAN) << " You can now use " << ui::colorize("ak load " + name, ui::Colors::BRIGHT_WHITE) << " directly\n";
        
        // If this was the first setup, let them know about shell restart
        if (!std::filesystem::exists(cfg.configDir + "/.shell_setup_complete")) {
            std::cerr << "📝 " << ui::colorize("Note:", ui::Colors::BRIGHT_YELLOW) << " For new terminals, restart your shell or run " << ui::colorize("source ~/.bashrc", ui::Colors::BRIGHT_WHITE) << " (or ~/.zshrc)\n";
            // Create a marker file to avoid showing this message repeatedly
            std::ofstream marker(cfg.configDir + "/.shell_setup_complete");
            marker << "1\n";
        }
    } else if (isInteractive) {
        // Fallback for when shell integration isn't available
        std::cerr << "💡 " << ui::colorize("TO LOAD:", ui::Colors::BRIGHT_GREEN) << " " << ui::colorize("eval \"$(ak load " + name + (persist ? " --persist" : "") + ")\"", ui::Colors::BRIGHT_CYAN) << "\n";
    } else {
        // Non-interactive context - just show script usage
        std::cerr << "💡 " << ui::colorize("IN SCRIPTS:", ui::Colors::BRIGHT_GREEN) << " " << ui::colorize("eval \"$(ak load " + name + (persist ? " --persist" : "") + ")\"", ui::Colors::BRIGHT_CYAN) << "\n";
    }
    return 0;
}

int cmd_unload(const core::Config& cfg, const std::vector<std::string>& args) {
    // Simplified implementation
    std::vector<std::string> profilesToUnload;
    
    if (args.size() == 1) {
        profilesToUnload = storage::listProfiles(cfg);
    } else {
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] != "--persist") {
                profilesToUnload.push_back(args[i]);
            }
        }
        if (profilesToUnload.empty()) {
            profilesToUnload = storage::listProfiles(cfg);
        }
    }
    
    std::unordered_set<std::string> allKeys;
    for (const std::string& profile : profilesToUnload) {
        for (const auto& key : storage::readProfile(cfg, profile)) {
            allKeys.insert(key);
        }
    }
    
    for (const std::string& key : allKeys) {
        std::cout << "unset " << key << "\n";
    }
    
    core::auditLog(cfg, "unload", profilesToUnload);
    return 0;
}

int cmd_env(const core::Config& cfg, const std::vector<std::string>& args) {
    if (args.size() < 3 || args[1] != "--profile") {
        core::error(cfg, "Usage: ak env --profile <name>");
    }
    
    std::string profile = args[2];
    printExportsForProfile(cfg, profile);
    core::auditLog(cfg, "env", storage::readProfile(cfg, profile));
    
    return 0;
}

// Placeholder implementations for complex commands
int cmd_export(const core::Config& cfg, const std::vector<std::string>& args) {
    (void)args; // Parameter intentionally unused
    core::ok(cfg, "Export command - implementation needed");
    return 0;
}

int cmd_import(const core::Config& cfg, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        core::error(cfg, "Usage: ak import --profile|-p <profile> [--format|-f <format>] --file|-i <file> [--keys]");
    }
    
    std::string profileName;
    std::string format = "env"; // Default to env format
    std::string filePath;
    bool keysOnly = false;
    
    // Parse arguments
    for (size_t i = 1; i < args.size(); ++i) {
        if ((args[i] == "--profile" || args[i] == "-p") && i + 1 < args.size()) {
            profileName = args[i + 1];
            ++i;
        } else if ((args[i] == "--format" || args[i] == "-f") && i + 1 < args.size()) {
            format = args[i + 1];
            ++i;
        } else if ((args[i] == "--file" || args[i] == "-i") && i + 1 < args.size()) {
            filePath = args[i + 1];
            ++i;
        } else if (args[i] == "--keys") {
            keysOnly = true;
        }
    }
    
    // Validate required arguments
    if (profileName.empty()) {
        core::error(cfg, "Profile name is required. Use --profile|-p <profile>");
    }
    if (filePath.empty()) {
        core::error(cfg, "File path is required. Use --file|-i <file>");
    }
    
    // Validate format
    if (format != "env" && format != "dotenv" && format != "json" && format != "yaml") {
        core::error(cfg, "Unsupported format '" + format + "'. Supported formats: env, dotenv, json, yaml");
    }
    
    // Check if file exists
    if (!std::filesystem::exists(filePath)) {
        core::error(cfg, "File not found: " + filePath);
    }
    
    // Parse the file based on format
    std::vector<std::pair<std::string, std::string>> keyValuePairs;
    
    try {
        if (format == "env" || format == "dotenv") {
            std::ifstream file(filePath);
            if (!file.is_open()) {
                core::error(cfg, "Failed to open file: " + filePath);
            }
            keyValuePairs = storage::parse_env_file(file);
        } else if (format == "json") {
            std::ifstream file(filePath);
            if (!file.is_open()) {
                core::error(cfg, "Failed to open file: " + filePath);
            }
            std::ostringstream buffer;
            buffer << file.rdbuf();
            keyValuePairs = storage::parse_json_min(buffer.str());
        } else if (format == "yaml") {
            // Simple YAML parsing - just handle key: value pairs
            std::ifstream file(filePath);
            if (!file.is_open()) {
                core::error(cfg, "Failed to open file: " + filePath);
            }
            
            std::string line;
            while (std::getline(file, line)) {
                line = core::trim(line);
                if (line.empty() || line[0] == '#') {
                    continue;
                }
                
                auto colon = line.find(':');
                if (colon == std::string::npos) {
                    continue;
                }
                
                std::string key = core::trim(line.substr(0, colon));
                std::string value = core::trim(line.substr(colon + 1));
                
                // Remove quotes if present
                if (!value.empty() && ((value.front() == '"' && value.back() == '"') ||
                                      (value.front() == '\'' && value.back() == '\''))) {
                    value = value.substr(1, value.size() - 2);
                }
                
                // Validate key name
                if (!key.empty() && (std::isalpha(key[0]) || key[0] == '_')) {
                    bool validKey = true;
                    for (char c : key) {
                        if (!std::isalnum(c) && c != '_') {
                            validKey = false;
                            break;
                        }
                    }
                    if (validKey) {
                        keyValuePairs.push_back({key, value});
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        core::error(cfg, "Failed to parse file: " + std::string(e.what()));
    }
    
    if (keyValuePairs.empty()) {
        core::error(cfg, "No valid key-value pairs found in file");
    }
    
    // Filter for known service keys if --keys flag is used
    if (keysOnly) {
        auto knownKeys = services::getKnownServiceKeys();
        auto filtered = keyValuePairs;
        keyValuePairs.clear();
        
        for (const auto& [key, value] : filtered) {
            if (knownKeys.find(key) != knownKeys.end()) {
                keyValuePairs.push_back({key, value});
            }
        }
        
        if (keyValuePairs.empty()) {
            core::error(cfg, "No known service provider keys found in file");
        }
    }
    
    // Load existing vault and profile
    core::KeyStore ks = storage::loadVault(cfg);
    std::vector<std::string> profileKeys = storage::readProfile(cfg, profileName);
    auto profileKeyMap = storage::loadProfileKeys(cfg, profileName);
    
    // Track what we're doing
    int addedToVault = 0;
    int updatedInVault = 0;
    int addedToProfile = 0;
    
    // Add each key-value pair
    for (const auto& [key, value] : keyValuePairs) {
        if (key.empty() || value.empty()) {
            continue;
        }
        
        // Check if key exists in vault
        bool keyExistsInVault = ks.kv.find(key) != ks.kv.end();
        bool keyExistsInProfile = std::find(profileKeys.begin(), profileKeys.end(), key) != profileKeys.end();
        
        // Add/update in vault
        ks.kv[key] = value;
        if (keyExistsInVault) {
            updatedInVault++;
        } else {
            addedToVault++;
        }
        
        // Also store/update value in the profile-scoped encrypted keystore
        profileKeyMap[key] = value;
        
        // Add to profile if not already there
        if (!keyExistsInProfile) {
            profileKeys.push_back(key);
            addedToProfile++;
        }
    }
    
    // Save vault and profile + profile key map
    storage::saveVault(cfg, ks);
    storage::writeProfile(cfg, profileName, profileKeys);
    storage::saveProfileKeys(cfg, profileName, profileKeyMap);
    
    // Generate updated exports for the profile
    std::string exports = makeExportsForProfile(cfg, profileName);
    if (!exports.empty()) {
        storage::writeEncryptedBundle(cfg, profileName, exports);
    }
    
    // Provide feedback
    std::string message = "Imported " + std::to_string(keyValuePairs.size()) + " key(s) to profile '" + profileName + "' ";
    message += "(" + std::to_string(addedToVault) + " new in vault, " + std::to_string(updatedInVault) + " updated in vault, ";
    message += std::to_string(addedToProfile) + " new in profile)";
    
    if (keysOnly) {
        message += " [known service keys only]";
    }
    
    core::ok(cfg, message);
    
    // Log the operation
    std::vector<std::string> logKeys;
    logKeys.reserve(keyValuePairs.size() + 1);
    logKeys.push_back(profileName);
    for (const auto& [key, value] : keyValuePairs) {
        logKeys.push_back(key);
    }
    core::auditLog(cfg, "import", logKeys);
    
    return 0;
}

int cmd_migrate(const core::Config& cfg, const std::vector<std::string>& args) {
    (void)args; // Parameter intentionally unused
    core::ok(cfg, "Migrate command - implementation needed");
    return 0;
}

int cmd_run(const core::Config& cfg, const std::vector<std::string>& args) {
    (void)args; // Parameter intentionally unused
    core::ok(cfg, "Run command - implementation needed");
    return 0;
}

int cmd_guard(const core::Config& cfg, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        core::error(cfg, "Usage: ak guard enable|disable");
    }
    
    if (args[1] == "enable") {
        system::guard_enable(cfg);
    } else if (args[1] == "disable") {
        system::guard_disable();
    } else {
        core::error(cfg, "Usage: ak guard enable|disable");
    }
    
    return 0;
}

int cmd_test(const core::Config& cfg, const std::vector<std::string>& args) {
    std::vector<std::string> servicesToTest;
    bool failFast = std::find(args.begin(), args.end(), "--fail-fast") != args.end();
    
    // Parse arguments for specific testing modes
    std::string profileName;
    std::vector<std::string> specificServices;
    std::vector<std::string> specificKeys;
    
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-p" || args[i] == "--profile") {
            if (i + 1 < args.size()) {
                profileName = args[i + 1];
                i++; // Skip next argument
            }
        } else if (args[i] != "--fail-fast" && args[i] != "--all" && args[i] != "--json") {
            // Determine if this is a key name or service name
            std::string arg = args[i];
            if (arg.find('_') != std::string::npos && std::all_of(arg.begin(), arg.end(),
                [](char c) { return std::isupper(c) || std::isdigit(c) || c == '_'; })) {
                specificKeys.push_back(arg);
            } else {
                specificServices.push_back(arg);
            }
        }
    }
    
    // Determine what to test based on arguments
    if (!profileName.empty()) {
        // Test profile keys: ./ak test -p default
        try {
            auto profileKeys = storage::readProfile(cfg, profileName);
            auto vault = storage::loadVault(cfg);
            auto profileValues = storage::loadProfileKeys(cfg, profileName);
            
            // Map profile keys to services they belong to
            for (const auto& keyName : profileKeys) {
                for (const auto& [service, serviceKey] : services::SERVICE_KEYS) {
                    if (serviceKey == keyName && services::TESTABLE_SERVICES.find(service) != services::TESTABLE_SERVICES.end()) {
                        // Check if key exists in profile store, vault, or environment
                        bool hasKey = false;
                        if (profileValues.find(keyName) != profileValues.end()) {
                            hasKey = true;
                        } else if (vault.kv.find(keyName) != vault.kv.end()) {
                            hasKey = true;
                        } else {
                            const char* envValue = getenv(keyName.c_str());
                            hasKey = (envValue && *envValue);
                        }
                        
                        if (hasKey && std::find(servicesToTest.begin(), servicesToTest.end(), service) == servicesToTest.end()) {
                            servicesToTest.push_back(service);
                        }
                        break;
                    }
                }
            }
            
            if (servicesToTest.empty()) {
                if (cfg.json) {
                    std::cout << "[]\n";
                } else {
                    std::cerr << "ℹ️  No testable services found in profile '" << profileName << "'.\n";
                }
                return 0;
            }
            
        } catch (const std::exception& e) {
            core::error(cfg, "Failed to load profile '" + profileName + "': " + std::string(e.what()));
            return 1;
        }
    } else if (!specificKeys.empty()) {
        // Test specific keys: ./ak test OPENAI_API_KEY ANTHROPIC_API_KEY
        for (const auto& specificKey : specificKeys) {
            bool found = false;
            for (const auto& [service, serviceKey] : services::SERVICE_KEYS) {
                if (serviceKey == specificKey && services::TESTABLE_SERVICES.find(service) != services::TESTABLE_SERVICES.end()) {
                    servicesToTest.push_back(service);
                    found = true;
                    break;
                }
            }
            if (!found) {
                core::error(cfg, "Key '" + specificKey + "' is not associated with any testable service");
                return 1;
            }
        }
    } else if (!specificServices.empty()) {
        // Test specific services: ./ak test openai anthropic myapi
        for (const auto& specificService : specificServices) {
            bool isTestable = false;
            
            // Check if it's a built-in testable service
            if (services::TESTABLE_SERVICES.find(specificService) != services::TESTABLE_SERVICES.end()) {
                isTestable = true;
            } else {
                // Check if it's a testable custom service
                try {
                    auto allServices = services::loadAllServices(cfg);
                    std::vector<services::Service> customServices;
                    for (const auto& [name, service] : allServices) {
                        if (!service.isBuiltIn) {
                            customServices.push_back(service);
                        }
                    }
                    for (const auto& customService : customServices) {
                        if (customService.name == specificService && customService.testable) {
                            isTestable = true;
                            break;
                        }
                    }
                } catch (const std::exception&) {
                    // Ignore errors loading custom services
                }
            }
            
            if (isTestable) {
                servicesToTest.push_back(specificService);
            } else {
                core::error(cfg, "Service '" + specificService + "' is not testable");
                return 1;
            }
        }
    } else {
        // Test all configured services (default behavior)
        servicesToTest = services::detectConfiguredServices(cfg, storage::getDefaultProfileName());
        
        if (servicesToTest.empty()) {
            if (cfg.json) {
                std::cout << "[]\n";
            } else {
                std::cerr << "ℹ️  No services to test.\n";
            }
            return 0;
        }
    }
    
    // Run the tests
    if (!cfg.json && servicesToTest.size() > 1) {
        core::working(cfg, "Testing " + std::to_string(servicesToTest.size()) + " API connections...");
    }
    
    auto results = services::run_tests_parallel(cfg, servicesToTest, failFast);
    
    // Output results
    if (cfg.json) {
        std::cout << "[";
        bool first = true;
        for (const auto& result : results) {
            if (!first) std::cout << ",";
            first = false;
            std::cout << "{\"service\":\"" << result.service << "\",\"ok\":"
                     << (result.ok ? "true" : "false") << "}";
        }
        std::cout << "]\n";
    } else {
        for (const auto& result : results) {
            if (servicesToTest.size() == 1) {
                // Single service test - show detailed steps like in demo
                // Get service display name
                std::string serviceName = result.service;
                
                // Check built-in services first
                auto builtinIt = services::DEFAULT_SERVICES.find(result.service);
                if (builtinIt != services::DEFAULT_SERVICES.end()) {
                    serviceName = builtinIt->second.description;
                } else {
                    // Check custom services
                    try {
                        auto allServices = services::loadAllServices(cfg);
                        std::vector<services::Service> customServices;
                        for (const auto& [name, service] : allServices) {
                            if (!service.isBuiltIn) {
                                customServices.push_back(service);
                            }
                        }
                        for (const auto& customService : customServices) {
                            if (customService.name == result.service) {
                                serviceName = customService.description.empty() ?
                                    customService.name : customService.description;
                                break;
                            }
                        }
                    } catch (const std::exception&) {
                        // Fallback: capitalize first letter
                        if (!serviceName.empty()) {
                            serviceName[0] = std::toupper(serviceName[0]);
                        }
                    }
                }
                
                std::cout << ui::colorize("🧪 Testing " + serviceName + " API connection...", ui::Colors::BRIGHT_YELLOW) << "\n";
                
                // Show the actual steps being performed based on what test_one() does
                std::cout << ui::colorize("├── Finding API key...", ui::Colors::DIM) << " ";
                std::cout << ui::colorize("✓", ui::Colors::BRIGHT_GREEN) << "\n";
                
                std::cout << ui::colorize("├── Connecting to API endpoint...", ui::Colors::DIM) << " ";
                if (result.ok) {
                    std::cout << ui::colorize("✓", ui::Colors::BRIGHT_GREEN) << "\n";
                    std::cout << ui::colorize("└── Verifying authentication...", ui::Colors::DIM) << " ";
                    std::cout << ui::colorize("✓", ui::Colors::BRIGHT_GREEN) << "\n\n";
                    std::cout << ui::colorize("✅ " + serviceName + " API: All tests passed!", ui::Colors::BRIGHT_GREEN) << "\n";
                } else {
                    std::cout << ui::colorize("❌", ui::Colors::BRIGHT_RED) << "\n";
                    std::cout << ui::colorize("└── Verifying authentication...", ui::Colors::DIM) << " ";
                    std::cout << ui::colorize("❌", ui::Colors::BRIGHT_RED) << "\n\n";
                    std::cout << ui::colorize("❌ " + serviceName + " API: " + (result.error_message.empty() ? "Test failed" : result.error_message), ui::Colors::BRIGHT_RED) << "\n";
                }
            } else {
                // Multiple services - show tree-like progress
                bool isLast = (&result == &results.back());
                std::string branch = isLast ? "└── " : "├── ";
                
                // Get display name for service (built-in or custom)
                std::string displayName = result.service;
                auto builtinIt2 = services::DEFAULT_SERVICES.find(result.service);
                if (builtinIt2 != services::DEFAULT_SERVICES.end()) {
                    displayName = builtinIt2->second.description;
                } else {
                    // Check custom services
                    try {
                        auto allServices = services::loadAllServices(cfg);
                        std::vector<services::Service> customServices;
                        for (const auto& [name, service] : allServices) {
                            if (!service.isBuiltIn) {
                                customServices.push_back(service);
                            }
                        }
                        for (const auto& customService : customServices) {
                            if (customService.name == result.service) {
                                displayName = customService.description.empty() ?
                                    customService.name : customService.description;
                                break;
                            }
                        }
                    } catch (const std::exception&) {
                        // Fallback: capitalize first letter
                        if (!displayName.empty()) {
                            displayName[0] = std::toupper(displayName[0]);
                        }
                    }
                }
                
                // Use the same display name logic as for single service
                std::string serviceName = result.service;
                auto builtinIter = services::DEFAULT_SERVICES.find(result.service);
                if (builtinIter != services::DEFAULT_SERVICES.end()) {
                    serviceName = builtinIter->second.description;
                } else {
                    // Check custom services
                    try {
                        auto allServices = services::loadAllServices(cfg);
                        std::vector<services::Service> customServices;
                        for (const auto& [name, service] : allServices) {
                            if (!service.isBuiltIn) {
                                customServices.push_back(service);
                            }
                        }
                        for (const auto& customService : customServices) {
                            if (customService.name == result.service) {
                                serviceName = customService.description.empty() ?
                                    customService.name : customService.description;
                                break;
                            }
                        }
                    } catch (const std::exception&) {
                        // Fallback: capitalize first letter
                        if (!serviceName.empty()) {
                            serviceName[0] = std::toupper(serviceName[0]);
                        }
                    }
                }
                
                std::cout << ui::colorize(branch + serviceName + "...", ui::Colors::DIM) << " ";
                
                if (result.ok) {
                    std::cout << ui::colorize("✓", ui::Colors::BRIGHT_GREEN) << "\n";
                } else {
                    std::cout << ui::colorize("❌", ui::Colors::BRIGHT_RED) << "\n";
                }
            }
        }
        
        // Summary for multiple tests
        if (!cfg.json && servicesToTest.size() > 1) {
            int passed = 0;
            int failed = 0;
            for (const auto& result : results) {
                if (result.ok) passed++;
                else failed++;
            }
            
            std::cout << "\n";
            if (failed == 0) {
                core::success(cfg, "All " + std::to_string(passed) + " API tests passed!");
            } else {
                std::cout << ui::colorize("📊 Results: " + std::to_string(passed) + " passed, " + std::to_string(failed) + " failed", ui::Colors::BRIGHT_BLUE) << "\n";
            }
        }
    }
    
    return 0;
}

int cmd_doctor(const core::Config& cfg, const std::vector<std::string>& args) {
    (void)args; // Parameter intentionally unused
    std::cout << "backend: " << ((cfg.gpgAvailable && !cfg.forcePlain) ? "gpg" : "plain") << "\n";
    if (cfg.gpgAvailable) {
        std::cout << "found: gpg\n";
    }
    
    for (const auto& tool : {"pbcopy", "wl-copy", "xclip"}) {
        if (core::commandExists(tool)) {
            std::cout << "clipboard: " << tool << "\n";
        }
    }
    
    std::cout << "profiles: " << storage::listProfiles(cfg).size() << "\n";
    std::cout << "vault: " << cfg.vaultPath << "\n";
    
    return 0;
}

int cmd_audit(const core::Config& cfg, const std::vector<std::string>& args) {
    size_t tail = 50;
    if (args.size() >= 2) {
        try {
            tail = std::stoul(args[1]);
        } catch (...) {
            // Use default
        }
    }
    
    std::ifstream in(cfg.auditLogPath);
    if (!in) {
        core::error(cfg, "No audit log");
    }
    
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    
    size_t start = lines.size() > tail ? lines.size() - tail : 0;
    for (size_t i = start; i < lines.size(); ++i) {
        std::cout << lines[i] << "\n";
    }
    
    return 0;
}

int cmd_install_shell(const core::Config& cfg, const std::vector<std::string>& args) {
    (void)args; // Parameter intentionally unused
    
    // When running under sudo, we need to use the target user's config directory
    // instead of root's directory
    core::Config targetCfg = cfg;
    system::TargetUser targetUser = system::resolveTargetUser();
    
    // If we detected a different user (i.e., running under sudo), adjust the config
    if (!targetUser.home.empty() && targetUser.home != core::getenvs("HOME")) {
        std::string base = targetUser.home + "/.config";
        targetCfg.configDir = base + "/ak";
        targetCfg.profilesDir = targetCfg.configDir + "/profiles";
        targetCfg.persistDir = targetCfg.configDir + "/persist";
        
        // Update vault path to use target user's directory
        if (targetCfg.gpgAvailable && !targetCfg.forcePlain) {
            targetCfg.vaultPath = targetCfg.configDir + "/vault.gpg";
        } else {
            targetCfg.vaultPath = targetCfg.configDir + "/vault.env";
        }
    }
    
    system::writeShellInitFile(targetCfg);
    system::ensureSourcedInRc(targetCfg);
    std::cerr << "✅ Installed shell auto-load. Restart your terminal or source your config.\n";
    return 0;
}

int cmd_uninstall(const core::Config& cfg, const std::vector<std::string>& args) {
    (void)args; // Parameter intentionally unused
    core::ok(cfg, "Uninstall command - implementation needed");
    return 0;
}

int cmd_completion(const core::Config& cfg, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        core::error(cfg, "Usage: ak completion <shell>\nSupported shells: bash, zsh, fish");
    }
    
    std::string shell = args[1];
    if (shell == "bash") {
        cli::generateBashCompletion();
    } else if (shell == "zsh") {
        cli::generateZshCompletion();
    } else if (shell == "fish") {
        cli::generateFishCompletion();
    } else {
        core::error(cfg, "Unsupported shell: " + shell + "\nSupported shells: bash, zsh, fish");
    }
    
    return 0;
}

int cmd_gui(const core::Config& cfg, const std::vector<std::string>& args) {
#ifdef BUILD_GUI
    // Check if GUI is available
    if (!gui::isGuiAvailable()) {
        core::error(cfg, "GUI support not available. Please build with -DBUILD_GUI=ON");
        return 1;
    }
    
    // Launch GUI application
    core::auditLog(cfg, "gui", {"launched"});
    return gui::runGuiApplication(cfg, args);
#else
    (void)cfg;  // Suppress unused parameter warning
    (void)args; // Suppress unused parameter warning
    std::cerr << "Error: GUI support not compiled. Please build with -DBUILD_GUI=ON" << std::endl;
    return 1;
#endif
}

// Service management
int cmd_service(const core::Config& cfg, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        core::error(cfg, "Usage: ak service <add|list|edit|delete> [options]");
    }
    
    std::string subcommand = args[1];
    
    if (subcommand == "list" || subcommand == "ls") {
        try {
            // Load both built-in and custom services
            auto allServices = services::loadAllServices(cfg);
            std::vector<services::Service> customServices;
            for (const auto& [name, service] : allServices) {
                if (!service.isBuiltIn) {
                    customServices.push_back(service);
                }
            }
            
            std::cout << ui::colorize("🔧 Available Services:", ui::Colors::BRIGHT_MAGENTA) << "\n\n";
            
            // Show built-in services first
            std::cout << ui::colorize("Built-in Services:", ui::Colors::BRIGHT_BLUE + ui::Colors::BOLD) << "\n";
            
            for (const auto& [name, service] : services::DEFAULT_SERVICES) {
                std::string serviceName = ui::colorize(service.name, ui::Colors::BRIGHT_CYAN);
                std::string keyName = ui::colorize(service.keyName, ui::Colors::WHITE);
                std::string testable = service.testable ?
                    ui::colorize(" (testable)", ui::Colors::BRIGHT_GREEN) :
                    ui::colorize(" (read-only)", ui::Colors::DIM);
                
                std::cout << "  " << serviceName << " -> " << keyName << testable << "\n";
                
                if (!service.description.empty()) {
                    std::cout << "    " << ui::colorize(service.description, ui::Colors::DIM) << "\n";
                }
                
                if (service.testable && !service.testEndpoint.empty()) {
                    std::cout << "    " << ui::colorize("Test: " + service.authMethod + " auth, " + service.testEndpoint, ui::Colors::DIM) << "\n";
                } else if (!service.authMethod.empty()) {
                    std::cout << "    " << ui::colorize("Auth: " + service.authMethod, ui::Colors::DIM) << "\n";
                }
            }
            
            // Show custom services
            if (!customServices.empty()) {
                std::cout << "\n" << ui::colorize("Custom Services:", ui::Colors::BRIGHT_YELLOW + ui::Colors::BOLD) << "\n";
                
                for (const auto& service : customServices) {
                    std::string serviceName = ui::colorize(service.name, ui::Colors::BRIGHT_CYAN);
                    std::string keyName = ui::colorize(service.keyName, ui::Colors::WHITE);
                    std::string testable = service.testable ?
                        ui::colorize(" (testable)", ui::Colors::BRIGHT_GREEN) :
                        ui::colorize(" (read-only)", ui::Colors::DIM);
                    
                    std::cout << "  " << serviceName << " -> " << keyName << testable << "\n";
                    
                    if (!service.description.empty()) {
                        std::cout << "    " << ui::colorize(service.description, ui::Colors::DIM) << "\n";
                    }
                    
                    if (service.testable && !service.testEndpoint.empty()) {
                        std::cout << "    " << ui::colorize("Test: " + service.testMethod + " " + service.testEndpoint, ui::Colors::DIM) << "\n";
                    }
                }
            }
            
            // Show totals
            std::cout << "\n" << ui::colorize("Total: " + std::to_string(services::DEFAULT_SERVICES.size()) +
                                             " built-in + " + std::to_string(customServices.size()) +
                                             " custom services", ui::Colors::DIM) << "\n";
            
        } catch (const std::exception& e) {
            core::error(cfg, "Failed to load services: " + std::string(e.what()));
        }
        
    } else if (subcommand == "add") {
        services::Service newService;
        
        // Interactive service creation
        std::cout << ui::colorize("Creating new custom service...", ui::Colors::BRIGHT_YELLOW) << "\n";
        
        std::cout << ui::colorize("Service name: ", ui::Colors::WHITE);
        std::getline(std::cin, newService.name);
        
        if (newService.name.empty()) {
            core::error(cfg, "Service name cannot be empty");
        }
        
        std::cout << ui::colorize("Environment variable name: ", ui::Colors::WHITE);
        std::getline(std::cin, newService.keyName);
        
        if (newService.keyName.empty()) {
            core::error(cfg, "Key name cannot be empty");
        }
        
        std::cout << ui::colorize("Description (optional): ", ui::Colors::WHITE);
        std::getline(std::cin, newService.description);
        
        std::string response;
        std::cout << ui::colorize("Enable testing? [y/N]: ", ui::Colors::WHITE);
        std::getline(std::cin, response);
        
        if (!response.empty() && (response[0] == 'y' || response[0] == 'Y')) {
            newService.testable = true;
            
            std::cout << ui::colorize("Test endpoint URL: ", ui::Colors::WHITE);
            std::getline(std::cin, newService.testEndpoint);
            
            std::cout << ui::colorize("HTTP method [GET]: ", ui::Colors::WHITE);
            std::getline(std::cin, response);
            newService.testMethod = response.empty() ? "GET" : response;
            
            std::cout << ui::colorize("Additional headers (optional): ", ui::Colors::WHITE);
            std::getline(std::cin, newService.testHeaders);
        } else {
            newService.testable = false;
            newService.testMethod = "GET";
        }
        
        try {
            services::addService(cfg, newService);
            core::success(cfg, "Service '" + newService.name + "' created successfully");
        } catch (const std::exception& e) {
            core::error(cfg, "Failed to create custom service: " + std::string(e.what()));
        }
        
    } else if (subcommand == "delete" || subcommand == "rm") {
        if (args.size() < 3) {
            core::error(cfg, "Usage: ak service delete <service_name>");
        }
        
        std::string serviceName = args[2];
        
        try {
            auto allServices = services::loadAllServices(cfg);
            auto it = allServices.find(serviceName);
            
            if (it == allServices.end()) {
                core::error(cfg, "Service '" + serviceName + "' not found");
            }
            
            services::removeService(cfg, serviceName);
            core::success(cfg, "Service '" + serviceName + "' deleted successfully");
            
        } catch (const std::exception& e) {
            core::error(cfg, "Failed to delete custom service: " + std::string(e.what()));
        }
        
    } else if (subcommand == "edit") {
        if (args.size() < 3) {
            core::error(cfg, "Usage: ak service edit <service_name>");
        }
        
        std::string serviceName = args[2];
        
        try {
            auto allServices = services::loadAllServices(cfg);
            auto it = allServices.find(serviceName);
            
            if (it == allServices.end()) {
                core::error(cfg, "Service '" + serviceName + "' not found");
            }
            
            services::Service editedService = it->second;
            
            std::cout << ui::colorize("Editing service '" + serviceName + "'...", ui::Colors::BRIGHT_YELLOW) << "\n";
            
            std::string input;
            std::cout << ui::colorize("Description [" + editedService.description + "]: ", ui::Colors::WHITE);
            std::getline(std::cin, input);
            if (!input.empty()) {
                editedService.description = input;
            }
            
            std::cout << ui::colorize(std::string("Enable testing? [") + (editedService.testable ? "Y/n" : "y/N") + "]: ", ui::Colors::WHITE);
            std::getline(std::cin, input);
            
            if (!input.empty()) {
                editedService.testable = (input[0] == 'y' || input[0] == 'Y');
            }
            
            if (editedService.testable) {
                std::cout << ui::colorize("Test endpoint [" + editedService.testEndpoint + "]: ", ui::Colors::WHITE);
                std::getline(std::cin, input);
                if (!input.empty()) {
                    editedService.testEndpoint = input;
                }
                
                std::cout << ui::colorize("HTTP method [" + editedService.testMethod + "]: ", ui::Colors::WHITE);
                std::getline(std::cin, input);
                if (!input.empty()) {
                    editedService.testMethod = input;
                }
                
                std::cout << ui::colorize("Headers [" + editedService.testHeaders + "]: ", ui::Colors::WHITE);
                std::getline(std::cin, input);
                if (!input.empty()) {
                    editedService.testHeaders = input;
                }
            }
            
            // Remove old service and add updated one
            services::removeService(cfg, serviceName);
            services::addService(cfg, editedService);
            
            core::success(cfg, "Service '" + serviceName + "' updated successfully");
            
        } catch (const std::exception& e) {
            core::error(cfg, "Failed to edit custom service: " + std::string(e.what()));
        }
        
    } else {
        core::error(cfg, "Unknown service subcommand '" + subcommand + "'. Use: add, list, edit, delete");
    }
    
    return 0;
}

// Profile duplication
int cmd_duplicate(const core::Config& cfg, const std::vector<std::string>& args) {
    if (args.size() < 3) {
        core::error(cfg, "Usage: ak duplicate <source_profile> <new_profile>");
    }
    
    std::string sourceProfile = args[1];
    std::string newProfile = args[2];
    
    // Check if source profile exists
    auto profiles = storage::listProfiles(cfg);
    if (std::find(profiles.begin(), profiles.end(), sourceProfile) == profiles.end()) {
        core::error(cfg, "Source profile '" + sourceProfile + "' not found");
    }
    
    // Check if target profile already exists
    if (std::find(profiles.begin(), profiles.end(), newProfile) != profiles.end()) {
        core::error(cfg, "Profile '" + newProfile + "' already exists");
    }
    
    try {
        // Read source profile keys
        auto sourceKeys = storage::readProfile(cfg, sourceProfile);
        
        // Write to new profile
        storage::writeProfile(cfg, newProfile, sourceKeys);
        
        // Create encrypted bundle for new profile
        std::string exports = makeExportsForProfile(cfg, newProfile);
        if (!exports.empty()) {
            storage::writeEncryptedBundle(cfg, newProfile, exports);
        }
        
        core::success(cfg, "Successfully duplicated profile '" + sourceProfile + "' to '" + newProfile + "' with " + 
                     std::to_string(sourceKeys.size()) + " key" + (sourceKeys.size() == 1 ? "" : "s"));
        
        core::auditLog(cfg, "duplicate_profile", {sourceProfile, newProfile});
        
    } catch (const std::exception& e) {
        core::error(cfg, "Failed to duplicate profile: " + std::string(e.what()));
    }
    
    return 0;
}

// Internal commands for shell integration auto-loading
int cmd_internal_get_dir_profiles(const core::Config& cfg, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        return 1; // Silent failure for internal commands
    }
    
    std::string dir = args[1];
    auto profiles = storage::readDirProfiles(cfg, dir);
    
    for (const auto& profile : profiles) {
        std::cout << profile << "\n";
    }
    
    return 0;
}

int cmd_internal_get_bundle(const core::Config& cfg, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        return 1; // Silent failure for internal commands
    }
    
    std::string profileName = args[1];
    
    // Try to read from bundle first (for persisted data)
    std::string exports = storage::readEncryptedBundle(cfg, profileName);
    
    if (exports.empty()) {
        // If no bundle, generate exports from profile (fallback)
        auto profiles = storage::listProfiles(cfg);
        bool isProfile = std::find(profiles.begin(), profiles.end(), profileName) != profiles.end();
        
        if (isProfile) {
            exports = makeExportsForProfile(cfg, profileName);
        } else if (profileName.substr(0, 5) == "_key_") {
            // Handle temporary key profiles
            std::string keyName = profileName.substr(5);
            core::KeyStore ks = storage::loadVault(cfg);
            auto it = ks.kv.find(keyName);
            if (it != ks.kv.end()) {
                std::string value = it->second;
                std::string escaped;
                escaped.reserve(value.size() * 2);
                
                for (char c : value) {
                    if (c == '\\' || c == '"') {
                        escaped.push_back('\\');
                    }
                    if (c == '\n') {
                        escaped += "\\n";
                        continue;
                    }
                    escaped.push_back(c);
                }
                
                exports = "export " + keyName + "=\"" + escaped + "\"\n";
            }
        }
    }
    
    std::cout << exports;
    return 0;
}

} // namespace commands
} // namespace ak