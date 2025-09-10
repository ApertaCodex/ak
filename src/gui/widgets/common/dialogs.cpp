#ifdef BUILD_GUI

#include "gui/widgets/common/dialogs.hpp"
#include "gui/widgets/common/secureinput.hpp"
#include "services/services.hpp"
#include <wx/valtext.h>
#include <wx/msgdlg.h>
#include <wx/filedlg.h>
#include <wx/stdpaths.h>
#include <wx/regex.h>
#include <wx/filename.h>

namespace ak {
namespace gui {
namespace widgets {

// BaseDialog Implementation
wxBEGIN_EVENT_TABLE(BaseDialog, wxDialog)
    EVT_BUTTON(wxID_OK, BaseDialog::OnOK)
    EVT_BUTTON(wxID_CANCEL, BaseDialog::OnCancel)
wxEND_EVENT_TABLE()

BaseDialog::BaseDialog(const wxString &title, wxWindow *parent)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(400, 300),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    // Set up the layout
    mainSizer = new wxBoxSizer(wxVERTICAL);
    formSizer = new wxFlexGridSizer(2, 5, 9); // 2 columns, 5px vertical gap, 9px horizontal gap
    formSizer->AddGrowableCol(1, 1); // Second column can grow
    
    mainSizer->Add(formSizer, 1, wxEXPAND | wxALL, 10);
    
    // Add stretch space before buttons
    mainSizer->AddStretchSpacer();
    
