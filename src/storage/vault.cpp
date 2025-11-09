#include "storage/vault.hpp"
#include "crypto/crypto.hpp"
#include "system/system.hpp"
#include "core/config.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_set>
#include <filesystem>
#ifdef __unix__
#include <sys/stat.h>
#endif

namespace ak {
namespace storage {

namespace fs = std::filesystem;

// Vault operations
core::KeyStore loadVault(const core::Config& cfg) {
    core::KeyStore ks;
    if (!fs::exists(cfg.vaultPath)) {
        return ks;
    }
    
    std::string data;
    if (cfg.gpgAvailable && !cfg.forcePlain && 
        cfg.vaultPath.size() > 4 && 
        cfg.vaultPath.substr(cfg.vaultPath.size() - 4) == ".gpg") {
        
        int rc = 0;
        if (!cfg.presetPassphrase.empty()) {
            auto pfile = fs::path(cfg.configDir) / ".pass.tmp";
            {
                std::ofstream pf(pfile, std::ios::trunc);
                pf << cfg.presetPassphrase;
                pf.close();
            }
#ifdef __unix__
            ::chmod(pfile.c_str(), 0600);
#endif
            std::string cmd = "gpg --batch --yes --quiet --pinentry-mode loopback --passphrase-file '" + 
                             pfile.string() + "' --decrypt '" + cfg.vaultPath + "' 2>/dev/null";
            data = system::runCmdCapture(cmd, &rc);
            fs::remove(pfile);
        } else {
            data = system::runCmdCapture("gpg --quiet --decrypt '" + cfg.vaultPath + "' 2>/dev/null", &rc);
        }
        
        if (rc != 0) {
            std::cerr << "⚠️  Failed to decrypt vault\n";
            return ks;
        }
    } else {
        std::ifstream in(cfg.vaultPath);
        std::ostringstream ss;
        ss << in.rdbuf();
        data = ss.str();
    }
    
    std::istringstream lines(data);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, eq);
        std::string encoded = line.substr(eq + 1);
        ks.kv[key] = crypto::base64Decode(encoded);
    }
    
    return ks;
}

void saveVault(const core::Config& cfg, const core::KeyStore& ks) {
    fs::create_directories(fs::path(cfg.vaultPath).parent_path());
    auto tmp = fs::path(cfg.vaultPath).parent_path() / ".tmp.ak.vault";
    
    {
        std::ofstream out(tmp);
        for (const auto& pair : ks.kv) {
            out << pair.first << "=" << crypto::base64Encode(pair.second) << "\n";
        }
    }
    
    if (cfg.gpgAvailable && !cfg.forcePlain && 
        cfg.vaultPath.size() > 4 && 
        cfg.vaultPath.substr(cfg.vaultPath.size() - 4) == ".gpg") {
        
        std::string cmd;
        std::string passFile;
        
        if (!cfg.presetPassphrase.empty()) {
            passFile = (fs::path(cfg.configDir) / ".pass.tmp").string();
            {
                std::ofstream pf(passFile, std::ios::trunc);
                pf << cfg.presetPassphrase;
                pf.close();
            }
#ifdef __unix__
            ::chmod(passFile.c_str(), 0600);
#endif
            cmd = "gpg --batch --yes -o '" + cfg.vaultPath + 
                  "' --pinentry-mode loopback --passphrase-file '" + passFile + 
                  "' --symmetric --cipher-algo AES256 '" + tmp.string() + "'";
        } else {
            cmd = "gpg --yes -o '" + cfg.vaultPath + 
                  "' --symmetric --cipher-algo AES256 '" + tmp.string() + "'";
        }
        
        int rc = ::system(cmd.c_str());
        if (!passFile.empty()) {
            fs::remove(passFile);
        }
        
        if (rc != 0) {
            throw std::runtime_error("Failed to encrypt vault with gpg");
        }
        
        fs::remove(tmp);
    } else {
        fs::rename(tmp, cfg.vaultPath);
    }
}

