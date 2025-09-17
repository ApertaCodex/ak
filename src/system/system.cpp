#include "system/system.hpp"
#include "core/config.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#ifdef __unix__
#include <termios.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#endif

namespace ak {
namespace system {

namespace fs = std::filesystem;

// Platform-specific file operations
#ifdef __unix__
void secureChmod(const fs::path& path, mode_t mode) {
    ::chmod(path.c_str(), mode);
}
#else
void secureChmod(const fs::path&, int) {
    // No-op on non-Unix platforms
}
#endif

void ensureSecureDir(const fs::path& path) {
    if (!fs::exists(path)) {
        fs::create_directories(path);
    }
#ifdef __unix__
    secureChmod(path, 0700);
#endif
}

void ensureSecureFile(const fs::path& path) {
    if (!fs::exists(path)) {
        std::ofstream(path.string(), std::ios::app).close();
    }
#ifdef __unix__
    secureChmod(path, 0600);
#endif
}

// Process execution
std::string runCmdCapture(const std::string& cmd, int* exitCode) {
    std::array<char, 4096> buffer{};
    std::string result;
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        if (exitCode) *exitCode = -1;
        return {};
    }
    
    while (true) {
        size_t bytesRead = fread(buffer.data(), 1, buffer.size(), pipe);
        if (bytesRead > 0) {
            result.append(buffer.data(), bytesRead);
        }
        if (bytesRead < buffer.size()) {
            break;
        }
    }
    
    int rc = pclose(pipe);
    if (exitCode) *exitCode = rc;
    return result;
}

// Input handling
std::string promptSecret(const std::string& prompt) {
    std::string value;
#ifdef __unix__
    std::cerr << prompt;
    std::cerr.flush();
    
    termios oldTermios;
    tcgetattr(STDIN_FILENO, &oldTermios);
    termios newTermios = oldTermios;
    newTermios.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);
    
    std::getline(std::cin, value);
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
    std::cerr << "\n";
#else
    std::cerr << prompt;
    std::getline(std::cin, value);
#endif
    return value;
}

// Clipboard operations
bool copyClipboard(const std::string& text) {
    if (core::commandExists("pbcopy")) {
        std::string cmd = "printf %s '" + text + "' | pbcopy";
        return ::system(cmd.c_str()) == 0;
    }
    if (core::commandExists("wl-copy")) {
        std::string cmd = "printf %s '" + text + "' | wl-copy";
        return ::system(cmd.c_str()) == 0;
    }
    if (core::commandExists("xclip")) {
        std::string cmd = "printf %s '" + text + "' | xclip -selection clipboard";
        return ::system(cmd.c_str()) == 0;
    }
    return false;
}

// Working directory
std::string getCwd() {
    return fs::current_path().string();
}

// File utilities
bool fileContains(const std::string& path, const std::string& needle) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void appendLine(const std::string& path, const std::string& line) {
    std::ofstream file(path, std::ios::app);
    file << line << "\n";
}

// User information
TargetUser resolveTargetUser() {
    TargetUser user;
    
#ifdef __unix__
    // Check if running under sudo
    const char* sudoUser = getenv("SUDO_USER");
    if (sudoUser) {
        user.userName = sudoUser;
        struct passwd* pwd = getpwnam(sudoUser);
        if (pwd) {
            user.home = pwd->pw_dir;
            user.shellName = fs::path(pwd->pw_shell).filename().string();
        }
    } else {
        // Get current user
        uid_t uid = getuid();
        struct passwd* pwd = getpwuid(uid);
        if (pwd) {
            user.userName = pwd->pw_name;
            user.home = pwd->pw_dir;
            user.shellName = fs::path(pwd->pw_shell).filename().string();
        }
    }
#else
    // Fallback for non-Unix systems
    const char* username = getenv("USER");
    if (!username) username = getenv("USERNAME");
    user.userName = username ? username : "unknown";
    
    const char* home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    user.home = home ? home : "/tmp";
    
    user.shellName = "sh"; // Default fallback
#endif
    
    return user;
}

