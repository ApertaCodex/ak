#pragma once

#include "core/config.hpp"
#include <string>
#include <vector>

namespace ak {
namespace gui {

/**
 * Initialize and run the GUI application
 * @param cfg Configuration object
 * @param args Command line arguments
 * @return Exit code
 */
int runGuiApplication(const core::Config& cfg, const std::vector<std::string>& args);

/**
 * Check if GUI support is available
 * @return true if GUI is available, false otherwise
 */
bool isGuiAvailable();

} // namespace gui
} // namespace ak