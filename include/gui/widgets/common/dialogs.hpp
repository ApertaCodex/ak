#pragma once

#ifdef BUILD_GUI

#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/choice.h>
#include <wx/button.h>
#include <wx/gauge.h>
#include <wx/statline.h>
#include <wx/filedlg.h>

namespace ak {
namespace gui {
namespace widgets {

// Forward declaration
class SecureInputWidget;

// Base dialog class with common functionality
class BaseDialog : public wxDialog
{
public:
    explicit BaseDialog(const wxString &title, wxWindow *parent = nullptr);

protected:
    void setupButtons(int flags);
    void addFormRow(const wxString &label, wxWindow *widget);
    void addWidget(wxWindow *widget);
    void setValid(bool valid);

    // Event handlers
    void OnOK(wxCommandEvent &event);
    void OnCancel(wxCommandEvent &event);

    wxDECLARE_EVENT_TABLE();

private:
    wxBoxSizer *mainSizer;
    wxFlexGridSizer *formSizer;
    wxBoxSizer *buttonSizer;
    wxButton *okButton;
    wxButton *cancelButton;
};

// Key Add/Edit Dialog
class KeyEditDialog : public BaseDialog
{
public:
    explicit KeyEditDialog(wxWindow *parent = nullptr);
    KeyEditDialog(const wxString &keyName, const wxString &keyValue, 
                  const wxString &service, wxWindow *parent = nullptr);

    wxString getKeyName() const;
    wxString getKeyValue() const;
    wxString getService() const;

    // Event handlers
    void OnInputChanged(wxCommandEvent &event);

    wxDECLARE_EVENT_TABLE();

private:
    void setupUi();
    void populateServices();
    void validateInput();

    wxTextCtrl *nameEdit;
    SecureInputWidget *valueEdit;
    wxChoice *serviceChoice;
    bool editMode;
};

// Profile Create Dialog
class ProfileCreateDialog : public BaseDialog
{
public:
    explicit ProfileCreateDialog(wxWindow *parent = nullptr);

    wxString getProfileName() const;

    // Event handlers
    void OnNameChanged(wxCommandEvent &event);

    wxDECLARE_EVENT_TABLE();

private:
    void setupUi();
    void validateInput();

    wxTextCtrl *nameEdit;
};

// Confirmation Dialog helper class
class ConfirmationDialog
{
public:
    static bool confirm(wxWindow *parent, const wxString &title, 
                      const wxString &message, const wxString &details = wxEmptyString);

    static void showInfo(wxWindow *parent, const wxString &title, const wxString &message);
    static void showError(wxWindow *parent, const wxString &title, const wxString &message);
    static void showWarning(wxWindow *parent, const wxString &title, const wxString &message);
};

// Progress Dialog
class ProgressDialog : public wxDialog
{
public:
    explicit ProgressDialog(const wxString &title, wxWindow *parent = nullptr);

    void setProgress(int value);
    void setMaximum(int maximum);
    void setLabelText(const wxString &text);
    void setDetailText(const wxString &text);

private:
    void setupUi();

    wxStaticText *statusLabel;
    wxGauge *progressBar;
    wxTextCtrl *detailsText;
};

// Profile Import/Export Dialog
class ProfileImportExportDialog : public BaseDialog
{
public:
    enum Mode { Import, Export };
    
    explicit ProfileImportExportDialog(Mode mode, wxWindow *parent = nullptr);
    explicit ProfileImportExportDialog(Mode mode, const wxString &defaultProfileName, wxWindow *parent = nullptr);

    wxString getFilePath() const;
    wxString getFormat() const;
    wxString getProfileName() const;
    
    void setDefaultProfileName(const wxString &profileName);

    // Event handlers
    void OnBrowseFile(wxCommandEvent &event);
    void OnInputChanged(wxCommandEvent &event);

    wxDECLARE_EVENT_TABLE();

private:
    void setupUi();
    void validateInput();

    Mode mode;
    wxTextCtrl *filePathEdit;
    wxButton *browseButton;
    wxChoice *formatChoice;
    wxTextCtrl *profileNameEdit;
};

// Event IDs
enum {
    ID_KEY_NAME = wxID_HIGHEST + 2000,
    ID_KEY_VALUE,
    ID_KEY_SERVICE,
    ID_PROFILE_NAME,
    ID_BROWSE_BUTTON,
    ID_FILE_PATH,
    ID_FORMAT_CHOICE,
    ID_PROFILE_NAME_EDIT
};

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI