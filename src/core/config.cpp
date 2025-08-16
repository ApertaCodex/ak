#include "core/config.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <cctype>

namespace ak {

// Global configuration instance
Config config;

// Version information
const std::string AK_VERSION = "1.0.0";

// String utility functions
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool icontains(const std::string& haystack, const std::string& needle) {
    std::string lower_haystack = toLower(haystack);
    std::string lower_needle = toLower(needle);
    return lower_haystack.find(lower_needle) != std::string::npos;
}

std::string maskValue(const std::string& value) {
    if (value.empty()) {
        return "";
    }
    
    if (value.length() <= 8) {
        return std::string(value.length(), '*');
    }
    
    // Show first 4 and last 4 characters
    return value.substr(0, 4) + std::string(value.length() - 8, '*') + value.substr(value.length() - 4);
}

// Config class implementation
Config::Config() {
    // Initialize default values
}

void Config::loadFromFile(const std::string& /* filename */) {
    // Implementation for loading configuration from file
    // This would typically read from a JSON or TOML file
}

void Config::saveToFile(const std::string& /* filename */) const {
    // Implementation for saving configuration to file
}

bool Config::validate() const {
    // Basic validation logic
    return true;
}

// KeyStore implementation
KeyStore::KeyStore() {
    // Initialize empty key store
}

void KeyStore::addKey(const std::string& name, const std::string& value) {
    keys[name] = value;
}

std::string KeyStore::getKey(const std::string& name) const {
    auto it = keys.find(name);
    return (it != keys.end()) ? it->second : "";
}

bool KeyStore::hasKey(const std::string& name) const {
    return keys.find(name) != keys.end();
}

void KeyStore::removeKey(const std::string& name) {
    keys.erase(name);
}

std::vector<std::string> KeyStore::listKeys() const {
    std::vector<std::string> keyList;
    for (const auto& pair : keys) {
        keyList.push_back(pair.first);
    }
    return keyList;
}

size_t KeyStore::size() const {
    return keys.size();
}

void KeyStore::clear() {
    keys.clear();
}

} // namespace ak
