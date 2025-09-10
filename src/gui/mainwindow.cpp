#ifdef BUILD_GUI

#include "gui/mainwindow.hpp"
#include <memory>
#include <iostream>
#include <functional>
#include <string>
#include <slint.h>
#include "main_slint.h"  // Generated from main.slint

namespace ak {
namespace gui {

// MainWindow implementation using Slint
MainWindow::MainWindow(const core::Config& cfg)
    : config(cfg)
{
    std::cerr << "DEBUG: MainWindow constructor start" << std::endl;
    
    // Load the Slint UI from the compiled .slint file
    std::cerr << "DEBUG: Creating Slint UI" << std::endl;
    ui = std::make_unique<::MainWindow>();
    
    // Set up properties
    std::cerr << "DEBUG: Setting UI properties" << std::endl;
    ui->set_gpg_available(config.gpgAvailable);
    ui->set_version(slint::SharedString(AK_VERSION_STRING));
    ui->set_status_message(slint::SharedString("Ready"));
    
    // Connect callbacks from Slint UI to C++ implementation
    std::cerr << "DEBUG: Setting up callbacks" << std::endl;
    setupCallbacks();
    
    std::cerr << "DEBUG: MainWindow constructor end" << std::endl;
}

MainWindow::~MainWindow()
{
    // Slint handles cleanup through smart pointers
}

void MainWindow::setupCallbacks()
{
    // Connect main window callbacks - these match our simplified Slint UI
    ui->on_exit_application([this]() {
        exitApplication();
    });
    
    ui->on_show_help([this]() {
        showHelp();
    });
    
    ui->on_show_about([this]() {
        showAbout();
    });
}

void MainWindow::run()
{
    std::cerr << "DEBUG: MainWindow::run start" << std::endl;
    
    // Run the Slint UI using the internal run method which handles the event loop
    try {
        std::cerr << "DEBUG: Running UI's run() method directly" << std::endl;
        
        // Use the UI's run method directly instead of show() + run_event_loop()
        // This is how the Slint C++ template handles it
        ui->run();
        
        std::cerr << "DEBUG: UI run() method returned" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "DEBUG: Exception in run: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "DEBUG: Unknown exception in run" << std::endl;
    }
    
    std::cerr << "DEBUG: MainWindow::run end" << std::endl;
}

void MainWindow::showAbout()
{
    // Show about dialog using simple message
    std::string aboutText = "AK - API Key Manager v";
    aboutText += AK_VERSION_STRING;
    aboutText += "\n\nA secure tool for managing API keys and environment variables.\n\n";
    aboutText += "Backend: ";
    aboutText += (config.gpgAvailable ? "GPG Encryption" : "Plain Text");
    aboutText += "\nConfig Directory: ";
    aboutText += config.configDir;
    
    // Display in the UI status message
    ui->set_status_message(slint::SharedString(aboutText));
}

void MainWindow::showHelp()
{
    // Show help dialog using simple message
    std::string helpText = "AK GUI Help\n\n";
    helpText += "Use the tabs to navigate between different features:\n\n";
    helpText += "• Key Manager: Manage your API keys and test endpoints\n";
    helpText += "• Profile Manager: Create and manage key profiles\n";
    helpText += "• Service Manager: Add and manage custom API services\n";
    helpText += "• Settings: Configure application preferences\n\n";
    helpText += "For detailed documentation, visit the project repository.";
    
    // Display in the UI status message
    ui->set_status_message(slint::SharedString(helpText));
}

void MainWindow::exitApplication()
{
    // Terminate the Slint UI and exit
    // In Slint 1.13.0, hide() will cause the run() method to return
    ui->hide();
    
    // We can also use this to terminate the event loop directly if needed
    slint::quit_event_loop();
}

bool MainWindow::copyToClipboard(const std::string& text)
{
    // Simplified clipboard handling - using system tools
    std::vector<std::string> clipboardTools = {"pbcopy", "wl-copy", "xclip"};
    
    for (const auto& tool : clipboardTools) {
        if (core::commandExists(tool)) {
            std::string command;
            
            if (tool == "xclip") {
                command = "echo -n '" + text + "' | xclip -selection clipboard";
            } else if (tool == "wl-copy") {
                command = "echo -n '" + text + "' | wl-copy";
            } else if (tool == "pbcopy") {
                command = "echo -n '" + text + "' | pbcopy";
            }
            
            int result = system(command.c_str());
            return (result == 0);
        }
    }
    
    return false; // No clipboard tool found
}

} // namespace gui
} // namespace ak

#endif // BUILD_GUI