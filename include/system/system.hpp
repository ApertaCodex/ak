#pragma once

#include "core/config.hpp"
#include <string>
#include <filesystem>

namespace ak {
namespace system {

// Platform-specific file operations
#ifdef __unix__
void secureChmod(const std::filesystem::path& path, mode_t mode);
#else
void secureChmod(const std::filesystem::path& path, int mode);
#endif

void ensureSecureDir(const std::filesystem::path& path);
void ensureSecureFile(const std::filesystem::path& path);

// Process execution
std::string runCmdCapture(const std::string& cmd, int* exitCode = nullptr);

// Input handling
std::string promptSecret(const std::string& prompt);

// Clipboard operations
bool copyClipboard(const std::string& text);

// Working directory
std::string getCwd();

// File utilities
bool fileContains(const std::string& path, const std::string& needle);
void appendLine(const std::string& path, const std::string& line);

// User information
struct TargetUser {
    std::string userName;
    std::string home;
    std::string shellName;
};

TargetUser resolveTargetUser();

// Shell integration
void writeShellInitFile(const core::Config& cfg);
void ensureSourcedInRc(const core::Config& cfg);

// Guard functions
void guard_enable(const core::Config& cfg);
void guard_disable();

} // namespace system
} // namespace ak