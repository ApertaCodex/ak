#include "gui/gui.hpp"
#include "core/config.hpp"
#include <iostream>
#include <cstring>

#ifdef BUILD_GUI
#include "gui/mainwindow.hpp"
#include <wx/wx.h>
#include <wx/app.h>
#include <wx/msgdlg.h>
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
// Global config instance that will be passed to the app
static const core::Config* g_cfg = nullptr;
static std::vector<std::string> g_args;

// wxWidgets application class
class AkGuiApp : public wxApp {
public:
    virtual bool OnInit() override {
        try {
            // Set application metadata
            SetAppName("AK - API Key Manager");
            SetVendorName("AK");
            
            // Create and show main window
            MainWindow* window = new MainWindow(*g_cfg);
            window->SetTitle("AK - API Key Manager " AK_VERSION_STRING);
            window->Show();
            
            return true;
        } catch (const std::exception& e) {
            wxMessageBox(e.what(), "Error", wxICON_ERROR | wxOK);
            return false;
        }
    }
};

// Define the wxWidgets application
wxIMPLEMENT_APP_NO_MAIN(AkGuiApp);
#endif

int runGuiApplication(const core::Config& cfg, const std::vector<std::string>& args) {
#ifdef BUILD_GUI
    try {
        // Set global config for the application
        g_cfg = &cfg;
        g_args = args;
        
        // Convert args to wxWidgets format
        int argc = static_cast<int>(args.size()) + 1;
        wxChar** argv = new wxChar*[argc];
        
        // Set program name
        std::string progName = "ak";
        argv[0] = new wxChar[progName.size() + 1];
        #ifdef _WIN32
        // Windows uses wide chars
        mbstowcs(argv[0], progName.c_str(), progName.size() + 1);
        #else
        // Unix systems can use char directly
        std::strcpy(reinterpret_cast<char*>(argv[0]), progName.c_str());
        #endif
        
        // Copy arguments
        for (size_t i = 0; i < args.size(); ++i) {
            argv[i + 1] = new wxChar[args[i].size() + 1];
            #ifdef _WIN32
            mbstowcs(argv[i + 1], args[i].c_str(), args[i].size() + 1);
            #else
            std::strcpy(reinterpret_cast<char*>(argv[i + 1]), args[i].c_str());
            #endif
        }
        
        // Start the wxWidgets application
        int result = wxEntry(argc, argv);
        
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