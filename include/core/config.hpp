#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace ak {
namespace core {

// Version
extern const std::string AK_VERSION;

// Configuration structure
struct Config {
    std::string configDir;       // $XDG_CONFIG_HOME/ak or $HOME/.config/ak
    std::string vaultPath;       // keys.env.gpg or keys.env
    std::string profilesDir;     // profiles directory
    bool gpgAvailable = false;
    bool json = false;
    bool forcePlain = false;     // AK_DISABLE_GPG
    std::string presetPassphrase; // AK_PASSPHRASE
    std::string auditLogPath;
    std::string instanceId;
    std::string persistDir;
};

// KeyStore structure
struct KeyStore {
    std::unordered_map<std::string, std::string> kv;
};

// Utility functions
bool commandExists(const std::string& cmd);
std::string getenvs(const char* key, const std::string& defaultValue = "");
std::string trim(const std::string& str);
std::string toLower(std::string str);
bool icontains(const std::string& haystack, const std::string& needle);
std::string maskValue(const std::string& value);

// Error handling and output
void error(const Config& cfg, const std::string& msg, int code = 1);
void ok(const Config& cfg, const std::string& msg);
void warn(const Config& cfg, const std::string& msg);

// Audit logging
std::string hashKeyName(const std::string& name);
std::string isoTimeUTC();
void auditLog(const Config& cfg, const std::string& action, const std::vector<std::string>& keys);

// Instance management
std::string loadOrCreateInstanceId(const Config& cfg);

} // namespace core
} // namespace ak