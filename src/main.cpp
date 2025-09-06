#include "core/config.hpp"
#include "commands/commands.hpp"
#include "storage/vault.hpp"
#include "system/system.hpp"
#include "cli/cli.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <functional>

using namespace ak;

int main(int argc, char** argv) {
    // Initialize configuration
    core::Config cfg;
    std::string base = core::getenvs("XDG_CONFIG_HOME", core::getenvs("HOME") + "/.config");
    cfg.configDir = base + "/ak";
    cfg.profilesDir = cfg.configDir + "/profiles";
    cfg.gpgAvailable = core::commandExists("gpg");
    
    if (getenv("AK_DISABLE_GPG")) {
        cfg.forcePlain = true;
    }
    if (getenv("AK_PASSPHRASE")) {
        cfg.presetPassphrase = core::getenvs("AK_PASSPHRASE");
    }
    if (cfg.forcePlain) {
        cfg.gpgAvailable = false;
    }
    
    cfg.vaultPath = cfg.configDir + ((cfg.gpgAvailable && !cfg.forcePlain) ? "/keys.env.gpg" : "/keys.env");
    cfg.auditLogPath = cfg.configDir + "/audit.log";
    
    system::ensureSecureDir(cfg.configDir);
    cfg.instanceId = core::loadOrCreateInstanceId(cfg);
    cfg.persistDir = cfg.configDir + "/persist";
    
    // Parse global flags - first expand short flags
    std::vector<std::string> rawArgs;
    rawArgs.reserve(argc - 1);
    for (int i = 1; i < argc; ++i) {
        rawArgs.push_back(argv[i]);
    }
    
    // Expand short flags
    std::vector<std::string> expandedArgs = cli::expandShortFlags(rawArgs);
    
    // Process global flags
    std::vector<std::string> args;
    args.reserve(expandedArgs.size());
    for (const auto& arg : expandedArgs) {
        if (arg == "--json") {
            cfg.json = true;
        } else {
            args.push_back(arg);
        }
    }
    
    // Check if no arguments provided - launch GUI if available, otherwise welcome
    std::string cmd;
    if (args.empty()) {
#ifdef BUILD_GUI
        cmd = "gui";
#else
        cmd = "welcome";
#endif
    } else {
        cmd = args[0];
    }
    
    // Ensure default profile exists
    storage::ensureDefaultProfile(cfg);
    
    // Command dispatch table
    std::map<std::string, commands::CommandHandler> commandMap = {
        {"welcome", commands::cmd_welcome},
        {"help", commands::cmd_help},
        {"--help", commands::cmd_help},
        {"-h", commands::cmd_help},
        {"version", commands::cmd_version},
        {"--version", commands::cmd_version},
        {"-v", commands::cmd_version},
        {"backend", commands::cmd_backend},
        
        // Secret management
        {"add", commands::cmd_add},
        {"set", commands::cmd_set},
        {"get", commands::cmd_get},
        {"ls", commands::cmd_ls},
        {"rm", commands::cmd_rm},
        {"search", commands::cmd_search},
        {"cp", commands::cmd_cp},
        {"purge", commands::cmd_purge},
        
        // Profile management
        {"save", commands::cmd_save},
        {"load", commands::cmd_load},
        {"unload", commands::cmd_unload},
        {"profiles", commands::cmd_profiles},
        {"env", commands::cmd_env},
        
        // Export/Import
        {"export", commands::cmd_export},
        {"import", commands::cmd_import},
        {"migrate", commands::cmd_migrate},
        
        // Utilities
        {"run", commands::cmd_run},
        {"guard", commands::cmd_guard},
        {"test", commands::cmd_test},
        {"doctor", commands::cmd_doctor},
        {"audit", commands::cmd_audit},
        
        // System
        {"install-shell", commands::cmd_install_shell},
        {"uninstall", commands::cmd_uninstall},
        {"completion", commands::cmd_completion},
        
        // Service Management
        {"service", commands::cmd_service},
        
        // Profile Management Extensions
        {"duplicate", commands::cmd_duplicate},
        
        // GUI
        {"gui", commands::cmd_gui}
    };
    
    // Dispatch to appropriate command handler
    auto it = commandMap.find(cmd);
    if (it != commandMap.end()) {
        return it->second(cfg, args);
    } else {
        core::error(cfg, "Unknown command '" + cmd + "' (try: ak help)");
        return 1;
    }
}