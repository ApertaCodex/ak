#include "core/config.hpp"
#include "ui/ui.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <random>

namespace ak {
namespace core {

// Version information
#ifndef AK_VERSION_STRING
#define AK_VERSION_STRING "4.2.19"
#endif
const std::string AK_VERSION = AK_VERSION_STRING;

// Utility functions
bool commandExists(const std::string& cmd) {
    std::string command = "command -v " + cmd + " >/dev/null 2>&1";
    return system(command.c_str()) == 0;
}

std::string getenvs(const char* key, const std::string& defaultValue) {
    const char* val = std::getenv(key);
    return val ? std::string(val) : defaultValue;
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return str;
}

bool icontains(const std::string& haystack, const std::string& needle) {
    std::string lower_haystack = toLower(haystack);
    std::string lower_needle = toLower(needle);
    return lower_haystack.find(lower_needle) != std::string::npos;
}

std::string maskValue(const std::string& value) {
    if (value.empty()) {
        return "(empty)";
    }
    
    // For values 12 characters or less, mask everything
    if (value.length() <= 12) {
        return std::string(value.length(), '*');
    }
    
    // For longer values, show first 8 + "***" + last 4 characters
    return value.substr(0, 8) + "***" + value.substr(value.length() - 4);
}

// Error handling and output
void error(const Config& /* cfg */, const std::string& msg, int code) {
    std::cerr << ui::colorize("❌ " + msg, ui::Colors::BRIGHT_RED) << std::endl;
    exit(code);
}

void ok(const Config& /* cfg */, const std::string& msg) {
    std::cout << ui::colorize("✅ " + msg, ui::Colors::BRIGHT_GREEN) << std::endl;
}

void warn(const Config& /* cfg */, const std::string& msg) {
    std::cout << ui::colorize("⚠️  " + msg, ui::Colors::BRIGHT_YELLOW) << std::endl;
}

void info(const Config& /* cfg */, const std::string& msg) {
    std::cout << ui::colorize("ℹ️  " + msg, ui::Colors::BRIGHT_CYAN) << std::endl;
}

void success(const Config& /* cfg */, const std::string& msg) {
    std::cout << ui::colorize("🎉 " + msg, ui::Colors::BRIGHT_GREEN) << std::endl;
}

void working(const Config& /* cfg */, const std::string& msg) {
    std::cout << ui::colorize("⚡ " + msg, ui::Colors::BRIGHT_BLUE) << std::endl;
}

// Audit logging
std::string hashKeyName(const std::string& name) {
    // Simple hash implementation for key names (in production, use proper crypto hash)
    std::hash<std::string> hasher;
    size_t hashValue = hasher(name);
    
    std::ostringstream ss;
    ss << std::hex << hashValue;
    std::string result = ss.str();
    
    // Truncate to 16 characters
    return result.substr(0, std::min(result.length(), size_t(16)));
}

std::string isoTimeUTC() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    
    return ss.str();
}

void auditLog(const Config& cfg, const std::string& action, const std::vector<std::string>& keys) {
    if (cfg.auditLogPath.empty()) return;
    
    std::ofstream logFile(cfg.auditLogPath, std::ios::app);
    if (!logFile.is_open()) return;
    
    std::string timestamp = isoTimeUTC();
    
    for (const auto& key : keys) {
        std::string hashedKey = hashKeyName(key);
        logFile << timestamp << " " << action << " " << hashedKey << std::endl;
    }
    
    logFile.close();
}

// Instance management
std::string loadOrCreateInstanceId(const Config& cfg) {
    std::string instanceFile = cfg.persistDir + "/instance_id";
    
    // Try to read existing instance ID
    std::ifstream file(instanceFile);
    if (file.is_open()) {
        std::string instanceId;
        if (std::getline(file, instanceId) && !instanceId.empty()) {
            file.close();
            return instanceId;
        }
        file.close();
    }
    
    // Generate new instance ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::ostringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << dis(gen);
    }
    
    std::string instanceId = ss.str();
    
    // Create directory if it doesn't exist
    try {
        std::filesystem::create_directories(cfg.persistDir);
        
        // Write instance ID to file
        std::ofstream outFile(instanceFile);
        if (outFile.is_open()) {
            outFile << instanceId << std::endl;
            outFile.close();
        }
    } catch (const std::exception&) {
        // Failed to create directory or write file, return generated ID anyway
    }
    
    return instanceId;
}

} // namespace core
} // namespace ak
