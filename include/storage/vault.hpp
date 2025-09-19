#pragma once

#include "core/config.hpp"
#include <string>
#include <vector>
#include <filesystem>

namespace ak {
namespace storage {

// Vault operations (legacy - for migration)
core::KeyStore loadVault(const core::Config& cfg);
void saveVault(const core::Config& cfg, const core::KeyStore& ks);

// Profile-specific vault operations
std::map<std::string, std::string> loadProfileKeys(const core::Config& cfg, const std::string& profileName);
void saveProfileKeys(const core::Config& cfg, const std::string& profileName, const std::map<std::string, std::string>& keys);

// Profile operations
std::filesystem::path profilePath(const core::Config& cfg, const std::string& name);
std::filesystem::path profileKeysPath(const core::Config& cfg, const std::string& name);
std::vector<std::string> listProfiles(const core::Config& cfg);
std::vector<std::string> readProfile(const core::Config& cfg, const std::string& name);
void writeProfile(const core::Config& cfg, const std::string& name, const std::vector<std::string>& keys);
std::map<std::string, std::string> readProfileKeys(const core::Config& cfg, const std::string& name);
void writeProfileKeys(const core::Config& cfg, const std::string& name, const std::map<std::string, std::string>& keys);

// Persistence operations
std::string mappingFileForDir(const core::Config& cfg, const std::string& dir);
std::vector<std::string> readDirProfiles(const core::Config& cfg, const std::string& dir);
void writeDirProfiles(const core::Config& cfg, const std::string& dir, const std::vector<std::string>& profiles);

// Bundle operations
std::string bundleFile(const core::Config& cfg, const std::string& profile);
void writeEncryptedBundle(const core::Config& cfg, const std::string& profile, const std::string& exports);
std::string readEncryptedBundle(const core::Config& cfg, const std::string& profile);

// Import/Export helpers
std::vector<std::pair<std::string, std::string>> parse_env_file(std::istream& input);
std::vector<std::pair<std::string, std::string>> parse_json_min(const std::string& text);

// Default profile management
void ensureDefaultProfile(const core::Config& cfg);
std::string getDefaultProfileName();
void setDefaultProfile(const core::Config& cfg, const std::string& profileName);

// Migration utilities
void migrateGlobalVaultToProfiles(const core::Config& cfg);
bool hasGlobalVault(const core::Config& cfg);

} // namespace storage
} // namespace ak