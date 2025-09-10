#pragma once

#ifdef BUILD_GUI

#include "core/config.hpp"
#include "gui/widgets/keymanager.hpp"
#include "gui/widgets/profilemanager.hpp"
#include "gui/widgets/servicemanager.hpp"
#include "gui/widgets/servicetester.hpp"
#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/panel.h>
#include <wx/menu.h>
#include <wx/statusbr.h>

namespace ak {
namespace gui {

class MainWindow : public wxFrame
{
public:
    explicit MainWindow(const core::Config& cfg, wxWindow *parent = nullptr);
    virtual ~MainWindow();

private:
    // Event handlers
    void OnAbout(wxCommandEvent& event);
    void OnHelp(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnStatusMessage(wxCommandEvent& event);

    // Setup methods
    void setupUi();
    void setupMenuBar();
    void setupStatusBar();
    void setupTabs();

    const core::Config& config;

    // UI Components
    wxNotebook *tabWidget;
    widgets::KeyManagerWidget *keyManagerWidget;
    widgets::ProfileManagerWidget *profileManagerWidget;
    widgets::ServiceManagerWidget *serviceManagerWidget;
    widgets::ServiceTesterWidget *serviceTesterWidget;
    wxPanel *settingsTab;

    // wxWidgets DECLARE_EVENT_TABLE() alternative
    wxDECLARE_EVENT_TABLE();
    
    // Custom event ID
    enum {
        ID_About = wxID_HIGHEST + 1,
        ID_Help,
        ID_StatusMessage
    };
};

} // namespace gui
} // namespace ak

#endif // BUILD_GUI