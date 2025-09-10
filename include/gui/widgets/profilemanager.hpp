#pragma once

#ifdef BUILD_GUI

#include "core/config.hpp"
#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/statbox.h>
#include <wx/textctrl.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/dirdlg.h>
#include <wx/gauge.h>
#include <wx/timer.h>
#include <vector>
#include <string>

namespace ak {
namespace gui {
namespace widgets {

// Forward declarations
class ProfileImportExportDialog;

// Profile Manager Widget
class ProfileManagerWidget : public wxPanel
{
public:
    explicit ProfileManagerWidget(const core::Config& config, wxWindow *parent = nullptr);
    virtual ~ProfileManagerWidget();

    void RefreshProfiles();

    // Event handlers
    void OnCreateProfile(wxCommandEvent& event);
    void OnDeleteProfile(wxCommandEvent& event);
    void OnRenameProfile(wxCommandEvent& event);
    void OnDuplicateProfile(wxCommandEvent& event);
    void OnImportProfile(wxCommandEvent& event);
    void OnExportProfile(wxCommandEvent& event);
    void OnProfileSelectionChanged(wxCommandEvent& event);
    void OnProfileDoubleClicked(wxCommandEvent& event);
    void OnRefresh(wxCommandEvent& event);

private:
    void SetupUi();
    void SetupProfileList();
    void SetupControls();
    void SetupProfileDetails();
    void LoadProfiles();
    void UpdateProfileDetails(const wxString &profileName);
    void ShowProfileKeys(const wxString &profileName);
    bool ValidateProfileName(const wxString &name);
    void ShowError(const wxString &message);
    void ShowSuccess(const wxString &message);
    void SendStatusEvent(const wxString &message);

    // Configuration and data
    const core::Config& config;
    std::vector<wxString> availableProfiles;
    wxString currentProfile;
    std::map<wxString, int> profileKeyCounts;

    // UI components
    wxBoxSizer *mainSizer;
    wxBoxSizer *controlsSizer;

    // Profile list
    wxListBox *profileList;
    wxStaticText *profileListLabel;

    // Controls
    wxButton *createButton;
    wxButton *deleteButton;
    wxButton *renameButton;
    wxButton *duplicateButton;
    wxButton *importButton;
    wxButton *exportButton;
    wxButton *refreshButton;

    // Profile details
    wxStaticBoxSizer *detailsGroup;
    wxStaticText *profileNameLabel;
    wxStaticText *keyCountLabel;
    wxTextCtrl *keysText;

    // Status
    wxStaticText *statusLabel;

    // Custom event IDs
    enum {
        ID_CREATE_PROFILE = wxID_HIGHEST + 200,
        ID_DELETE_PROFILE,
        ID_RENAME_PROFILE,
        ID_DUPLICATE_PROFILE,
        ID_IMPORT_PROFILE,
        ID_EXPORT_PROFILE,
        ID_PROFILE_LIST,
        ID_REFRESH_PROFILES,
        ID_STATUS_MESSAGE
    };

    // wxWidgets event table
    wxDECLARE_EVENT_TABLE();
};

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI