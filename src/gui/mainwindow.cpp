#ifdef BUILD_GUI

#include "gui/mainwindow.hpp"
#include "gui/widgets/servicetester.hpp"
#include <wx/wx.h>
#include <wx/aboutdlg.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/msgdlg.h>
#include <wx/display.h>
#include <wx/colour.h>
#include <wx/font.h>
#include <wx/fontenum.h>

namespace ak {
namespace gui {

// Event table implementation
wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
    EVT_MENU(ID_About, MainWindow::OnAbout)
    EVT_MENU(ID_Help, MainWindow::OnHelp)
    EVT_MENU(wxID_EXIT, MainWindow::OnExit)
    EVT_COMMAND(ID_StatusMessage, wxEVT_COMMAND_TEXT_UPDATED, MainWindow::OnStatusMessage)
wxEND_EVENT_TABLE()

MainWindow::MainWindow(const core::Config& cfg, wxWindow *parent)
    : wxFrame(parent, wxID_ANY, "AK - API Key Manager", wxDefaultPosition, wxSize(1000, 700)),
      config(cfg), tabWidget(nullptr),
      keyManagerWidget(nullptr), profileManagerWidget(nullptr),
      serviceManagerWidget(nullptr), serviceTesterWidget(nullptr),
      settingsTab(nullptr)
{
    setupUi();
    setupMenuBar();
    setupStatusBar();
    setupTabs();

    // Set window properties
    SetMinSize(wxSize(800, 600));
    
    // Center window on screen
    CenterOnScreen();
}

MainWindow::~MainWindow()
{
    // wxWidgets handles cleanup automatically for child widgets
}

void MainWindow::setupUi()
{
    // Create main panel and sizer
    wxPanel *mainPanel = new wxPanel(this);
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
    mainPanel->SetSizer(mainSizer);
    
    // Create tab widget (notebook)
    tabWidget = new wxNotebook(mainPanel, wxID_ANY);
    mainSizer->Add(tabWidget, 1, wxEXPAND | wxALL, 10);
    
    // Set background color
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
}

void MainWindow::setupMenuBar()
{
    // Create menu bar
    wxMenuBar *menuBar = new wxMenuBar();
    
    // File menu
    wxMenu *fileMenu = new wxMenu();
    fileMenu->Append(wxID_EXIT, "E&xit\tAlt+F4", "Exit the application");
    menuBar->Append(fileMenu, "&File");
    
    // Help menu
    wxMenu *helpMenu = new wxMenu();
    helpMenu->Append(ID_Help, "&Help\tF1", "Show help documentation");
    helpMenu->AppendSeparator();
    helpMenu->Append(ID_About, "&About AK", "Show information about AK");
    menuBar->Append(helpMenu, "&Help");
    
    // Set the menu bar
    SetMenuBar(menuBar);
}

void MainWindow::setupStatusBar()
{
    // Create status bar
    CreateStatusBar(2);
    SetStatusText("Ready");
    
    // Add permanent widgets to status bar
    wxString backendText = config.gpgAvailable && !config.forcePlain ?
                           "Backend: GPG" : "Backend: Plain";
    SetStatusText(backendText, 1);
    
    // Version info is added in the status bar's second section
    SetStatusText("v" AK_VERSION_STRING, 1);
}

void MainWindow::setupTabs()
{
    // Key Manager Tab
    keyManagerWidget = new widgets::KeyManagerWidget(config, tabWidget);
    // We'll connect the event handler in the KeyManagerWidget implementation
    tabWidget->AddPage(keyManagerWidget, "Key Manager");

    // Profile Manager Tab
    profileManagerWidget = new widgets::ProfileManagerWidget(config, tabWidget);
    // We'll connect the event handler in the ProfileManagerWidget implementation
    tabWidget->AddPage(profileManagerWidget, "Profile Manager");

    // Service Manager Tab
    serviceManagerWidget = new widgets::ServiceManagerWidget(config, tabWidget);
    tabWidget->AddPage(serviceManagerWidget, "Service Manager");

    // Service Tester Tab
    serviceTesterWidget = new widgets::ServiceTesterWidget(config, tabWidget);
    tabWidget->AddPage(serviceTesterWidget, "Service Tester");

    // Settings Tab
    settingsTab = new wxPanel(tabWidget);
    wxBoxSizer *settingsSizer = new wxBoxSizer(wxVERTICAL);
    settingsTab->SetSizer(settingsSizer);
    
    // Backend Settings Group
    wxStaticBoxSizer *backendGroup = new wxStaticBoxSizer(wxVERTICAL, settingsTab, "Security Backend");
    
    wxString backendStatusText = wxString::Format("Current Backend: %s",
        config.gpgAvailable && !config.forcePlain ? "GPG Encryption" : "Plain Text");
    wxStaticText *backendStatus = new wxStaticText(settingsTab, wxID_ANY, backendStatusText);
    wxFont boldFont = backendStatus->GetFont();
    boldFont.SetWeight(wxFONTWEIGHT_BOLD);
    backendStatus->SetFont(boldFont);
    backendGroup->Add(backendStatus, 0, wxALL, 5);
    
    if (config.gpgAvailable) {
        wxStaticText *gpgInfo = new wxStaticText(settingsTab, wxID_ANY,
                                                "✅ GPG encryption is available and active");
        gpgInfo->SetForegroundColour(wxColour(0, 170, 0)); // Green
        backendGroup->Add(gpgInfo, 0, wxALL, 5);
    } else {
        wxStaticText *gpgWarning = new wxStaticText(settingsTab, wxID_ANY,
                                                   "⚠️ GPG not available - secrets stored as plain text");
        gpgWarning->SetForegroundColour(wxColour(255, 102, 0)); // Orange
        backendGroup->Add(gpgWarning, 0, wxALL, 5);
    }
    
    // File Paths Group
    wxStaticBoxSizer *pathsGroup = new wxStaticBoxSizer(wxVERTICAL, settingsTab, "File Locations");
    wxGridBagSizer *pathsGrid = new wxGridBagSizer(5, 5);
    
    // Helper to create path rows
    auto addPathRow = [&](int row, const wxString& label, const std::string& path) {
        wxStaticText *labelText = new wxStaticText(settingsTab, wxID_ANY, label);
        wxStaticText *pathText = new wxStaticText(settingsTab, wxID_ANY, path);
        
        // Try to use monospace font for path
        wxFont monoFont = wxFont(wxFontInfo().Family(wxFONTFAMILY_TELETYPE));
        pathText->SetFont(monoFont);
        
        pathsGrid->Add(labelText, wxGBPosition(row, 0), wxGBSpan(1, 1), wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
        pathsGrid->Add(pathText, wxGBPosition(row, 1), wxGBSpan(1, 1), wxEXPAND);
    };
    
    addPathRow(0, "Config Directory:", config.configDir);
    addPathRow(1, "Vault Path:", config.vaultPath);
    addPathRow(2, "Profiles Directory:", config.profilesDir);
    addPathRow(3, "Audit Log:", config.auditLogPath);
    
    pathsGroup->Add(pathsGrid, 1, wxEXPAND | wxALL, 5);
    
    // Application Info Group
    wxStaticBoxSizer *infoGroup = new wxStaticBoxSizer(wxVERTICAL, settingsTab, "Application Information");
    wxGridBagSizer *infoGrid = new wxGridBagSizer(5, 5);
    
    wxStaticText *versionLabel = new wxStaticText(settingsTab, wxID_ANY, "Version:");
    wxStaticText *versionText = new wxStaticText(settingsTab, wxID_ANY, "v" AK_VERSION_STRING);
    versionText->SetFont(boldFont);
    
    wxStaticText *instanceLabel = new wxStaticText(settingsTab, wxID_ANY, "Instance ID:");
    wxString instanceId = wxString(config.instanceId.substr(0, 8) + "...");
    wxStaticText *instanceText = new wxStaticText(settingsTab, wxID_ANY, instanceId);
    instanceText->SetFont(monoFont);
    
    infoGrid->Add(versionLabel, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
    infoGrid->Add(versionText, wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND);
    infoGrid->Add(instanceLabel, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
    infoGrid->Add(instanceText, wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND);
    
    infoGroup->Add(infoGrid, 1, wxEXPAND | wxALL, 5);
    
    // Clipboard Tools Detection
    wxStaticBoxSizer *toolsGroup = new wxStaticBoxSizer(wxVERTICAL, settingsTab, "System Tools");
    
    // Check for clipboard tools
    std::vector<std::string> clipboardTools = {"pbcopy", "wl-copy", "xclip"};
    bool foundClipboard = false;
    
    for (const auto& tool : clipboardTools) {
        if (core::commandExists(tool)) {
            wxStaticText *toolLabel = new wxStaticText(settingsTab, wxID_ANY,
                                                      wxString::Format("✅ Clipboard: %s", tool));
            toolLabel->SetForegroundColour(wxColour(0, 170, 0)); // Green
            toolsGroup->Add(toolLabel, 0, wxALL, 5);
            foundClipboard = true;
            break;
        }
    }
    
    if (!foundClipboard) {
        wxStaticText *noClipboard = new wxStaticText(settingsTab, wxID_ANY,
                                                   "⚠️ No clipboard tools found (install xclip, wl-clipboard, or pbcopy)");
        noClipboard->SetForegroundColour(wxColour(255, 102, 0)); // Orange
        toolsGroup->Add(noClipboard, 0, wxALL, 5);
    }
    
    // Add all groups to layout
    settingsSizer->Add(backendGroup, 0, wxEXPAND | wxALL, 10);
    settingsSizer->Add(pathsGroup, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    settingsSizer->Add(infoGroup, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    settingsSizer->Add(toolsGroup, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    settingsSizer->AddStretch(1); // Push content to top
    
    tabWidget->AddPage(settingsTab, "Settings");

    // Set default tab
    tabWidget->SetSelection(0);
}

void MainWindow::OnAbout(wxCommandEvent& WXUNUSED(event))
{
    wxAboutDialogInfo aboutInfo;
    aboutInfo.SetName("AK - API Key Manager");
    aboutInfo.SetVersion(AK_VERSION_STRING);
    aboutInfo.SetDescription("A secure tool for managing API keys and environment variables.");
    
    wxString backendInfo = wxString::Format("Backend: %s\nConfig Directory: %s",
        config.gpgAvailable ? "GPG Encryption" : "Plain Text",
        config.configDir);
    aboutInfo.SetDescription(aboutInfo.GetDescription() + "\n\n" + backendInfo);
    
    wxAboutBox(aboutInfo);
}

void MainWindow::OnHelp(wxCommandEvent& WXUNUSED(event))
{
    wxMessageBox(
        "AK GUI Help\n\n"
        "Use the tabs to navigate between different features:\n\n"
        "• Key Manager: Manage your API keys and test endpoints\n"
        "• Profile Manager: Create and manage key profiles\n"
        "• Service Manager: Add and manage custom API services\n"
        "• Service Tester: Test API services and monitor connectivity\n"
        "• Settings: Configure application preferences\n\n"
        "For detailed documentation, visit the project repository.",
        "Help", wxICON_INFORMATION | wxOK, this);
}

void MainWindow::OnExit(wxCommandEvent& WXUNUSED(event))
{
    Close(true);
}

void MainWindow::OnStatusMessage(wxCommandEvent& event)
{
    SetStatusText(event.GetString(), 0);
    
    // Reset status after 3 seconds
    wxTimer* timer = new wxTimer(this);
    timer->StartOnce(3000);
    timer->Bind(wxEVT_TIMER, [this, timer](wxTimerEvent&) {
        SetStatusText("Ready", 0);
        timer->Destroy();
    });
}

} // namespace gui
} // namespace ak

#endif // BUILD_GUI