// Shell integration - Full implementation
void writeShellInitFile(const core::Config& cfg) {
    fs::path initFile = fs::path(cfg.configDir) / "shell-init.sh";
    ensureSecureDir(fs::path(cfg.configDir));
    
    std::ofstream out(initFile);
    out << "# AK Shell Integration\n";
    out << "# This file is auto-generated - do not edit manually\n";
    out << "# Provides ak_load and ak_unload functions for environment management\n\n";
    
    // Find the ak binary path
    std::string akBinary = "ak"; // Default to PATH lookup
    
    // Try to find the absolute path to ak
    std::string whichResult = runCmdCapture("which ak 2>/dev/null");
    if (!whichResult.empty()) {
        // Remove trailing newline
        whichResult.erase(whichResult.find_last_not_of(" \n\r\t") + 1);
        if (!whichResult.empty()) {
            akBinary = whichResult;
        }
    }
    
    out << "# Shell integration functions\n";
    out << "ak_load() {\n";
    out << "    if [ $# -eq 0 ]; then\n";
    out << "        echo \"Usage: ak_load <profile|key> [--persist]\" >&2\n";
    out << "        echo \"Examples:\" >&2\n";
    out << "        echo \"  ak_load myprofile\" >&2\n";
    out << "        echo \"  ak_load OPENAI_API_KEY\" >&2\n";
    out << "        echo \"  ak_load myprofile --persist\" >&2\n";
    out << "        return 1\n";
    out << "    fi\n";
    out << "    \n";
    out << "    # Capture both stdout (exports) and stderr (messages) separately\n";
    out << "    local ak_exports ak_messages ak_exit\n";
    out << "    local temp_file=$(mktemp 2>/dev/null || echo /tmp/ak_load_$$)\n";
    out << "    \n";
    out << "    # Run ak load and capture output\n";
    out << "    ak_exports=$(" << akBinary << " load \"$@\" 2>\"$temp_file\")\n";
    out << "    ak_exit=$?\n";
    out << "    ak_messages=$(cat \"$temp_file\" 2>/dev/null || echo \"\")\n";
    out << "    rm -f \"$temp_file\" 2>/dev/null\n";
    out << "    \n";
    out << "    if [ $ak_exit -eq 0 ]; then\n";
    out << "        # Only evaluate if we got actual exports\n";
    out << "        if [ -n \"$ak_exports\" ]; then\n";
    out << "            eval \"$ak_exports\"\n";
    out << "            echo \"✅ Loaded '$1' into current shell\" >&2\n";
    out << "            \n";
    out << "            # Show what was loaded\n";
    out << "            local loaded_vars\n";
    out << "            loaded_vars=$(echo \"$ak_exports\" | grep -o 'export [^=]*' | sed 's/export //' | tr '\\n' ' ')\n";
    out << "            if [ -n \"$loaded_vars\" ]; then\n";
    out << "                echo \"   Variables: $loaded_vars\" >&2\n";
    out << "            fi\n";
    out << "        else\n";
    out << "            echo \"⚠️  No environment variables to load\" >&2\n";
    out << "        fi\n";
    out << "        \n";
    out << "        # Show any help messages from ak load (but filter out the eval suggestions)\n";
    out << "        if [ -n \"$ak_messages\" ]; then\n";
    out << "            echo \"$ak_messages\" | grep -v \"eval\" >&2\n";
    out << "        fi\n";
    out << "    else\n";
    out << "        echo \"❌ Failed to load '$1'\" >&2\n";
    out << "        if [ -n \"$ak_messages\" ]; then\n";
    out << "            echo \"$ak_messages\" >&2\n";
    out << "        fi\n";
    out << "        return $ak_exit\n";
    out << "    fi\n";
    out << "}\n\n";
    
    out << "ak_unload() {\n";
    out << "    local ak_output ak_exit\n";
    out << "    ak_output=$(" << akBinary << " unload \"$@\" 2>&1)\n";
    out << "    ak_exit=$?\n";
    out << "    \n";
    out << "    if [ $ak_exit -eq 0 ]; then\n";
    out << "        if [ -n \"$ak_output\" ]; then\n";
    out << "            eval \"$ak_output\"\n";
    out << "            # Show what was unloaded\n";
    out << "            local unset_vars\n";
    out << "            unset_vars=$(echo \"$ak_output\" | grep -o 'unset [^[:space:]]*' | sed 's/unset //' | tr '\\n' ' ')\n";
    out << "            echo \"🔄 Unloaded environment variables\" >&2\n";
    out << "            if [ -n \"$unset_vars\" ]; then\n";
    out << "                echo \"   Variables: $unset_vars\" >&2\n";
    out << "            fi\n";
    out << "        else\n";
    out << "            echo \"⚠️  No environment variables to unload\" >&2\n";
    out << "        fi\n";
    out << "    else\n";
    out << "        echo \"❌ Failed to unload\" >&2\n";
    out << "        echo \"$ak_output\" >&2\n";
    out << "        return $ak_exit\n";
    out << "    fi\n";
    out << "}\n\n";
    
    out << "# Auto-load persisted profiles for current directory\n";
    out << "ak_auto_load() {\n";
    out << "    local current_dir=\"$(pwd)\"\n";
    out << "    local ak_binary=\"" << akBinary << "\"\n";
    out << "    \n";
    out << "    # Skip auto-loading if we're already in the middle of loading\n";
    out << "    if [ -n \"$AK_LOADING\" ]; then\n";
    out << "        return 0\n";
    out << "    fi\n";
    out << "    \n";
    out << "    # Get persisted profiles for this directory\n";
    out << "    local persisted_profiles\n";
    out << "    persisted_profiles=$(\"$ak_binary\" _internal_get_dir_profiles \"$current_dir\" 2>/dev/null)\n";
    out << "    \n";
    out << "    if [ -z \"$persisted_profiles\" ]; then\n";
    out << "        return 0\n";
    out << "    fi\n";
    out << "    \n";
    out << "    # Set loading flag to prevent recursion\n";
    out << "    export AK_LOADING=1\n";
    out << "    \n";
    out << "    # Load each persisted profile\n";
    out << "    echo \"$persisted_profiles\" | while read -r profile_name; do\n";
    out << "        if [ -n \"$profile_name\" ]; then\n";
    out << "            # Check if it's a default profile or if we should auto-load it\n";
    out << "            if [ \"$profile_name\" = \"default\" ] || [ \"$AK_AUTO_LOAD_ALL\" = \"1\" ]; then\n";
    out << "                local exports\n";
    out << "                exports=$(\"$ak_binary\" _internal_get_bundle \"$profile_name\" 2>/dev/null)\n";
    out << "                if [ -n \"$exports\" ]; then\n";
    out << "                    eval \"$exports\"\n";
    out << "                    echo \"🔄 Auto-loaded profile '$profile_name' for $current_dir\" >&2\n";
    out << "                fi\n";
    out << "            fi\n";
    out << "        fi\n";
    out << "    done\n";
    out << "    \n";
    out << "    # Clear loading flag\n";
    out << "    unset AK_LOADING\n";
    out << "}\n\n";
    
    out << "# Completion functions (basic)\n";
    out << "if [ -n \"$BASH_VERSION\" ]; then\n";
    out << "    _ak_load_complete() {\n";
    out << "        local cur=\"${COMP_WORDS[COMP_CWORD]}\"\n";
    out << "        local profiles=$(ak profiles 2>/dev/null)\n";
    out << "        local keys=$(ak ls 2>/dev/null | awk '{print $1}')\n";
    out << "        COMPREPLY=( $(compgen -W \"$profiles $keys --persist\" -- \"$cur\") )\n";
    out << "    }\n";
    out << "    complete -F _ak_load_complete ak_load\n";
    out << "fi\n\n";
    
    out << "# Main ak wrapper function that intercepts load/unload commands\n";
    out << "ak() {\n";
    out << "    # Find the real ak binary path\n";
    out << "    local ak_binary=\"" << akBinary << "\"\n";
    out << "    \n";
    out << "    # If no arguments, show help\n";
    out << "    if [ $# -eq 0 ]; then\n";
    out << "        \"$ak_binary\"\n";
    out << "        return $?\n";
    out << "    fi\n";
    out << "    \n";
    out << "    # Handle load command specially\n";
    out << "    if [ \"$1\" = \"load\" ]; then\n";
    out << "        shift  # Remove 'load' from arguments\n";
    out << "        ak_load \"$@\"\n";
    out << "        return $?\n";
    out << "    fi\n";
    out << "    \n";
    out << "    # Handle unload command specially\n";
    out << "    if [ \"$1\" = \"unload\" ]; then\n";
    out << "        shift  # Remove 'unload' from arguments\n";
    out << "        ak_unload \"$@\"\n";
    out << "        return $?\n";
    out << "    fi\n";
    out << "    \n";
    out << "    # For all other commands, pass through to real ak binary\n";
    out << "    \"$ak_binary\" \"$@\"\n";
    out << "}\n\n";
    
    out << "# Directory change detection for auto-loading\n";
    out << "ak_setup_cd_hook() {\n";
    out << "    if [ -n \"$BASH_VERSION\" ]; then\n";
    out << "        # Bash: override cd with a function\n";
    out << "        cd() {\n";
    out << "            builtin cd \"$@\" && ak_auto_load\n";
    out << "        }\n";
    out << "    elif [ -n \"$ZSH_VERSION\" ]; then\n";
    out << "        # Zsh: use chpwd hook\n";
    out << "        chpwd() {\n";
    out << "            ak_auto_load\n";
    out << "        }\n";
    out << "    fi\n";
    out << "}\n\n";
    
    out << "# Setup hooks and run initial auto-load\n";
    out << "ak_setup_cd_hook\n";
    out << "ak_auto_load\n\n";
    
    out << "# Export functions for availability in subshells\n";
    out << "export -f ak ak_load ak_unload ak_auto_load ak_setup_cd_hook 2>/dev/null || true\n";
}

