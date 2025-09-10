#ifdef BUILD_GUI

#include "gui/widgets/profilemanager.hpp"
#include "gui/widgets/common/dialogs.hpp"
#include "storage/vault.hpp"
#include "core/config.hpp"
#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/statbox.h>
#include <wx/regex.h>
#include <wx/timer.h>
#include <wx/textdlg.h>
#include <fstream>
#include <algorithm>
#include <filesystem>

namespace ak {
namespace gui {
namespace widgets {

// Event table for ProfileManagerWidget
wxBEGIN_EVENT_TABLE(ProfileManagerWidget, wxPanel)
    EVT_BUTTON(ID_CREATE_PROFILE, ProfileManagerWidget::OnCreateProfile)
    EVT_BUTTON(ID_DELETE_PROFILE, ProfileManagerWidget::OnDeleteProfile)
    EVT_BUTTON(ID_RENAME_PROFILE, ProfileManagerWidget::OnRenameProfile)
    EVT_BUTTON(ID_DUPLICATE_PROFILE, ProfileManagerWidget::OnDuplicateProfile)
    EVT_BUTTON(ID_IMPORT_PROFILE, ProfileManagerWidget::OnImportProfile)
    EVT_BUTTON(ID_EXPORT_PROFILE, ProfileManagerWidget::OnExportProfile)
    EVT_BUTTON(ID_REFRESH_PROFILES, ProfileManagerWidget::OnRefresh)
    EVT_LISTBOX(ID_PROFILE_LIST, ProfileManagerWidget::OnProfileSelectionChanged)
    EVT_LISTBOX_DCLICK(ID_PROFILE_LIST, ProfileManagerWidget::OnProfileDoubleClicked)
wxEND_EVENT_TABLE()

// ProfileManagerWidget Implementation
ProfileManagerWidget::ProfileManagerWidget(const core::Config& config, wxWindow *parent)
    : wxPanel(parent, wxID_ANY), config(config), mainSizer(nullptr), controlsSizer(nullptr),
      profileList(nullptr), profileListLabel(nullptr), createButton(nullptr),
      deleteButton(nullptr), renameButton(nullptr), duplicateButton(nullptr),
      importButton(nullptr), exportButton(nullptr), refreshButton(nullptr),
      detailsGroup(nullptr), profileNameLabel(nullptr), keyCountLabel(nullptr),
      keysText(nullptr), statusLabel(nullptr)
{
    SetupUi();
    LoadProfiles();
}

ProfileManagerWidget::~ProfileManagerWidget()
{
    // wxWidgets handles cleanup of child widgets
}

void ProfileManagerWidget::SetupUi()
{
    mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(mainSizer);

    // Create splitter for main layout
    wxSplitterWindow *splitter = new wxSplitterWindow(this, wxID_ANY,
                                                    wxDefaultPosition, wxDefaultSize,
                                                    wxSP_3D | wxSP_LIVE_UPDATE);
    splitter->SetMinimumPaneSize(200); // Prevent panel from disappearing

    // Left panel - profile list and controls
    wxPanel *leftPanel = new wxPanel(splitter, wxID_ANY);
    wxBoxSizer *leftSizer = new wxBoxSizer(wxVERTICAL);
    leftPanel->SetSizer(leftSizer);

    SetupProfileList();
    SetupControls();

    leftSizer->Add(profileListLabel, 0, wxEXPAND | wxALL, 5);
    leftSizer->Add(profileList, 1, wxEXPAND | wxALL, 5);
    leftSizer->Add(controlsSizer, 0, wxEXPAND | wxALL, 5);

    // Right panel - profile details
    wxPanel *rightPanel = new wxPanel(splitter, wxID_ANY);
    wxBoxSizer *rightSizer = new wxBoxSizer(wxVERTICAL);
    rightPanel->SetSizer(rightSizer);

    SetupProfileDetails();
    rightSizer->Add(detailsGroup, 1, wxEXPAND | wxALL, 5);

    // Add panels to splitter
    splitter->SplitVertically(leftPanel, rightPanel);
    splitter->SetSashPosition(300); // Initial position of splitter

    // Add splitter to main layout
    mainSizer->Add(splitter, 1, wxEXPAND | wxALL, 5);

    // Status label
    statusLabel = new wxStaticText(this, wxID_ANY, "Ready");
    statusLabel->SetForegroundColour(wxColour(100, 100, 100));
    wxFont smallFont = statusLabel->GetFont();
    smallFont.SetPointSize(smallFont.GetPointSize() - 1);
    statusLabel->SetFont(smallFont);
    mainSizer->Add(statusLabel, 0, wxALL, 5);
}

void ProfileManagerWidget::SetupProfileList()
{
    profileListLabel = new wxStaticText(this, wxID_ANY, "Available Profiles:");
    wxFont boldFont = profileListLabel->GetFont();
    boldFont.SetWeight(wxFONTWEIGHT_BOLD);
    profileListLabel->SetFont(boldFont);

    profileList = new wxListBox(this, ID_PROFILE_LIST, wxDefaultPosition, wxDefaultSize, 0, NULL,
                               wxLB_SINGLE | wxLB_NEEDED_SB);
}

void ProfileManagerWidget::SetupControls()
{
    controlsSizer = new wxBoxSizer(wxVERTICAL);

    // Create main buttons row
    wxBoxSizer *mainButtonsSizer = new wxBoxSizer(wxHORIZONTAL);

    // Create action buttons row
    wxBoxSizer *actionButtonsSizer = new wxBoxSizer(wxHORIZONTAL);

    // Create buttons
    createButton = new wxButton(this, ID_CREATE_PROFILE, "Create");
    deleteButton = new wxButton(this, ID_DELETE_PROFILE, "Delete");
    renameButton = new wxButton(this, ID_RENAME_PROFILE, "Rename");
    duplicateButton = new wxButton(this, ID_DUPLICATE_PROFILE, "Duplicate");
    importButton = new wxButton(this, ID_IMPORT_PROFILE, "Import");
    exportButton = new wxButton(this, ID_EXPORT_PROFILE, "Export");
    refreshButton = new wxButton(this, ID_REFRESH_PROFILES, "Refresh");

    // Initially disable buttons that require selection
    deleteButton->Disable();
    renameButton->Disable();
    duplicateButton->Disable();
    exportButton->Disable();

    // Add buttons to layout
    mainButtonsSizer->Add(createButton, 1, wxALL, 2);
    mainButtonsSizer->Add(duplicateButton, 1, wxALL, 2);
    mainButtonsSizer->Add(renameButton, 1, wxALL, 2);
    mainButtonsSizer->Add(deleteButton, 1, wxALL, 2);

    actionButtonsSizer->Add(importButton, 1, wxALL, 2);
    actionButtonsSizer->Add(exportButton, 1, wxALL, 2);
    actionButtonsSizer->Add(refreshButton, 1, wxALL, 2);

    // Add button rows to main controls sizer
    controlsSizer->Add(mainButtonsSizer, 0, wxEXPAND);
    controlsSizer->Add(actionButtonsSizer, 0, wxEXPAND);
}

void ProfileManagerWidget::SetupProfileDetails()
{
    wxPanel *detailsPanel = new wxPanel(this, wxID_ANY);
    detailsGroup = new wxStaticBoxSizer(wxVERTICAL, detailsPanel, "Profile Details");

    // Profile name and key count
    profileNameLabel = new wxStaticText(detailsPanel, wxID_ANY, "No profile selected");
    wxFont boldFont = profileNameLabel->GetFont();
    boldFont.SetWeight(wxFONTWEIGHT_BOLD);
    profileNameLabel->SetFont(boldFont);

    keyCountLabel = new wxStaticText(detailsPanel, wxID_ANY, "");

    // Keys in profile
    keysText = new wxTextCtrl(detailsPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                            wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH);

    // Add components to layout
    detailsGroup->Add(profileNameLabel, 0, wxEXPAND | wxALL, 5);
    detailsGroup->Add(keyCountLabel, 0, wxEXPAND | wxALL, 5);
    detailsGroup->Add(new wxStaticText(detailsPanel, wxID_ANY, "Keys in profile:"), 0, wxEXPAND | wxALL, 5);
    detailsGroup->Add(keysText, 1, wxEXPAND | wxALL, 5);
}

void ProfileManagerWidget::LoadProfiles()
{
    try {
        // Clear existing data
        profileList->Clear();
        availableProfiles.clear();
        profileKeyCounts.clear();

        // Get profiles from storage
        auto profiles = storage::listProfiles(config);

        // Sort profiles alphabetically
        std::sort(profiles.begin(), profiles.end());

        // Add profiles to list
        for (const auto& profile : profiles) {
            wxString profileName = wxString::FromUTF8(profile);
            availableProfiles.push_back(profileName);

            // Get key count
            auto keys = storage::readProfile(config, profile);
            int keyCount = static_cast<int>(keys.size());
            profileKeyCounts[profileName] = keyCount;

            // Add to list with formatted display
            wxString displayText = wxString::Format("%s (%d keys)", profileName, keyCount);
            profileList->Append(displayText);
        }

        // Update status
        statusLabel->SetLabel(wxString::Format("Loaded %zu profiles", profiles.size()));

        // Update profile list label
        profileListLabel->SetLabel(wxString::Format("Available Profiles (%zu):", profiles.size()));

        // Update UI state
        OnProfileSelectionChanged(wxCommandEvent());

    } catch (const std::exception& e) {
        ShowError(wxString::Format("Failed to load profiles: %s", e.what()));
    }
}

void ProfileManagerWidget::RefreshProfiles()
{
    wxString currentSelection;
    int selectedIndex = profileList->GetSelection();
    if (selectedIndex != wxNOT_FOUND) {
        currentSelection = availableProfiles[selectedIndex];
    }

    LoadProfiles();

    // Restore selection if possible
    if (!currentSelection.IsEmpty()) {
        for (size_t i = 0; i < availableProfiles.size(); ++i) {
            if (availableProfiles[i] == currentSelection) {
                profileList->SetSelection(i);
                UpdateProfileDetails(currentSelection);
                break;
            }
        }
    }
}

void ProfileManagerWidget::UpdateProfileDetails(const wxString &profileName)
{
    currentProfile = profileName;

    if (profileName.IsEmpty()) {
        profileNameLabel->SetLabel("No profile selected");
        keyCountLabel->SetLabel("");
        keysText->Clear();
        keysText->AppendText("Select a profile to view its keys...");
        return;
    }

    profileNameLabel->SetLabel(wxString::Format("Profile: %s", profileName));

    try {
        auto keys = storage::readProfile(config, profileName.ToStdString());
        keyCountLabel->SetLabel(wxString::Format("%zu keys", keys.size()));

        ShowProfileKeys(profileName);

    } catch (const std::exception& e) {
        keyCountLabel->SetLabel("Error loading keys");
        keysText->Clear();
        keysText->AppendText(wxString::Format("Error: %s", e.what()));
    }
}

void ProfileManagerWidget::ShowProfileKeys(const wxString &profileName)
{
    try {
        keysText->Clear();

        // Get keys in profile
        auto keys = storage::readProfile(config, profileName.ToStdString());

        // Load vault to get values (if available)
        core::KeyStore keyStore;
        bool vaultLoaded = false;

        try {
            keyStore = storage::loadVault(config);
            vaultLoaded = true;
        } catch (...) {
            // Vault loading failed, continue without values
        }

        // Sort keys
        std::sort(keys.begin(), keys.end());

        // Add keys to text control
        if (keys.empty()) {
            keysText->AppendText("This profile contains no keys.");
        } else {
            keysText->AppendText("Keys in this profile:\n\n");
            for (const auto& key : keys) {
                wxString keyName = wxString::FromUTF8(key);
                wxString displayText = keyName;

                if (vaultLoaded) {
                    auto it = keyStore.kv.find(key);
                    if (it != keyStore.kv.end()) {
                        wxString value = wxString::FromUTF8(it->second);

                        // Mask the value (first 4 + last 4 chars visible)
                        wxString maskedValue;
                        if (value.Length() > 8) {
                            maskedValue = value.Left(4) + wxString('*', value.Length() - 8) + value.Right(4);
                        } else {
                            maskedValue = wxString('*', value.Length());
                        }

                        displayText += ": " + maskedValue;
                    }
                }

                keysText->AppendText(displayText + "\n");
            }
        }

    } catch (const std::exception& e) {
        keysText->Clear();
        keysText->AppendText(wxString::Format("Error loading keys: %s", e.what()));
    }
}

bool ProfileManagerWidget::ValidateProfileName(const wxString &name)
{
    if (name.IsEmpty()) {
        ShowError("Profile name cannot be empty");
        return false;
    }

    // Check for valid characters (alphanumeric, underscore, and hyphen)
    wxRegEx regEx("^[A-Za-z0-9_-]+$");
    if (!regEx.Matches(name)) {
        ShowError("Profile name can only contain letters, numbers, hyphens, and underscores");
        return false;
    }

    return true;
}

void ProfileManagerWidget::ShowError(const wxString &message)
{
    statusLabel->SetLabel(wxString::Format("Error: %s", message));
    statusLabel->SetForegroundColour(wxColour(255, 68, 68));

    // Reset after 5 seconds
    wxTimer *timer = new wxTimer(this, wxID_ANY);
    timer->StartOnce(5000);
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
        statusLabel->SetLabel("Ready");
        statusLabel->SetForegroundColour(wxColour(100, 100, 100));
    }, timer->GetId());
}

void ProfileManagerWidget::ShowSuccess(const wxString &message)
{
    statusLabel->SetLabel(message);
    statusLabel->SetForegroundColour(wxColour(0, 170, 0));

    // Reset after 3 seconds
    wxTimer *timer = new wxTimer(this, wxID_ANY);
    timer->StartOnce(3000);
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
        statusLabel->SetLabel("Ready");
        statusLabel->SetForegroundColour(wxColour(100, 100, 100));
    }, timer->GetId());
}

