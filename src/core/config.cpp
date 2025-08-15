#include "core/config.hpp"
#include "crypto/crypto.hpp"
#include "ui/ui.hpp"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <random>
#include <mutex>
#ifdef __unix__
#include <unistd.h>
#endif

namespace ak {
namespace core {

// Version
const std::string AK_VERSION = "1.0.0";

// Mask constants
static const int MASK_PREFIX = 8;
static const int MASK_SUFFIX = 4;

// Utility functions
bool commandExists(const std::string& cmd) {
    std::string which = "command -v '" + cmd + "' >/dev/null 2>&1";
    return system(which.c_str()) == 0;
}

std::string getenvs(const char* key, const std::string& defaultValue) {
    const char* value = getenv(key);
    return value ? std::string(value) : defaultValue;
}

std::string trim(const std::string& str) {
    size_t begin = 0;
    while (begin < str.size() && std::isspace(static_cast<unsigned char>(str[begin]))) {
        ++begin;
    }
    size_t end = str.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }
    return str.substr(begin, end - begin);
}

std::string toLower(std::string str) {
    for (char& c : str) {
        c = std::tolower(static_cast<unsigned char>(c));
    }
    return str;
}

bool icontains(const std::string& haystack, const std::string& needle) {
    auto h = toLower(haystack);
    auto n = toLower(needle);
    return h.find(n) != std::string::npos;
}

std::string maskValue(const std::string& value) {
    if (value.empty()) {
        return "(empty)";
    }
    if (static_cast<int>(value.size()) <= MASK_PREFIX + MASK_SUFFIX) {
        return std::string(value.size(), '*');
    }
    return value.substr(0, MASK_PREFIX) + "***" + value.substr(value.size() - MASK_SUFFIX);
}

// Error handling and output
void error(const Config& cfg, const std::string& msg, int code) {
    if (cfg.json) {
        std::cerr << "{\"ok\":false,\"error\":\"" << msg << "\"}\n";
    } else {
        std::cerr << ui::colorize("❌ " + msg, ui::Colors::BRIGHT_RED) << "\n";
    }
    exit(code);
}

void ok(const Config& cfg, const std::string& msg) {
    if (!cfg.json) {
        std::cerr << ui::colorize("✅ " + msg, ui::Colors::BRIGHT_GREEN) << "\n";
    }
}

void warn(const Config& cfg, const std::string& msg) {
    if (!cfg.json) {
        std::cerr << ui::colorize("⚠️  " + msg, ui::Colors::BRIGHT_YELLOW) << "\n";
    }
}

// Time utility
std::string isoTimeUTC() {
    using namespace std::chrono;
    auto t = system_clock::to_time_t(system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

// Hash key name using crypto module
std::string hashKeyName(const std::string& name) {
    return crypto::hashKeyName(name);
}

// Audit logging
static std::mutex auditMutex;

void auditLog(const Config& cfg, const std::string& action, const std::vector<std::string>& keys) {
    if (cfg.auditLogPath.empty()) {
        return;
    }
    
    std::filesystem::create_directories(std::filesystem::path(cfg.auditLogPath).parent_path());
    
    std::lock_guard<std::mutex> lock(auditMutex);
    std::ofstream out(cfg.auditLogPath, std::ios::app);
    out << isoTimeUTC() << " action=" << action << " instance=" << cfg.instanceId 
        << " count=" << keys.size();
    
    if (!keys.empty()) {
        out << " keys=";
        bool first = true;
        for (const auto& key : keys) {
            if (!first) {
                out << ',';
            }
            first = false;
            out << hashKeyName(key);
        }
    }
    out << "\n";
}

// Instance ID management
std::string loadOrCreateInstanceId(const Config& cfg) {
    auto path = std::filesystem::path(cfg.configDir) / "instance.id";
    
    if (std::filesystem::exists(path)) {
        std::ifstream in(path);
        std::string id;
        std::getline(in, id);
        return id;
    }
    
    std::string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, static_cast<int>(chars.size()) - 1);
    
    std::string id;
    id.reserve(24);
    for (int i = 0; i < 24; ++i) {
        id.push_back(chars[dis(gen)]);
    }
    
    std::filesystem::create_directories(std::filesystem::path(cfg.configDir));
    std::ofstream out(path);
    out << id;
    
    return id;
}

} // namespace core
} // namespace ak