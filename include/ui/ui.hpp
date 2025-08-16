#pragma once

#include <string>

namespace ak {
namespace ui {

// Colors namespace
namespace Colors {
    extern const std::string RESET;
    extern const std::string BOLD;
    extern const std::string DIM;
    
    // Basic colors
    extern const std::string BLACK;
    extern const std::string RED;
    extern const std::string GREEN;
    extern const std::string YELLOW;
    extern const std::string BLUE;
    extern const std::string MAGENTA;
    extern const std::string CYAN;
    extern const std::string WHITE;
    
    // Bright colors
    extern const std::string BRIGHT_BLACK;
    extern const std::string BRIGHT_RED;
    extern const std::string BRIGHT_GREEN;
    extern const std::string BRIGHT_YELLOW;
    extern const std::string BRIGHT_BLUE;
    extern const std::string BRIGHT_MAGENTA;
    extern const std::string BRIGHT_CYAN;
    extern const std::string BRIGHT_WHITE;
    
    // Background colors
    extern const std::string BG_BLACK;
    extern const std::string BG_RED;
    extern const std::string BG_GREEN;
    extern const std::string BG_YELLOW;
    extern const std::string BG_BLUE;
    extern const std::string BG_MAGENTA;
    extern const std::string BG_CYAN;
    extern const std::string BG_WHITE;
}

// Color support functions
bool isColorSupported();
std::string colorize(const std::string& text, const std::string& color);

} // namespace ui
} // namespace ak