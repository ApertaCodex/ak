#ifdef BUILD_GUI

#include "gui/mainwindow.hpp"
#include <memory>
#include <iostream>
#include <functional>
#include <string>
#include <slint.h>
#include "slint_generated.h"

namespace ak {
namespace gui {

// MainWindow implementation using Slint
MainWindow::MainWindow(const core::Config& cfg)
    : config(cfg)
{
    // Load the Slint UI from the compiled .slint file
    ui = std::make_unique<slint::MainWindow>();
    
    // Set up properties
    ui->set_gpg_available(config.gpgAvailable);
    ui->set_version(std::string(AK_VERSION_STRING));
    ui->set_status_message("Ready");
    
    // Connect callbacks from Slint UI to C++ implementation
    setupCallbacks();
}

MainWindow::~MainWindow()
{
    // Slint handles cleanup through smart pointers
}

void MainWindow::setupCallbacks()
{
    // Connect main window callbacks
    ui->on_exit_application([this]() {
        exitApplication();
    });
    
    ui->on_show_help([this]() {
        showHelp();
    });
    
    ui->on_show_about([this]() {
        showAbout();
    });
    
    // Connect KeyManagerWidget callbacks
    auto keyManager = ui->get_key_manager();
    keyManager.on_add_key([this]() {
        // Implementation for adding a key
        ui->set_status_message("Adding new key...");
    });
    
    keyManager.on_edit_key([this]() {
        // Implementation for editing a key
        ui->set_status_message("Editing selected key...");
    });
    
    keyManager.on_delete_key([this]() {
        // Implementation for deleting a key
        ui->set_status_message("Deleting selected key...");
    });
    
    keyManager.on_test_key([this]() {
        // Implementation for testing a key
        ui->set_status_message("Testing selected key...");
    });
    
    keyManager.on_copy_to_clipboard([this](const slint::SharedString& key_name) {
        // Implementation for copying to clipboard
        std::string key = key_name.to_std_string();
        bool success = copyToClipboard(key);
        if (success) {
            ui->set_status_message("Copied " + key + " to clipboard");
        } else {
            ui->set_status_message("Failed to copy to clipboard");
        }
    });
    
    // Connect ProfileManagerWidget callbacks
    auto profileManager = ui->get_profile_manager();
    profileManager.on_add_profile([this]() {
        ui->set_status_message("Adding new profile...");
    });
    
    profileManager.on_edit_profile([this]() {
        ui->set_status_message("Editing selected profile...");
    });
    
    profileManager.on_delete_profile([this]() {
        ui->set_status_message("Deleting selected profile...");
    });
    
    profileManager.on_export_profile([this]() {
        ui->set_status_message("Exporting selected profile...");
    });
    
    profileManager.on_import_profile([this]() {
        ui->set_status_message("Importing profile...");
    });
    
    // Connect ServiceManagerWidget callbacks
    auto serviceManager = ui->get_service_manager();
    serviceManager.on_add_service([this]() {
        ui->set_status_message("Adding new service...");
    });
    
    serviceManager.on_edit_service([this]() {
        ui->set_status_message("Editing selected service...");
    });
    
    serviceManager.on_delete_service([this]() {
        ui->set_status_message("Deleting selected service...");
    });
    
    serviceManager.on_test_service([this]() {
        ui->set_status_message("Testing selected service...");
    });
    
    // Configure settings widget with configuration values
    auto settingsWidget = ui->get_settings();
    settingsWidget.set_config_dir(std::string(config.configDir));
    settingsWidget.set_vault_path(std::string(config.vaultPath));
    settingsWidget.set_profiles_dir(std::string(config.profilesDir));
    settingsWidget.set_audit_log_path(std::string(config.auditLogPath));
    settingsWidget.set_instance_id(std::string(config.instanceId));
    settingsWidget.set_gpg_available(config.gpgAvailable);
    settingsWidget.set_version(std::string(AK_VERSION_STRING));
}

void MainWindow::run()
{
    // Show and run the Slint UI
    ui->run();
}

void MainWindow::showAbout()
{
    // Show about dialog using Slint
    slint::MessageBox aboutBox;
    aboutBox.set_title(slint::SharedString("About AK"));
    
    std::string aboutText = "AK - API Key Manager v";
    aboutText += AK_VERSION_STRING;
    aboutText += "\n\nA secure tool for managing API keys and environment variables.\n\n";
    aboutText += "Backend: ";
    aboutText += (config.gpgAvailable ? "GPG Encryption" : "Plain Text");
    aboutText += "\nConfig Directory: ";
    aboutText += config.configDir;
    
    aboutBox.set_text(slint::SharedString(aboutText));
    aboutBox.set_buttons(slint::StandardButton::Ok);
    aboutBox.run();
}

void MainWindow::showHelp()
{
    // Show help dialog using Slint
    slint::MessageBox helpBox;
    helpBox.set_title(slint::SharedString("Help"));
    
    std::string helpText = "AK GUI Help\n\n";
    helpText += "Use the tabs to navigate between different features:\n\n";
    helpText += "• Key Manager: Manage your API keys and test endpoints\n";
    helpText += "• Profile Manager: Create and manage key profiles\n";
    helpText += "• Service Manager: Add and manage custom API services\n";
    helpText += "• Settings: Configure application preferences\n\n";
    helpText += "For detailed documentation, visit the project repository.";
    
    helpBox.set_text(slint::SharedString(helpText));
    helpBox.set_buttons(slint::StandardButton::Ok);
    helpBox.run();
}

void MainWindow::exitApplication()
{
    // Terminate the Slint UI and exit
    ui->quit();
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