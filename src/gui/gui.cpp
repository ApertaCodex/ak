#include "gui/gui.hpp"
#include "core/config.hpp"
#include <iostream>
#include <cstring>

#ifdef BUILD_GUI
#include "gui/mainwindow.hpp"
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QLoggingCategory>
#include <cstdio>
#include <cstdlib>
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

#ifdef BUILD_GUI
// Verbose handler used only when debug is enabled
void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    bool debugEnabled = (getenv("AK_DEBUG") != nullptr || getenv("AK_DEBUG_GUI") != nullptr);
    if (!debugEnabled) {
        return; // Safety: do nothing unless explicitly in debug
    }

    const char *typeStr = "INFO";
    switch (type) {
        case QtDebugMsg:    typeStr = "DEBUG";    break;
        case QtWarningMsg:  typeStr = "WARNING";  break;
        case QtCriticalMsg: typeStr = "CRITICAL"; break;
        case QtFatalMsg:    typeStr = "FATAL";    break;
        case QtInfoMsg:     typeStr = "INFO";     break;
    }

    std::cerr << "[GUI] [" << typeStr << "]";
    if (context.category && strlen(context.category) > 0) {
        std::cerr << " [" << context.category << "]";
    }
    std::cerr << " " << msg.toStdString();
    if (context.file && debugEnabled) {
        std::cerr << " (" << context.file << ":" << context.line << ")";
    }
    std::cerr << std::endl;

    if (type == QtFatalMsg) {
        abort();
    }
}

// Fully quiet handler for non-debug runs
void qtQuietMessageHandler(QtMsgType, const QMessageLogContext &, const QString &)
{
    // Intentionally no-op to suppress all Qt logging in non-debug mode
}
#endif

int runGuiApplication(const core::Config& cfg, const std::vector<std::string>& args) {
#ifdef BUILD_GUI
    try {
        // Check for debug environment variables
        bool debugEnabled = (getenv("AK_DEBUG") != nullptr || getenv("AK_DEBUG_GUI") != nullptr);
        if (debugEnabled) {
            std::cerr << "[GUI] Debug mode enabled" << std::endl;
            std::cerr << "[GUI] Config dir: " << cfg.configDir << std::endl;
            std::cerr << "[GUI] GPG available: " << (cfg.gpgAvailable ? "yes" : "no") << std::endl;
        }
        
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
        
        // Install message handler: verbose in debug, quiet otherwise
        if (debugEnabled) {
            qInstallMessageHandler(qtMessageHandler);
            // Enable debug logging categories if debug is enabled
            QLoggingCategory::setFilterRules("*.debug=true");
            qDebug() << "GUI debug logging enabled";
        } else {
            qInstallMessageHandler(qtQuietMessageHandler);
            // Disable all logging categories explicitly
            QLoggingCategory::setFilterRules("*.debug=false\n*.info=false\n*.warning=false\n*.critical=false");
        }
        
        // Set application properties
        app.setApplicationName("AK - API Key Manager");
        app.setApplicationVersion(AK_VERSION_STRING);
        app.setOrganizationName("AK");
        app.setApplicationDisplayName("AK GUI");
        
        if (debugEnabled) {
            std::cerr << "[GUI] Creating main window..." << std::endl;
        }
        
        // Create and show main window
        MainWindow window(cfg);
        window.show();
        
        if (debugEnabled) {
            std::cerr << "[GUI] Main window shown, starting event loop..." << std::endl;
        }
        
        // Start event loop
        int result = app.exec();
        
        if (debugEnabled) {
            std::cerr << "[GUI] Event loop ended with code: " << result << std::endl;
        }
        
        // Cleanup
        for (int i = 0; i < argc; ++i) {
            delete[] argv[i];
        }
        delete[] argv;
        
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "[GUI] Error: " << e.what() << std::endl;
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