void ProfileManagerWidget::SendStatusEvent(const wxString &message)
{
    // Custom status event - could be used by parent window
    wxCommandEvent event(wxEVT_COMMAND_TEXT_UPDATED, wxID_ANY);
    event.SetString(message);
    wxPostEvent(GetParent(), event);
}

// Event handlers
void ProfileManagerWidget::OnCreateProfile(wxCommandEvent& event)
{
    wxTextEntryDialog dialog(this, "Enter profile name:", "Create Profile", "");
    if (dialog.ShowModal() == wxID_OK) {
        wxString profileName = dialog.GetValue();

        if (!ValidateProfileName(profileName)) {
            return;
        }

        // Check if profile already exists
        for (const auto& existingProfile : availableProfiles) {
            if (existingProfile == profileName) {
                ShowError("A profile with this name already exists!");
                return;
            }
        }

        try {
            // Create empty profile
            std::vector<std::string> emptyKeys;
            storage::writeProfile(config, profileName.ToStdString(), emptyKeys);

            LoadProfiles();
            ShowSuccess(wxString::Format("Profile '%s' created", profileName));

        } catch (const std::exception& e) {
            ShowError(wxString::Format("Failed to create profile: %s", e.what()));
        }
    }
}

void ProfileManagerWidget::OnDeleteProfile(wxCommandEvent& event)
{
    int selectedIndex = profileList->GetSelection();
    if (selectedIndex == wxNOT_FOUND) return;

    wxString profileName = availableProfiles[selectedIndex];

    wxMessageDialog dialog(this,
                          wxString::Format("Are you sure you want to delete the profile '%s'?", profileName),
                          "Delete Profile",
                          wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);

    if (dialog.ShowModal() == wxID_YES) {
        try {
            // Delete profile file
            std::filesystem::path profilePath = storage::profilePath(config, profileName.ToStdString());
            if (std::filesystem::exists(profilePath)) {
                std::filesystem::remove(profilePath);
            }

            LoadProfiles();
            ShowSuccess(wxString::Format("Profile '%s' deleted", profileName));

        } catch (const std::exception& e) {
            ShowError(wxString::Format("Failed to delete profile: %s", e.what()));
        }
    }
}

