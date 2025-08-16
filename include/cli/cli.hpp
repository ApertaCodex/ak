#pragma once

#include <string>
#include <vector>

namespace ak {
namespace cli {

// Flag expansion
std::vector<std::string> expandShortFlags(const std::vector<std::string>& args);

// Help system
void cmd_help();
void showWelcome();
void showLogo();
void showTips();

// Shell completion generation
void generateBashCompletion();
void generateZshCompletion();
void generateFishCompletion();

void writeBashCompletionToFile(const std::string& path);
void writeZshCompletionToFile(const std::string& path);
void writeFishCompletionToFile(const std::string& path);

} // namespace cli
} // namespace ak