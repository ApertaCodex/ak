#pragma once

#ifdef BUILD_GUI

#include "core/config.hpp"
#include <memory>
#include <string>

// Forward declaration for Slint classes
namespace slint {
    class MainWindow;
}

namespace ak {
namespace gui {

class MainWindow
{
public:
    explicit MainWindow(const core::Config& cfg);
    ~MainWindow();

    // Main function to run the UI
    void run();

private:
    void setupCallbacks();
    void showAbout();
    void showHelp();
    void exitApplication();
    bool copyToClipboard(const std::string& text);

    const core::Config& config;

    // Slint UI
    std::unique_ptr<slint::MainWindow> ui;
};

} // namespace gui
} // namespace ak

#endif // BUILD_GUI