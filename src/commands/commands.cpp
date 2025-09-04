#include "commands/commands.hpp"
#include "core/config.hpp"
#include "storage/vault.hpp"
#include "system/system.hpp"
#include "services/services.hpp"
#include "ui/ui.hpp"
#include "cli/cli.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <unordered_set>
#include <filesystem>

namespace ak {
namespace commands {

// Utility functions
std::string makeExportsForProfile(const core::Config& cfg, const std::string& name) {
    auto keys = storage::readProfile(cfg, name);
    core::KeyStore ks = storage::loadVault(cfg);
    std::ostringstream oss;
    
    for (const auto& key : keys) {
        auto it = ks.kv.find(key);
        if (it == ks.kv.end()) {
            continue;
        }
        
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
    
    // Add to vault
    core::KeyStore ks = storage::loadVault(cfg);
    bool keyExistsInVault = ks.kv.find(name) != ks.kv.end();
    ks.kv[name] = value;
    storage::saveVault(cfg, ks);
    
    // Add to profile if specified
    if (!profileName.empty()) {
        // Read existing profile keys
        std::vector<std::string> profileKeys = storage::readProfile(cfg, profileName);
        
        // Check if key already exists in profile
        auto it = std::find(profileKeys.begin(), profileKeys.end(), name);
        bool keyExistsInProfile = (it != profileKeys.end());
        
        if (!keyExistsInProfile) {
            profileKeys.push_back(name);
            storage::writeProfile(cfg, profileName, profileKeys);
        }
        
        // Provide appropriate feedback based on whether key existed
        if (keyExistsInVault && keyExistsInProfile) {
            core::ok(cfg, "Updated " + name + " in vault and profile '" + profileName + "'.");
        } else if (keyExistsInVault && !keyExistsInProfile) {
            core::ok(cfg, "Updated " + name + " in vault and added to profile '" + profileName + "'.");
        } else if (!keyExistsInVault && keyExistsInProfile) {
            core::ok(cfg, "Added " + name + " to vault (already in profile '" + profileName + "').");
        } else {
            core::ok(cfg, "Added " + name + " to vault and profile '" + profileName + "'.");
        }
        
        core::auditLog(cfg, keyExistsInVault ? "update_profile" : "add_profile", {name, profileName});
    } else {
        // Provide appropriate feedback for vault-only operations
        if (keyExistsInVault) {
            core::ok(cfg, "Updated " + name + " in vault.");
        } else {
            core::ok(cfg, "Added " + name + " to vault.");
        }
        core::auditLog(cfg, keyExistsInVault ? "update" : "add", {name});
    }
    
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
    
    core::ok(cfg, "Stored " + name + ".");
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
        for (const auto& name : names) {
            std::cout << std::left << std::setw(34) << name << " " << core::maskValue(ks.kv[name]) << "\n";
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
            core::ok(cfg, "Removed profile '" + profileName + "'.");
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
    core::ok(cfg, "Removed " + name + ".");
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
    
    std::sort(hits.begin(), hits.end());
    for (const auto& hit : hits) {
        std::cout << hit << "\n";
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
    
    core::ok(cfg, "Copied " + name + " to clipboard.");
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
    core::ok(cfg, "Saved profile '" + profile + "' (" + std::to_string(names.size()) + " keys).");
    core::auditLog(cfg, "save_profile", names);
    
    return 0;
}

int cmd_profiles(const core::Config& cfg, const std::vector<std::string>& args) {
    (void)args; // Parameter intentionally unused
    for (const auto& name : storage::listProfiles(cfg)) {
        std::cout << name << "\n";
    }
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
    
    std::cout << exports;
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
    (void)args; // Parameter intentionally unused
    core::ok(cfg, "Import command - implementation needed");
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
    std::vector<std::string> servicesToTest = services::detectConfiguredServices(cfg);
    
    if (servicesToTest.empty()) {
        if (cfg.json) {
            std::cout << "[]\n";
        } else {
            std::cerr << "ℹ️  No services to test.\n";
        }
        return 0;
    }
    
    bool failFast = std::find(args.begin(), args.end(), "--fail-fast") != args.end();
    auto results = services::run_tests_parallel(cfg, servicesToTest, failFast);
    
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
            if (result.ok) {
                std::cout << ui::colorize("✅ PASS", ui::Colors::BRIGHT_GREEN) << " " << result.service << "\n";
            } else {
                // Print provider and error message on the same line
                std::cout << ui::colorize("❌ FAIL", ui::Colors::BRIGHT_RED) << " " << result.service
                          << ", " << (result.error_message.empty() ? "unknown error" : result.error_message) << " ...\n";
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
    system::writeShellInitFile(cfg);
    system::ensureSourcedInRc(cfg);
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

} // namespace commands
} // namespace ak