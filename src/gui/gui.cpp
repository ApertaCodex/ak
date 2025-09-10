#include "gui/gui.hpp"
#include "core/config.hpp"
#include <iostream>
#include <cstring>

#ifdef BUILD_GUI
#include "gui/mainwindow.hpp"
#include <slint.h>
#endif

namespace ak {
namespace gui {

bool isGuiAvailable() {
#ifdef BUILD_GUI
    return true;
#else
    return false;
#endif
}

int runGuiApplication(const core::Config& cfg, const std::vector<std::string>& args) {
#ifdef BUILD_GUI
    try {
        // Initialize Slint (init is handled automatically in Slint 1.13.0)
        
        // Suppress unused parameter warning
        (void)args;
        
        // Create main window
        MainWindow window(cfg);
        
        // Run the main window (this blocks until the window is closed)
        window.run();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "GUI Error: " << e.what() << std::endl;
        return 1;
    }
#else
    (void)cfg;  // Suppress unused parameter warning
    (void)args; // Suppress unused parameter warning
    std::cerr << "Error: GUI support not compiled. Please build with -DBUILD_GUI=ON" << std::endl;
    return 1;
#endif
}

} // namespace gui
} // namespace ak