// Profile operations
fs::path profilePath(const core::Config& cfg, const std::string& name) {
    return fs::path(cfg.profilesDir) / (name + ".profile");
}

std::vector<std::string> listProfiles(const core::Config& cfg) {
    std::vector<std::string> names;
    if (!fs::exists(cfg.profilesDir)) {
        return names;
    }
    
    for (const auto& entry : fs::directory_iterator(cfg.profilesDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        auto filename = entry.path().filename().string();
        if (filename.size() > 8 && filename.substr(filename.size() - 8) == ".profile") {
            names.push_back(filename.substr(0, filename.size() - 8));
        }
    }
    
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> readProfile(const core::Config& cfg, const std::string& name) {
    std::vector<std::string> keys;
    auto path = profilePath(cfg, name);
    if (!fs::exists(path)) {
        return keys;
    }
    
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        line = core::trim(line);
        if (!line.empty()) {
            keys.push_back(line);
        }
    }
    
    return keys;
}

void writeProfile(const core::Config& cfg, const std::string& name, const std::vector<std::string>& keys) {
    fs::create_directories(cfg.profilesDir);
    std::ofstream out(profilePath(cfg, name));
    
    std::unordered_set<std::string> seen;
    std::vector<std::string> sorted = keys;
    std::sort(sorted.begin(), sorted.end());
    
    for (const auto& key : sorted) {
        if (seen.insert(key).second) {
            out << key << "\n";
        }
    }
}

// Persistence operations
std::string mappingFileForDir(const core::Config& cfg, const std::string& dir) {
    crypto::SHA256 hasher;
    hasher.update(dir);
    std::string sha = hasher.final().substr(0, 16);
    return cfg.persistDir + "/" + sha + ".mapping";
}

std::vector<std::string> readDirProfiles(const core::Config& cfg, const std::string& dir) {
    std::vector<std::string> profiles;
    std::string mappingFile = mappingFileForDir(cfg, dir);
    
    if (!fs::exists(mappingFile)) {
        return profiles;
    }
    
    std::ifstream in(mappingFile);
    std::string line;
    while (std::getline(in, line)) {
        line = core::trim(line);
        if (!line.empty()) {
            profiles.push_back(line);
        }
    }
    
    return profiles;
}

void writeDirProfiles(const core::Config& cfg, const std::string& dir, const std::vector<std::string>& profiles) {
    fs::create_directories(cfg.persistDir);
    std::string mappingFile = mappingFileForDir(cfg, dir);
    
    std::ofstream out(mappingFile);
    for (const auto& profile : profiles) {
        out << profile << "\n";
    }
}

// Bundle operations
std::string bundleFile(const core::Config& cfg, const std::string& profile) {
    return cfg.persistDir + "/" + profile + ".bundle";
}

void writeEncryptedBundle(const core::Config& cfg, const std::string& profile, const std::string& exports) {
    fs::create_directories(cfg.persistDir);
    std::string bundlePath = bundleFile(cfg, profile);
    
    // For now, just write as plain text. In a real implementation, you might want to encrypt this
    std::ofstream out(bundlePath);
    out << exports;
}

std::string readEncryptedBundle(const core::Config& cfg, const std::string& profile) {
    std::string bundlePath = bundleFile(cfg, profile);
    if (!fs::exists(bundlePath)) {
        return "";
    }
    
    std::ifstream in(bundlePath);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Import/Export helpers
std::vector<std::pair<std::string, std::string>> parse_env_file(std::istream& input) {
    std::vector<std::pair<std::string, std::string>> kvs;
    std::string line;
    
    while (std::getline(input, line)) {
        line = core::trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Skip shell constructs that aren't env vars
        if (line.find("alias ") == 0 ||
            line.find("[[") != std::string::npos ||
            line.find("$(") != std::string::npos ||
            line.find("function ") == 0 ||
            line.find("if ") == 0 ||
            line.find("case ") == 0 ||
            line.find("for ") == 0 ||
            line.find("while ") == 0) {
            continue;
        }
        
        if (line.rfind("export ", 0) == 0) {
            line = line.substr(7);
        }
        
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        
        std::string key = core::trim(line.substr(0, eq));
        std::string value = line.substr(eq + 1);
        
        // Validate key name - must be valid variable name
        if (key.empty() || (!std::isalpha(key[0]) && key[0] != '_')) {
            continue;
        }
        
        bool validKey = true;
        for (char c : key) {
            if (!std::isalnum(c) && c != '_') {
                validKey = false;
                break;
            }
        }
        if (!validKey) {
            continue;
        }
        
        if (!value.empty() && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        
        kvs.push_back({key, value});
    }
    
    return kvs;
}

std::vector<std::pair<std::string, std::string>> parse_json_min(const std::string& text) {
    std::vector<std::pair<std::string, std::string>> kvs;
    size_t i = 0;
    
    while (true) {
        i = text.find('"', i);
        if (i == std::string::npos) {
            break;
        }
        
        size_t j = text.find('"', i + 1);
        if (j == std::string::npos) {
            break;
        }
        
        std::string key = text.substr(i + 1, j - i - 1);
        size_t colon = text.find(':', j);
        if (colon == std::string::npos) {
            break;
        }
        
        size_t v1 = text.find('"', colon);
        if (v1 == std::string::npos) {
            i = j + 1;
            continue;
        }
        
        size_t v2 = text.find('"', v1 + 1);
        if (v2 == std::string::npos) {
            break;
        }
        
        std::string value = text.substr(v1 + 1, v2 - v1 - 1);
        kvs.push_back({key, value});
        i = v2 + 1;
    }
    
    return kvs;
}

// Profile-specific key storage functions
std::filesystem::path profileKeysPath(const core::Config& cfg, const std::string& name) {
    return fs::path(cfg.profilesDir) / (name + ".keys.gpg");
}

std::map<std::string, std::string> loadProfileKeys(const core::Config& cfg, const std::string& profileName) {
    std::map<std::string, std::string> keys;
    auto path = profileKeysPath(cfg, profileName);
    
    if (!fs::exists(path)) {
        return keys;
    }
    
    std::string data;
    if (cfg.gpgAvailable && !cfg.forcePlain &&
        path.extension() == ".gpg") {
        
        // If file is encrypted, we need a passphrase - return empty if not provided
        if (cfg.presetPassphrase.empty()) {
            // Silently return empty map - don't try interactive decryption in non-interactive contexts
            return keys;
        }
        
        int rc = 0;
        auto pfile = fs::path(cfg.configDir) / ".pass.tmp";
        {
            std::ofstream pf(pfile, std::ios::trunc);
            pf << cfg.presetPassphrase;
            pf.close();
        }
#ifdef __unix__
        ::chmod(pfile.c_str(), 0600);
#endif
        std::string cmd = "gpg --batch --yes --quiet --pinentry-mode loopback --passphrase-file '" +
                         pfile.string() + "' --decrypt '" + path.string() + "' 2>/dev/null";
        data = system::runCmdCapture(cmd, &rc);
        fs::remove(pfile);
        
        if (rc != 0) {
            std::cerr << "⚠️  Failed to decrypt profile keys for " << profileName << "\n";
            return keys;
        }
    } else {
        std::ifstream in(path);
        std::ostringstream ss;
        ss << in.rdbuf();
        data = ss.str();
    }
    
    std::istringstream lines(data);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, eq);
        std::string encoded = line.substr(eq + 1);
        keys[key] = crypto::base64Decode(encoded);
    }
    
    return keys;
}

void saveProfileKeys(const core::Config& cfg, const std::string& profileName, const std::map<std::string, std::string>& keys) {
    fs::create_directories(cfg.profilesDir);
    auto path = profileKeysPath(cfg, profileName);
    auto tmp = fs::path(cfg.profilesDir) / (".tmp." + profileName + ".keys");
    
    {
        std::ofstream out(tmp);
        for (const auto& [key, value] : keys) {
            out << key << "=" << crypto::base64Encode(value) << "\n";
        }
    }
    
    if (cfg.gpgAvailable && !cfg.forcePlain && path.extension() == ".gpg") {
        std::string cmd;
        std::string passFile;
        
        if (!cfg.presetPassphrase.empty()) {
            passFile = (fs::path(cfg.configDir) / ".pass.tmp").string();
            {
                std::ofstream pf(passFile, std::ios::trunc);
                pf << cfg.presetPassphrase;
                pf.close();
            }
#ifdef __unix__
            ::chmod(passFile.c_str(), 0600);
#endif
            cmd = "gpg --batch --yes -o '" + path.string() +
                  "' --pinentry-mode loopback --passphrase-file '" + passFile +
                  "' --symmetric --cipher-algo AES256 '" + tmp.string() + "'";
        } else {
            cmd = "gpg --yes -o '" + path.string() +
                  "' --symmetric --cipher-algo AES256 '" + tmp.string() + "'";
        }
        
        int rc = ::system(cmd.c_str());
        if (!passFile.empty()) {
            fs::remove(passFile);
        }
        
        if (rc != 0) {
            throw std::runtime_error("Failed to encrypt profile keys with gpg");
        }
        
        fs::remove(tmp);
    } else {
        // For plain text storage, change extension
        if (path.extension() == ".gpg") {
            path = fs::path(cfg.profilesDir) / (profileName + ".keys");
        }
        fs::rename(tmp, path);
    }
}

std::map<std::string, std::string> readProfileKeys(const core::Config& cfg, const std::string& name) {
    return loadProfileKeys(cfg, name);
}

void writeProfileKeys(const core::Config& cfg, const std::string& name, const std::map<std::string, std::string>& keys) {
    saveProfileKeys(cfg, name, keys);
}

// Default profile management
void ensureDefaultProfile(const core::Config& cfg) {
    auto profiles = listProfiles(cfg);
    if (std::find(profiles.begin(), profiles.end(), "default") == profiles.end()) {
        writeProfile(cfg, "default", {});
    }
}

std::string getDefaultProfileName() {
    return "default";
}

void setDefaultProfile(const core::Config& cfg, const std::string& profileName) {
    // For now, we'll use a simple file to store the default profile name
    std::string defaultProfileFile = cfg.configDir + "/.default_profile";
    fs::create_directories(cfg.configDir);
    
    std::ofstream out(defaultProfileFile);
    out << profileName;
}

// Migration utilities
bool hasGlobalVault(const core::Config& cfg) {
    return fs::exists(cfg.vaultPath);
}

void migrateGlobalVaultToProfiles(const core::Config& cfg) {
    if (!hasGlobalVault(cfg)) {
        return; // Nothing to migrate
    }
    
    try {
        // Load global vault
        auto globalVault = loadVault(cfg);
        
        // Ensure default profile exists
        ensureDefaultProfile(cfg);
        
        // Migrate all keys to default profile
        auto defaultProfileName = getDefaultProfileName();
        auto existingKeys = loadProfileKeys(cfg, defaultProfileName);
        
        // Merge keys (existing profile keys take precedence)
        for (const auto& [key, value] : globalVault.kv) {
            if (existingKeys.find(key) == existingKeys.end()) {
                existingKeys[key] = value;
            }
        }
        
        // Save to default profile
        saveProfileKeys(cfg, defaultProfileName, existingKeys);
        
        // Optionally backup and remove global vault
        std::string backupPath = cfg.vaultPath + ".backup";
        fs::copy_file(cfg.vaultPath, backupPath);
        
        std::cout << "Global vault migrated to default profile. Backup saved to: " << backupPath << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Migration failed: " << e.what() << std::endl;
        throw;
    }
}

// Key constraint validation
bool validateKeyUniquenessInProfile(const core::Config& cfg, const std::string& profileName, const std::string& keyName, const std::string& excludeKeyName) {
    try {
        auto profileKeys = loadProfileKeys(cfg, profileName);
        
        // Check if key name already exists in profile (excluding the key being updated)
        for (const auto& [existingKey, value] : profileKeys) {
            if (existingKey == keyName && existingKey != excludeKeyName) {
                return false; // Key name already exists
            }
        }
        
        return true; // Key name is unique in this profile
    } catch (const std::exception&) {
        return true; // If we can't load profile, assume it's valid
    }
}

std::string getServiceForKeyName(const std::string& keyName) {
    // Extract service name from key name by common patterns
    std::string lowerKey = keyName;
    std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
    
    // Common service patterns
    if (lowerKey.find("openai") != std::string::npos) return "openai";
    if (lowerKey.find("anthropic") != std::string::npos) return "anthropic";
    if (lowerKey.find("gemini") != std::string::npos || lowerKey.find("google") != std::string::npos) return "gemini";
    if (lowerKey.find("azure") != std::string::npos) return "azure_openai";
    if (lowerKey.find("brave") != std::string::npos) return "brave";
    if (lowerKey.find("cohere") != std::string::npos) return "cohere";
    if (lowerKey.find("deepseek") != std::string::npos) return "deepseek";
    if (lowerKey.find("exa") != std::string::npos) return "exa";
    if (lowerKey.find("fireworks") != std::string::npos) return "fireworks";
    if (lowerKey.find("groq") != std::string::npos) return "groq";
    if (lowerKey.find("huggingface") != std::string::npos || lowerKey.find("hugging_face") != std::string::npos) return "huggingface";
    if (lowerKey.find("langchain") != std::string::npos) return "langchain";
    if (lowerKey.find("mistral") != std::string::npos) return "mistral";
    if (lowerKey.find("openrouter") != std::string::npos) return "openrouter";
    if (lowerKey.find("perplexity") != std::string::npos) return "perplexity";
    if (lowerKey.find("sambanova") != std::string::npos) return "sambanova";
    if (lowerKey.find("tavily") != std::string::npos) return "tavily";
    if (lowerKey.find("together") != std::string::npos) return "together";
    if (lowerKey.find("xai") != std::string::npos) return "xai";
    if (lowerKey.find("aws") != std::string::npos) return "aws";
    if (lowerKey.find("github") != std::string::npos) return "github";
    if (lowerKey.find("stripe") != std::string::npos) return "stripe";
    if (lowerKey.find("slack") != std::string::npos) return "slack";
    if (lowerKey.find("discord") != std::string::npos) return "discord";
    
    return "unknown"; // Could not determine service
}

std::vector<std::string> getServiceKeysInProfile(const core::Config& cfg, const std::string& profileName, const std::string& serviceName) {
    std::vector<std::string> serviceKeys;
    
    try {
        auto profileKeys = loadProfileKeys(cfg, profileName);
        
        for (const auto& [keyName, value] : profileKeys) {
            if (getServiceForKeyName(keyName) == serviceName) {
                serviceKeys.push_back(keyName);
            }
        }
    } catch (const std::exception&) {
        // Ignore errors loading profile
    }
    
    return serviceKeys;
}

bool canAddKeyToProfile(const core::Config& cfg, const std::string& profileName, const std::string& keyName, const std::string& serviceName) {
    (void)serviceName; // Currently unused, but kept for future extensibility
    
    // Check if key name is unique in profile
    if (!validateKeyUniquenessInProfile(cfg, profileName, keyName, "")) {
        return false;
    }
    
    // Multiple keys per service are allowed as long as they have different names
    // This is already handled by the key name uniqueness check above
    return true;
}

} // namespace storage
} // namespace ak