void ProfileManagerWidget::OnRenameProfile(wxCommandEvent& event)
{
    int selectedIndex = profileList->GetSelection();
    if (selectedIndex == wxNOT_FOUND) return;

    wxString oldName = availableProfiles[selectedIndex];

    wxTextEntryDialog dialog(this, "Enter new profile name:", "Rename Profile", oldName);
    if (dialog.ShowModal() == wxID_OK) {
        wxString newName = dialog.GetValue();

        if (!ValidateProfileName(newName) || newName == oldName) {
            return;
        }

        // Check if new name already exists
        for (const auto& existingProfile : availableProfiles) {
            if (existingProfile == newName) {
                ShowError("A profile with this name already exists!");
                return;
            }
        }

        try {
            // Read old profile
            auto keys = storage::readProfile(config, oldName.ToStdString());

            // Write to new profile
            storage::writeProfile(config, newName.ToStdString(), keys);

            // Delete old profile
            std::filesystem::path oldPath = storage::profilePath(config, oldName.ToStdString());
            if (std::filesystem::exists(oldPath)) {
                std::filesystem::remove(oldPath);
            }

            LoadProfiles();
            ShowSuccess(wxString::Format("Profile renamed from '%s' to '%s'", oldName, newName));

        } catch (const std::exception& e) {
            ShowError(wxString::Format("Failed to rename profile: %s", e.what()));
        }
    }
}

