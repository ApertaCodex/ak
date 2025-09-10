#include "gui/gui.hpp"
#include "core/config.hpp"
#include <iostream>
#include <cstring>

#ifdef BUILD_GUI
#include "gui/mainwindow.hpp"
#include <slint.h>
#include "main_slint.h"  // Generated from main.slint
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
        std::cerr << "DEBUG: Starting GUI application" << std::endl;
        
        // Set environment variables to help with display issues
        setenv("SLINT_NO_HARDWARE_ACCELERATION", "1", 1);
        setenv("SLINT_FULLSPEED_RENDERING", "1", 1);
        
        // Initialize Slint debugging (for troubleshooting)
        setenv("SLINT_DEBUG", "1", 1);
        
        std::cerr << "DEBUG: Slint initialization" << std::endl;
        
        // Suppress unused parameter warning
        (void)args;
        
        // Create the Slint UI directly (similar to the template approach)
        std::cerr << "DEBUG: Creating Slint UI" << std::endl;
        auto ui = ::MainWindow::create();
        
        // Set up properties
        std::cerr << "DEBUG: Setting UI properties" << std::endl;
        ui->set_gpg_available(cfg.gpgAvailable);
        ui->set_version(slint::SharedString(AK_VERSION_STRING));
        ui->set_status_message(slint::SharedString("Ready"));
        
        // Connect callbacks
        std::cerr << "DEBUG: Setting up callbacks" << std::endl;
        ui->on_exit_application([&]() {
            std::cerr << "DEBUG: Exit callback triggered" << std::endl;
            ui->hide();
            slint::quit_event_loop();
        });
        
        ui->on_show_help([&]() {
            std::string helpText = "AK GUI Help\n\n";
            helpText += "Use the tabs to navigate between different features:\n\n";
            helpText += "• Key Manager: Manage your API keys and test endpoints\n";
            helpText += "• Profile Manager: Create and manage key profiles\n";
            helpText += "• Service Manager: Add and manage custom API services\n";
            helpText += "• Settings: Configure application preferences\n\n";
            helpText += "For detailed documentation, visit the project repository.";
            
            ui->set_status_message(slint::SharedString(helpText));
        });
        
        ui->on_show_about([&]() {
            std::string aboutText = "AK - API Key Manager v";
            aboutText += AK_VERSION_STRING;
            aboutText += "\n\nA secure tool for managing API keys and environment variables.\n\n";
            aboutText += "Backend: ";
            aboutText += (cfg.gpgAvailable ? "GPG Encryption" : "Plain Text");
            aboutText += "\nConfig Directory: ";
            aboutText += cfg.configDir;
            
            ui->set_status_message(slint::SharedString(aboutText));
        });
        
        // Run the UI (this blocks until the window is closed)
        std::cerr << "DEBUG: Running UI directly" << std::endl;
        ui->run();
        
        std::cerr << "DEBUG: GUI application completed" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "GUI Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "GUI Error: Unknown exception" << std::endl;
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