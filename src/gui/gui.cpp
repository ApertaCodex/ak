#include "gui/gui.hpp"
#include "core/config.hpp"
#include <iostream>
#include <cstring>

#ifdef BUILD_GUI
#include "gui/mainwindow.hpp"
#include <QApplication>
#include <QMessageBox>
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
        // Convert args to Qt format
        int argc = static_cast<int>(args.size()) + 1;
        char** argv = new char*[argc];
        
        // Set program name
        std::string progName = "ak";
        argv[0] = new char[progName.size() + 1];
        std::strcpy(argv[0], progName.c_str());
        
        // Copy arguments
        for (size_t i = 0; i < args.size(); ++i) {
            argv[i + 1] = new char[args[i].size() + 1];
            std::strcpy(argv[i + 1], args[i].c_str());
        }
        
        // Create QApplication
        QApplication app(argc, argv);
        
        // Set application properties
        app.setApplicationName("AK - API Key Manager");
        app.setApplicationVersion(AK_VERSION_STRING);
        app.setOrganizationName("AK");
        app.setApplicationDisplayName("AK GUI");
        
        // Create and show main window
        MainWindow window(cfg);
        window.show();
        
        // Start event loop
        int result = app.exec();
        
        // Cleanup
        for (int i = 0; i < argc; ++i) {
            delete[] argv[i];
        }
        delete[] argv;
        
        return result;
        
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