void ProfileManagerWidget::OnDuplicateProfile(wxCommandEvent& event)
{
    int selectedIndex = profileList->GetSelection();
    if (selectedIndex == wxNOT_FOUND) return;

    wxString originalProfileName = availableProfiles[selectedIndex];

    wxTextEntryDialog dialog(this,
                            wxString::Format("Enter name for duplicate of '%s':", originalProfileName),
                            "Duplicate Profile",
                            originalProfileName + "_copy");

    if (dialog.ShowModal() == wxID_OK) {
        wxString newProfileName = dialog.GetValue();

        if (!ValidateProfileName(newProfileName)) {
            return;
        }

        // Check if profile already exists
        for (const auto& existingProfile : availableProfiles) {
            if (existingProfile == newProfileName) {
                ShowError("A profile with this name already exists!");
                return;
            }
        }

        try {
            // Read original profile keys
            auto originalKeys = storage::readProfile(config, originalProfileName.ToStdString());

            // Create new profile with same keys
            storage::writeProfile(config, newProfileName.ToStdString(), originalKeys);

            LoadProfiles();

            // Select the newly created profile
            for (size_t i = 0; i < availableProfiles.size(); ++i) {
                if (availableProfiles[i] == newProfileName) {
                    profileList->SetSelection(i);
                    UpdateProfileDetails(newProfileName);
                    break;
                }
            }

            ShowSuccess(wxString::Format("Profile '%s' duplicated as '%s'", originalProfileName, newProfileName));

        } catch (const std::exception& e) {
            ShowError(wxString::Format("Failed to duplicate profile: %s", e.what()));
        }
    }
}