    SetSizer(mainSizer);
}

void BaseDialog::setupButtons(int flags)
{
    buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Convert flags to wxWidgets standard flags
    if (flags & wxID_OK) {
        okButton = new wxButton(this, wxID_OK);
        buttonSizer->Add(okButton, 0, wxALL, 5);
    }
    
    if (flags & wxID_CANCEL) {
        cancelButton = new wxButton(this, wxID_CANCEL);
        buttonSizer->Add(cancelButton, 0, wxALL, 5);
    }
    
    mainSizer->Add(buttonSizer, 0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, 10);
}

void BaseDialog::addFormRow(const wxString &label, wxWindow *widget)
{
    formSizer->Add(new wxStaticText(this, wxID_ANY, label), 0, 
                   wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
    formSizer->Add(widget, 1, wxEXPAND);
}

void BaseDialog::addWidget(wxWindow *widget)
{
    formSizer->Add(1, 1); // Empty first column
    formSizer->Add(widget, 1, wxEXPAND);
}

void BaseDialog::setValid(bool valid)
{
    if (okButton) {
        okButton->Enable(valid);
    }
}

void BaseDialog::OnOK(wxCommandEvent &event)
{
    EndModal(wxID_OK);
}

void BaseDialog::OnCancel(wxCommandEvent &event)
{
    EndModal(wxID_CANCEL);
}

// KeyEditDialog Implementation
wxBEGIN_EVENT_TABLE(KeyEditDialog, BaseDialog)
    EVT_TEXT(ID_KEY_NAME, KeyEditDialog::OnInputChanged)
    EVT_COMMAND(ID_KEY_VALUE, wxEVT_COMMAND_TEXT_UPDATED, KeyEditDialog::OnInputChanged)
    EVT_CHOICE(ID_KEY_SERVICE, KeyEditDialog::OnInputChanged)
wxEND_EVENT_TABLE()

KeyEditDialog::KeyEditDialog(wxWindow *parent)
    : BaseDialog("Add New Key", parent), nameEdit(nullptr), valueEdit(nullptr), 
      serviceChoice(nullptr), editMode(false)
{
    setupUi();
}

KeyEditDialog::KeyEditDialog(const wxString &keyName, const wxString &keyValue, 
                           const wxString &service, wxWindow *parent)
    : BaseDialog("Edit Key", parent), nameEdit(nullptr), valueEdit(nullptr),
      serviceChoice(nullptr), editMode(true)
{
    setupUi();
    
    nameEdit->SetValue(keyName);
    valueEdit->SetText(keyValue);
    
    for (unsigned int i = 0; i < serviceChoice->GetCount(); i++) {
        if (serviceChoice->GetString(i).Lower() == service.Lower() ||
            serviceChoice->GetClientData(i) && 
            *static_cast<wxString*>(serviceChoice->GetClientData(i)) == service) {
            serviceChoice->SetSelection(i);
            break;
        }
    }
    
    // In edit mode, don't allow changing the name
    nameEdit->SetEditable(!editMode);
}

void KeyEditDialog::setupUi()
{
    nameEdit = new wxTextCtrl(this, ID_KEY_NAME);
    nameEdit->SetHint("Enter key name (e.g., OPENAI_API_KEY)");
    
    valueEdit = new SecureInputWidget(this);
    valueEdit->SetId(ID_KEY_VALUE);
    valueEdit->SetPlaceholderText("Enter key value");
    
    serviceChoice = new wxChoice(this, ID_KEY_SERVICE);
    populateServices();
    
    addFormRow("Key Name:", nameEdit);
    addFormRow("Key Value:", valueEdit);
    addFormRow("Service:", serviceChoice);
    
    setupButtons(wxID_OK | wxID_CANCEL);
    
    // Set name field validator for uppercase letters, numbers, and underscores
    wxTextValidator validator(wxFILTER_ASCII, nullptr);
    wxRegEx nameRegex("^[A-Z][A-Z0-9_]*$");
    // Note: wxWidgets doesn't have a direct regex validator like Qt,
    // so complete validation is done in validateInput()
    nameEdit->SetValidator(validator);
    
    validateInput();
}

void KeyEditDialog::populateServices()
{
    serviceChoice->Append("Custom", new wxString("custom"));
    
    // Get known services from the services module
    auto knownServices = ak::services::getKnownServiceKeys();
    wxArrayString services;
    for (const auto& service : knownServices) {
        services.Add(wxString::FromUTF8(service));
    }
    services.Sort();
    
    for (const wxString& service : services) {
        wxString displayName = service;
        // Convert snake_case to Title Case
        displayName.Replace("_", " ");
        wxArrayString parts = wxSplit(displayName, ' ');
        for (wxString& part : parts) {
            if (!part.IsEmpty()) {
                part[0] = wxToupper(part[0]);
                for (size_t i = 1; i < part.Length(); ++i) {
                    part[i] = wxTolower(part[i]);
                }
            }
        }
        displayName = wxJoin(parts, ' ');
        
        serviceChoice->Append(displayName, new wxString(service.Lower()));
    }
    
    // Select first item
    if (serviceChoice->GetCount() > 0) {
        serviceChoice->SetSelection(0);
    }
}

wxString KeyEditDialog::getKeyName() const
{
    return nameEdit->GetValue().Trim().Upper();
}

wxString KeyEditDialog::getKeyValue() const
{
    return valueEdit->GetText();
}

wxString KeyEditDialog::getService() const
{
    int sel = serviceChoice->GetSelection();
    if (sel != wxNOT_FOUND && serviceChoice->GetClientData(sel)) {
        return *static_cast<wxString*>(serviceChoice->GetClientData(sel));
    }
    return "custom";
}

void KeyEditDialog::OnInputChanged(wxCommandEvent &event)
{
    validateInput();
    event.Skip();
}

void KeyEditDialog::validateInput()
{
    bool valid = !nameEdit->GetValue().Trim().IsEmpty() && 
                 !valueEdit->GetText().IsEmpty();
    
    // Additional name validation
    if (valid && !editMode) {
        wxString name = nameEdit->GetValue().Trim();
        wxRegEx nameRegex("^[A-Z][A-Z0-9_]*$");
        valid = name.Length() > 0 && nameRegex.Matches(name);
    }
    
    setValid(valid);
}

// ProfileCreateDialog Implementation
wxBEGIN_EVENT_TABLE(ProfileCreateDialog, BaseDialog)
    EVT_TEXT(ID_PROFILE_NAME, ProfileCreateDialog::OnNameChanged)
wxEND_EVENT_TABLE()

ProfileCreateDialog::ProfileCreateDialog(wxWindow *parent)
    : BaseDialog("Create New Profile", parent), nameEdit(nullptr)
{
    setupUi();
}

void ProfileCreateDialog::setupUi()
{
    nameEdit = new wxTextCtrl(this, ID_PROFILE_NAME);
    nameEdit->SetHint("Enter profile name");
    
    addFormRow("Profile Name:", nameEdit);
    
    setupButtons(wxID_OK | wxID_CANCEL);
    
    // Set validator for profile names - alphanumeric, dash, underscore
    wxTextValidator validator(wxFILTER_ASCII, nullptr);
    nameEdit->SetValidator(validator);
    
    validateInput();
}

wxString ProfileCreateDialog::getProfileName() const
{
    return nameEdit->GetValue().Trim();
}

void ProfileCreateDialog::OnNameChanged(wxCommandEvent &event)
{
    validateInput();
    event.Skip();
}

void ProfileCreateDialog::validateInput()
{
    wxString name = nameEdit->GetValue().Trim();
    wxRegEx profileRegex("^[a-zA-Z0-9_-]+$");
    bool valid = !name.IsEmpty() && name.Length() >= 2 && profileRegex.Matches(name);
    setValid(valid);
}

// ConfirmationDialog Implementation (static methods)
bool ConfirmationDialog::confirm(wxWindow *parent, const wxString &title, 
                               const wxString &message, const wxString &details)
{
    if (details.IsEmpty()) {
        return wxMessageBox(message, title, wxYES_NO | wxICON_QUESTION, parent) == wxYES;
    } else {
        // For detailed confirmation, we need a custom dialog
        wxDialog dialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
                        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
        
        wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
        
        wxStaticText *messageLabel = new wxStaticText(&dialog, wxID_ANY, message);
        messageLabel->Wrap(350); // Wrap text to 350 pixels
        sizer->Add(messageLabel, 0, wxALL | wxEXPAND, 10);
        
        wxTextCtrl *detailsCtrl = new wxTextCtrl(&dialog, wxID_ANY, details,
                                                 wxDefaultPosition, wxSize(-1, 100),
                                                 wxTE_MULTILINE | wxTE_READONLY);
        sizer->Add(detailsCtrl, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
        
        sizer->Add(new wxStaticLine(&dialog, wxID_ANY), 0, wxEXPAND | wxALL, 5);
        
        wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        wxButton *yesButton = new wxButton(&dialog, wxID_YES);
        wxButton *noButton = new wxButton(&dialog, wxID_NO);
        buttonSizer->Add(yesButton, 0, wxALL, 5);
        buttonSizer->Add(noButton, 0, wxALL, 5);
        
        sizer->Add(buttonSizer, 0, wxALIGN_RIGHT | wxALL, 5);
        
        dialog.SetSizerAndFit(sizer);
        
        return dialog.ShowModal() == wxID_YES;
    }
}

void ConfirmationDialog::showInfo(wxWindow *parent, const wxString &title, const wxString &message)
{
    wxMessageBox(message, title, wxOK | wxICON_INFORMATION, parent);
}

void ConfirmationDialog::showError(wxWindow *parent, const wxString &title, const wxString &message)
{
    wxMessageBox(message, title, wxOK | wxICON_ERROR, parent);
}

void ConfirmationDialog::showWarning(wxWindow *parent, const wxString &title, const wxString &message)
{
    wxMessageBox(message, title, wxOK | wxICON_WARNING, parent);
}

// ProgressDialog Implementation
ProgressDialog::ProgressDialog(const wxString &title, wxWindow *parent)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(400, 300),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      statusLabel(nullptr), progressBar(nullptr), detailsText(nullptr)
{
    setupUi();
}

void ProgressDialog::setupUi()
{
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
    
    statusLabel = new wxStaticText(this, wxID_ANY, "Processing...");
    mainSizer->Add(statusLabel, 0, wxEXPAND | wxALL, 10);
    
    progressBar = new wxGauge(this, wxID_ANY, 100);
    mainSizer->Add(progressBar, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    
    detailsText = new wxTextCtrl(this, wxID_ANY, "",
                                wxDefaultPosition, wxSize(-1, 150),
                                wxTE_MULTILINE | wxTE_READONLY);
    mainSizer->Add(detailsText, 1, wxEXPAND | wxALL, 10);
    
    wxButton *cancelButton = new wxButton(this, wxID_CANCEL);
    mainSizer->Add(cancelButton, 0, wxALIGN_RIGHT | wxALL, 10);
    
    SetSizerAndFit(mainSizer);
}

void ProgressDialog::setProgress(int value)
{
    progressBar->SetValue(value);
}

void ProgressDialog::setMaximum(int maximum)
{
    progressBar->SetRange(maximum);
}

void ProgressDialog::setLabelText(const wxString &text)
{
    statusLabel->SetLabel(text);
}

void ProgressDialog::setDetailText(const wxString &text)
{
    detailsText->AppendText(text + "\n");
    
    // Auto-scroll to bottom
    detailsText->SetInsertionPointEnd();
}

// ProfileImportExportDialog Implementation
wxBEGIN_EVENT_TABLE(ProfileImportExportDialog, BaseDialog)
    EVT_BUTTON(ID_BROWSE_BUTTON, ProfileImportExportDialog::OnBrowseFile)
    EVT_TEXT(ID_FILE_PATH, ProfileImportExportDialog::OnInputChanged)
    EVT_TEXT(ID_PROFILE_NAME_EDIT, ProfileImportExportDialog::OnInputChanged)
wxEND_EVENT_TABLE()

ProfileImportExportDialog::ProfileImportExportDialog(Mode mode, wxWindow *parent)
    : BaseDialog(mode == Import ? "Import Profile" : "Export Profile", parent),
      mode(mode), filePathEdit(nullptr), browseButton(nullptr),
      formatChoice(nullptr), profileNameEdit(nullptr)
{
    setupUi();
}

ProfileImportExportDialog::ProfileImportExportDialog(Mode mode, const wxString &defaultProfileName, wxWindow *parent)
    : BaseDialog(mode == Import ? "Import Profile" : "Export Profile", parent),
      mode(mode), filePathEdit(nullptr), browseButton(nullptr),
      formatChoice(nullptr), profileNameEdit(nullptr)
{
    setupUi();
    setDefaultProfileName(defaultProfileName);
}

void ProfileImportExportDialog::setupUi()
{
    // File selection
    wxBoxSizer *fileLayout = new wxBoxSizer(wxHORIZONTAL);
    filePathEdit = new wxTextCtrl(this, ID_FILE_PATH);
    filePathEdit->SetHint(mode == Import ? "Select file to import" : "Select export location");
    browseButton = new wxButton(this, ID_BROWSE_BUTTON, "Browse...");
    fileLayout->Add(filePathEdit, 1, wxEXPAND | wxRIGHT, 5);
    fileLayout->Add(browseButton, 0);
    
    // Wrap layout in a wxPanel for the form sizer
    wxPanel *fileRow = new wxPanel(this);
    fileRow->SetSizer(fileLayout);
    addFormRow("File:", fileRow);
    
    // Format selection
    formatChoice = new wxChoice(this, wxID_ANY);
    formatChoice->Append("Environment (.env)", new wxString("env"));
    formatChoice->Append("JSON (.json)", new wxString("json"));
    formatChoice->Append("YAML (.yaml)", new wxString("yaml"));
    if (formatChoice->GetCount() > 0) {
        formatChoice->SetSelection(0);
    }
    addFormRow("Format:", formatChoice);
    
    // Profile name (for import or export name)
    profileNameEdit = new wxTextCtrl(this, ID_PROFILE_NAME_EDIT);
    if (mode == Import) {
        profileNameEdit->SetHint("Name for imported profile");
        addFormRow("Profile Name:", profileNameEdit);
    } else {
        profileNameEdit->SetHint("Profile to export");
        addFormRow("Export Profile:", profileNameEdit);
    }
    
    setupButtons(wxID_OK | wxID_CANCEL);
    
    validateInput();
}

void ProfileImportExportDialog::OnBrowseFile(wxCommandEvent &event)
{
    wxString filter;
    wxString formatStr = "env"; // Default format
    
    int sel = formatChoice->GetSelection();
    if (sel != wxNOT_FOUND && formatChoice->GetClientData(sel)) {
        formatStr = *static_cast<wxString*>(formatChoice->GetClientData(sel));
    }
    
    if (formatStr == "env") {
        filter = "Environment Files (*.env)|*.env|All Files (*.*)|*.*";
    } else if (formatStr == "json") {
        filter = "JSON Files (*.json)|*.json|All Files (*.*)|*.*";
    } else if (formatStr == "yaml") {
        filter = "YAML Files (*.yaml;*.yml)|*.yaml;*.yml|All Files (*.*)|*.*";
    } else {
        filter = "All Files (*.*)|*.*";
    }
    
    wxString defaultDir = wxStandardPaths::Get().GetDocumentsDir();
    wxString filePath;
    
    if (mode == Import) {
        wxFileDialog dialog(this, "Select file to import", defaultDir, "", filter, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dialog.ShowModal() == wxID_OK) {
            filePath = dialog.GetPath();
        }
    } else {
        wxFileDialog dialog(this, "Select export location", defaultDir, "", filter, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dialog.ShowModal() == wxID_OK) {
            filePath = dialog.GetPath();
        }
    }
    
    if (!filePath.IsEmpty()) {
        filePathEdit->SetValue(filePath);
    }
}

wxString ProfileImportExportDialog::getFilePath() const
{
    return filePathEdit->GetValue().Trim();
}

wxString ProfileImportExportDialog::getFormat() const
{
    int sel = formatChoice->GetSelection();
    if (sel != wxNOT_FOUND && formatChoice->GetClientData(sel)) {
        return *static_cast<wxString*>(formatChoice->GetClientData(sel));
    }
    return "env"; // Default
}

wxString ProfileImportExportDialog::getProfileName() const
{
    return profileNameEdit->GetValue().Trim();
}

void ProfileImportExportDialog::setDefaultProfileName(const wxString &profileName)
{
    if (profileNameEdit) {
        profileNameEdit->SetValue(profileName);
    }
}

void ProfileImportExportDialog::OnInputChanged(wxCommandEvent &event)
{
    validateInput();
    event.Skip();
}

void ProfileImportExportDialog::validateInput()
{
    bool valid = !filePathEdit->GetValue().Trim().IsEmpty() &&
                 !profileNameEdit->GetValue().Trim().IsEmpty();
    setValid(valid);
}

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI