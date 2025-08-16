#pragma once

#include "core/config.hpp"
#include <string>
#include <vector>

namespace ak {
namespace commands {

// Command handler type definition
using CommandHandler = int(*)(const core::Config& cfg, const std::vector<std::string>& args);

// Individual command handlers
int cmd_set(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_get(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_ls(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_rm(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_search(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_cp(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_purge(const core::Config& cfg, const std::vector<std::string>& args);

int cmd_save(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_load(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_unload(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_profiles(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_env(const core::Config& cfg, const std::vector<std::string>& args);

int cmd_export(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_import(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_migrate(const core::Config& cfg, const std::vector<std::string>& args);

int cmd_run(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_guard(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_test(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_doctor(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_audit(const core::Config& cfg, const std::vector<std::string>& args);

int cmd_install_shell(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_uninstall(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_completion(const core::Config& cfg, const std::vector<std::string>& args);

int cmd_version(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_backend(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_help(const core::Config& cfg, const std::vector<std::string>& args);
int cmd_welcome(const core::Config& cfg, const std::vector<std::string>& args);

// Utility functions
std::string makeExportsForProfile(const core::Config& cfg, const std::string& name);
void printExportsForProfile(const core::Config& cfg, const std::string& name);
void printUnsetsForProfile(const core::Config& cfg, const std::string& name);

} // namespace commands
} // namespace ak