void ensureSourcedInRc(const core::Config& cfg) {
    // This would contain the shell RC file modification logic
    // Placeholder implementation
    TargetUser user = resolveTargetUser();
    std::string initFile = (fs::path(cfg.configDir) / "shell-init.sh").string();
    std::string sourceLine = "source \"" + initFile + "\"";
    
    // Logic to add source line to appropriate RC file based on shell
    if (user.shellName == "bash") {
        std::string rcFile = user.home + "/.bashrc";
        if (!fileContains(rcFile, sourceLine)) {
            appendLine(rcFile, "# Added by ak installer");
            appendLine(rcFile, sourceLine);
        }
    } else if (user.shellName == "zsh") {
        std::string rcFile = user.home + "/.zshrc";
        if (!fileContains(rcFile, sourceLine)) {
            appendLine(rcFile, "# Added by ak installer");
            appendLine(rcFile, sourceLine);
        }
    }
    // Additional shell support would be added here
}

// Guard functions - Placeholder implementations  
void guard_enable(const core::Config& cfg) {
    // This would contain the guard enable logic
    std::cout << "Guard functionality enabled for config: " << cfg.configDir << "\n";
    // Full implementation would go here
}

void guard_disable() {
    // This would contain the guard disable logic
    std::cout << "Guard functionality disabled\n";
    // Full implementation would go here
}

} // namespace system
} // namespace ak