void ProfileManagerWidget::OnImportProfile(wxCommandEvent& event)
{
    // For import, suggest a new profile name or use current selection if available
    wxString suggestedName = currentProfile.IsEmpty() ? "imported_profile" : currentProfile + "_imported";

    wxTextEntryDialog nameDialog(this, "Enter profile name:", "Import Profile", suggestedName);
    if (nameDialog.ShowModal() != wxID_OK) return;

    wxString profileName = nameDialog.GetValue();
    if (!ValidateProfileName(profileName)) return;

    // Check if profile already exists
    for (const auto& existingProfile : availableProfiles) {
        if (existingProfile == profileName) {
            ShowError("A profile with this name already exists!");
            return;
        }
    }

    wxFileDialog fileDialog(this, "Import Profile", "", "",
                           "All supported files (*.env;*.json;*.yaml)|*.env;*.json;*.yaml|"
                           "Environment files (*.env)|*.env|"
                           "JSON files (*.json)|*.json|"
                           "YAML files (*.yaml)|*.yaml",
                           wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (fileDialog.ShowModal() == wxID_OK) {
        wxString filePath = fileDialog.GetPath();
        wxString format;

        // Determine format from file extension
        if (filePath.EndsWith(".env")) {
            format = "env";
        } else if (filePath.EndsWith(".json")) {
            format = "json";
        } else if (filePath.EndsWith(".yaml") || filePath.EndsWith(".yml")) {
            format = "yaml";
        } else {
            ShowError("Unsupported file format");
            return;
        }

        try {
            std::vector<std::string> keys;

            if (format == "env") {
                // Parse .env file
                std::ifstream input(filePath.ToStdString());
                auto parsedKeys = storage::parse_env_file(input);
                for (const auto& [key, value] : parsedKeys) {
                    keys.push_back(key);
                }
            } else if (format == "json") {
                // Parse JSON file
                std::ifstream input(filePath.ToStdString());
                std::string content((std::istreambuf_iterator<char>(input)),
                                   std::istreambuf_iterator<char>());
                auto parsedKeys = storage::parse_json_min(content);
                for (const auto& [key, value] : parsedKeys) {
                    keys.push_back(key);
                }
            } else if (format == "yaml") {
                // For now, treat YAML as JSON (simplified)
                std::ifstream input(filePath.ToStdString());
                std::string content((std::istreambuf_iterator<char>(input)),
                                   std::istreambuf_iterator<char>());
                auto parsedKeys = storage::parse_json_min(content);
                for (const auto& [key, value] : parsedKeys) {
                    keys.push_back(key);
                }
            }

            // Write profile
            storage::writeProfile(config, profileName.ToStdString(), keys);

            LoadProfiles();
            ShowSuccess(wxString::Format("Profile '%s' imported with %zu keys", profileName, keys.size()));

        } catch (const std::exception& e) {
            ShowError(wxString::Format("Failed to import profile: %s", e.what()));
        }
    }
}

void ProfileManagerWidget::OnExportProfile(wxCommandEvent& event)
{
    int selectedIndex = profileList->GetSelection();
    if (selectedIndex == wxNOT_FOUND) return;

    wxString profileName = availableProfiles[selectedIndex];

    wxTextEntryDialog nameDialog(this, "Enter export profile name:", "Export Profile", profileName);
    if (nameDialog.ShowModal() != wxID_OK) return;

    wxString selectedProfileForExport = nameDialog.GetValue();

    wxFileDialog fileDialog(this, "Export Profile", "", "",
                           "Environment files (*.env)|*.env|"
                           "JSON files (*.json)|*.json",
                           wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (fileDialog.ShowModal() == wxID_OK) {
        wxString filePath = fileDialog.GetPath();
        wxString format;

        // Determine format from file extension
        if (filePath.EndsWith(".env")) {
            format = "env";
        } else if (filePath.EndsWith(".json")) {
            format = "json";
        } else {
            ShowError("Unsupported file format");
            return;
        }

        try {
            // Read profile keys (use the name from dialog in case user changed it)
            auto keys = storage::readProfile(config, selectedProfileForExport.ToStdString());

            // Load vault to get actual key values
            auto vault = storage::loadVault(config);

            if (format == "env") {
                // Export as .env file with values
                std::ofstream output(filePath.ToStdString());
                for (const auto& key : keys) {
                    auto it = vault.kv.find(key);
                    if (it != vault.kv.end()) {
                        // Escape quotes and backslashes in values
                        std::string value = it->second;
                        std::string escaped;
                        for (char c : value) {
                            if (c == '"' || c == '\\') {
                                escaped += '\\';
                            }
                            escaped += c;
                        }
                        output << key << "=\"" << escaped << "\"" << std::endl;
                    } else {
                        output << key << "=" << std::endl;
                    }
                }
            } else if (format == "json") {
                // Export as JSON with values
                wxString jsonContent = "{\n";
                jsonContent += wxString::Format("  \"profile\": \"%s\",\n", selectedProfileForExport);
                jsonContent += "  \"keys\": {\n";

                for (size_t i = 0; i < keys.size(); ++i) {
                    const auto& key = keys[i];
                    auto it = vault.kv.find(key);
                    wxString keyStr = wxString::FromUTF8(key);
                    wxString valueStr = "\"\"";

                    if (it != vault.kv.end()) {
                        // Escape quotes and backslashes in JSON
                        std::string value = it->second;
                        std::string escaped;
                        for (char c : value) {
                            if (c == '"' || c == '\\') {
                                escaped += '\\';
                            }
                            escaped += c;
                        }
                        valueStr = wxString::Format("\"%s\"", wxString::FromUTF8(escaped));
                    }

                    jsonContent += wxString::Format("    \"%s\": %s", keyStr, valueStr);
                    if (i < keys.size() - 1) {
                        jsonContent += ",";
                    }
                    jsonContent += "\n";
                }

                jsonContent += "  }\n}";

                std::ofstream output(filePath.ToStdString());
                output << jsonContent.ToStdString();
            }

            ShowSuccess(wxString::Format("Profile '%s' exported to %s", selectedProfileForExport, filePath));

        } catch (const std::exception& e) {
            ShowError(wxString::Format("Failed to export profile: %s", e.what()));
        }
    }
}

void ProfileManagerWidget::OnProfileSelectionChanged(wxCommandEvent& event)
{
    int selectedIndex = profileList->GetSelection();
    bool hasSelection = selectedIndex != wxNOT_FOUND;

    deleteButton->Enable(hasSelection);
    renameButton->Enable(hasSelection);
    duplicateButton->Enable(hasSelection);
    exportButton->Enable(hasSelection);

    if (hasSelection) {
        wxString profileName = availableProfiles[selectedIndex];
        UpdateProfileDetails(profileName);
    } else {
        UpdateProfileDetails("");
    }
}

void ProfileManagerWidget::OnProfileDoubleClicked(wxCommandEvent& event)
{
    int selectedIndex = profileList->GetSelection();
    if (selectedIndex != wxNOT_FOUND) {
        wxString profileName = availableProfiles[selectedIndex];
        UpdateProfileDetails(profileName);
    }
}

void ProfileManagerWidget::OnRefresh(wxCommandEvent& event)
{
    RefreshProfiles();
    ShowSuccess("Profiles refreshed");
}

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI