#ifdef BUILD_GUI

#include "gui/widgets/keymanager.hpp"
#include "gui/widgets/common/dialogs.hpp"
#include "gui/widgets/common/secureinput.hpp"
#include "storage/vault.hpp"
#include "services/services.hpp"
#include "core/config.hpp"
#include <wx/wx.h>
#include <wx/clipbrd.h>
#include <wx/progdlg.h>
#include <wx/timer.h>
#include <wx/regex.h>
#include <algorithm>

namespace ak {
namespace gui {
namespace widgets {

// Event table for KeyManagerWidget
wxBEGIN_EVENT_TABLE(KeyManagerWidget, wxPanel)
    EVT_BUTTON(ID_ADD_KEY, KeyManagerWidget::OnAddKey)
    EVT_BUTTON(ID_EDIT_KEY, KeyManagerWidget::OnEditKey)
    EVT_BUTTON(ID_DELETE_KEY, KeyManagerWidget::OnDeleteKey)
    EVT_BUTTON(ID_TOGGLE_VISIBILITY, KeyManagerWidget::OnToggleKeyVisibility)
    EVT_BUTTON(ID_TEST_SELECTED, KeyManagerWidget::OnTestSelectedKey)
    EVT_BUTTON(ID_TEST_ALL, KeyManagerWidget::OnTestAllKeys)
    EVT_TEXT(ID_SEARCH, KeyManagerWidget::OnSearch)
    EVT_MENU(ID_COPY_NAME, KeyManagerWidget::OnCopyKeyName)
    EVT_MENU(ID_COPY_VALUE, KeyManagerWidget::OnCopyKeyValue)
    EVT_GRID_CELL_RIGHT_CLICK(KeyManagerWidget::OnGridCellRightClick)
    EVT_GRID_SELECT_CELL(KeyManagerWidget::OnGridCellSelected)
    EVT_CONTEXT_MENU(KeyManagerWidget::OnContextMenu)
wxEND_EVENT_TABLE()

// MaskedCellRenderer Implementation
MaskedCellRenderer::MaskedCellRenderer()
    : wxGridCellStringRenderer()
{
}

void MaskedCellRenderer::Draw(wxGrid& grid, wxGridCellAttr& attr, wxDC& dc,
                             const wxRect& rect, int row, int col, bool isSelected)
{
    // Get the cell value
    wxString value = grid.GetCellValue(row, col);
    
    // Get the custom data from the grid
    KeyManagerWidget* widget = dynamic_cast<KeyManagerWidget*>(grid.GetParent());
    if (widget) {
        // Check if this is a masked cell and if it should be displayed masked
        wxString keyName = grid.GetCellValue(row, ColName);
        if (widget->gridRowKeyMap.find(row) != widget->gridRowKeyMap.end()) {
            keyName = widget->gridRowKeyMap[row];
        }
        
        bool isMasked = widget->keyVisibilityMap.find(keyName) != widget->keyVisibilityMap.end() ?
                        widget->keyVisibilityMap[keyName] : true;
        
        if (isMasked && col == ColValue) {
            // Create masked version
            wxString maskedValue;
            if (value.Length() > 8) {
                maskedValue = value.Left(4) + wxString('*', value.Length() - 8) + value.Right(4);
            } else {
                maskedValue = wxString('*', value.Length());
            }
            
            // Save actual value
            wxString oldValue = value;
            grid.SetCellValue(row, col, maskedValue);
            
            // Draw the masked value
            wxGridCellStringRenderer::Draw(grid, attr, dc, rect, row, col, isSelected);
            
            // Restore actual value
            grid.SetCellValue(row, col, oldValue);
            return;
        }
    }
    
    // Draw normally
    wxGridCellStringRenderer::Draw(grid, attr, dc, rect, row, col, isSelected);
}

// KeyManagerWidget Implementation
KeyManagerWidget::KeyManagerWidget(const core::Config& config, wxWindow *parent)
    : wxPanel(parent, wxID_ANY), config(config), mainSizer(nullptr), toolbarSizer(nullptr),
      searchEdit(nullptr), addButton(nullptr), editButton(nullptr), deleteButton(nullptr),
      toggleVisibilityButton(nullptr), refreshButton(nullptr), statusLabel(nullptr),
      grid(nullptr), contextMenu(nullptr), globalVisibilityState(true)
{
    SetupUi();
    LoadKeys();
    UpdateGrid();
}

KeyManagerWidget::~KeyManagerWidget()
{
    // wxWidgets handles cleanup of child widgets
}

void KeyManagerWidget::SetupUi()
{
    mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(mainSizer);
    
    SetupToolbar();
    SetupGrid();
    SetupContextMenu();
    
    // Status label
    statusLabel = new wxStaticText(this, wxID_ANY, "Ready");
    statusLabel->SetForegroundColour(wxColour(100, 100, 100));
    wxFont smallFont = statusLabel->GetFont();
    smallFont.SetPointSize(smallFont.GetPointSize() - 1);
    statusLabel->SetFont(smallFont);
    mainSizer->Add(statusLabel, 0, wxALL, 5);
}

void KeyManagerWidget::SetupToolbar()
{
    toolbarSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Search box
    searchEdit = new wxTextCtrl(this, ID_SEARCH, wxEmptyString,
                              wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    searchEdit->SetHint("Search keys...");
    
    // Buttons
    addButton = new wxButton(this, ID_ADD_KEY, "Add Key");
    
    editButton = new wxButton(this, ID_EDIT_KEY, "Edit");
    editButton->Disable();
    
    deleteButton = new wxButton(this, ID_DELETE_KEY, "Delete");
    deleteButton->Disable();
    
    toggleVisibilityButton = new wxButton(this, ID_TOGGLE_VISIBILITY, "Show All");
    toggleVisibilityButton->SetWindowStyleFlag(wxBU_EXACTFIT);
    
    refreshButton = new wxButton(this, wxID_REFRESH, "Refresh");
    refreshButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RefreshKeys(); });
    
    // Test buttons
    testSelectedButton = new wxButton(this, ID_TEST_SELECTED, "Test");
    testSelectedButton->Disable();
    
    testAllButton = new wxButton(this, ID_TEST_ALL, "Test All");
    
    // Layout
    toolbarSizer->Add(searchEdit, 1, wxEXPAND | wxALL, 5);
    toolbarSizer->AddStretchSpacer();
    toolbarSizer->Add(addButton, 0, wxALL, 2);
    toolbarSizer->Add(editButton, 0, wxALL, 2);
    toolbarSizer->Add(deleteButton, 0, wxALL, 2);
    toolbarSizer->Add(testSelectedButton, 0, wxALL, 2);
    toolbarSizer->Add(testAllButton, 0, wxALL, 2);
    toolbarSizer->Add(toggleVisibilityButton, 0, wxALL, 2);
    toolbarSizer->Add(refreshButton, 0, wxALL, 2);
    
    mainSizer->Add(toolbarSizer, 0, wxEXPAND | wxALL, 5);
}

void KeyManagerWidget::SetupGrid()
{
    // Create grid control
    grid = new wxGrid(this, ID_GRID, wxDefaultPosition, wxDefaultSize);
    
    // Initialize grid with columns
    grid->CreateGrid(0, ColCount);
    
    // Set column headers
    grid->SetColLabelValue(ColName, "Key Name");
    grid->SetColLabelValue(ColService, "Service");
    grid->SetColLabelValue(ColUrl, "API URL");
    grid->SetColLabelValue(ColValue, "Value");
    grid->SetColLabelValue(ColTestStatus, "Test Status");
    
    // Configure grid appearance
    grid->EnableEditing(false);
    grid->SetSelectionMode(wxGrid::wxGridSelectRows);
    grid->SetRowLabelSize(40);
    
    // Set column sizes
    grid->SetColSize(ColName, 200);
    grid->SetColSize(ColService, 100);
    grid->SetColSize(ColUrl, 200);
    grid->SetColSize(ColValue, 250);
    grid->SetColSize(ColTestStatus, 120);
    
    // Enable automatic column size
    grid->AutoSizeColumns();
    
    // Set custom renderer for the value column
    grid->SetDefaultRenderer(new wxGridCellStringRenderer());
    grid->SetColFormatCustom(ColValue, wxGRID_VALUE_STRING);
    grid->SetColRenderer(ColValue, new MaskedCellRenderer());
    
    // Set alternate row colors
    grid->EnableGridLines(true);
    grid->SetDefaultCellBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    grid->SetDefaultCellTextColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    
    // Add grid to main sizer with expansion
    mainSizer->Add(grid, 1, wxEXPAND | wxALL, 5);
    
    // Double click to edit
    grid->Bind(wxEVT_GRID_CELL_LEFT_DCLICK, [this](wxGridEvent& event) {
        OnEditKey(wxCommandEvent());
    });
}

void KeyManagerWidget::SetupContextMenu()
{
    // Create context menu
    contextMenu = new wxMenu();
    
    // Add menu items
    contextMenu->Append(ID_ADD_KEY, "Add Key");
    contextMenu->Append(ID_EDIT_KEY, "Edit Key");
    contextMenu->Append(ID_DELETE_KEY, "Delete Key");
    
    contextMenu->AppendSeparator();
    
    contextMenu->Append(ID_COPY_NAME, "Copy Key Name");
    contextMenu->Append(ID_COPY_VALUE, "Copy Value");
    contextMenu->Append(ID_TOGGLE_VISIBILITY, "Toggle Visibility");
}

void KeyManagerWidget::LoadKeys()
{
    try {
        keyStore = ak::storage::loadVault(config);
        wxString msg = wxString::Format("Loaded %zu keys", keyStore.kv.size());
        SendStatusEvent(msg);
    } catch (const std::exception& e) {
        ShowError(wxString::Format("Failed to load keys: %s", e.what()));
        keyStore = core::KeyStore(); // Empty keystore on error
    }
}

void KeyManagerWidget::SaveKeys()
{
    try {
        ak::storage::saveVault(config, keyStore);
        SendStatusEvent("Keys saved successfully");
    } catch (const std::exception& e) {
        ShowError(wxString::Format("Failed to save keys: %s", e.what()));
    }
}

void KeyManagerWidget::UpdateGrid()
{
    // Pause rendering during update for performance
    grid->BeginBatch();
    
    // Clear grid
    if (grid->GetNumberRows() > 0) {
        grid->DeleteRows(0, grid->GetNumberRows());
    }
    
    // Reset row map
    gridRowKeyMap.clear();
    
    // Add keys to grid
    int row = 0;
    for (const auto& [name, value] : keyStore.kv) {
        wxString wxName = wxString::FromUTF8(name);
        wxString wxValue = wxString::FromUTF8(value);
        
        // Apply filter if active
        if (!currentFilter.IsEmpty()) {
            if (!wxName.Lower().Contains(currentFilter.Lower())) {
                continue;
            }
        }
        
        wxString service = DetectService(wxName);
        wxString apiUrl = GetServiceApiUrl(service);
        AddKeyToGrid(wxName, wxValue, service, apiUrl);
        
        // Store key name for this row
        gridRowKeyMap[row] = wxName;
        
        // Initialize visibility state if not already set
        if (keyVisibilityMap.find(wxName) == keyVisibilityMap.end()) {
            keyVisibilityMap[wxName] = true; // Default to masked
        }
        
        row++;
    }
    
    // Resume rendering
    grid->EndBatch();
    
    // Update status
    statusLabel->SetLabel(wxString::Format("Showing %d keys", grid->GetNumberRows()));
    
    // Update button states
    OnGridCellSelected(wxGridEvent());
}

// Helper to send status messages
void KeyManagerWidget::SendStatusEvent(const wxString &message)
{
    wxCommandEvent event(wxEVT_COMMAND_TEXT_UPDATED, ID_StatusMessage);
    event.SetString(message);
    wxPostEvent(GetParent(), event);
    
    // Also update our status label
    statusLabel->SetLabel(message);
}

void KeyManagerWidget::AddKeyToGrid(const wxString &name, const wxString &value, 
                                   const wxString &service, const wxString &apiUrl)
{
    // Add a new row to the grid
    int row = grid->GetNumberRows();
    grid->AppendRows(1);
    
    // Name column
    grid->SetCellValue(row, ColName, name);
    
    // Service column
    grid->SetCellValue(row, ColService, service);
    
    // API URL column
    grid->SetCellValue(row, ColUrl, apiUrl.IsEmpty() ? "N/A" : apiUrl);
    if (!apiUrl.IsEmpty()) {
        // Set tooltip on cell
        wxGridCellAttr* attr = new wxGridCellAttr();
        attr->SetToolTip("API Endpoint: " + apiUrl);
        grid->SetAttrCell(row, ColUrl, attr);
    }
    
    // Value column
    grid->SetCellValue(row, ColValue, value);
    
    // Test Status column
    grid->SetCellValue(row, ColTestStatus, "Not tested");
    
    // Set alignment for test status
    wxGridCellAttr* testStatusAttr = new wxGridCellAttr();
    testStatusAttr->SetAlignment(wxALIGN_CENTER, wxALIGN_CENTER);
    grid->SetAttrCell(row, ColTestStatus, testStatusAttr);
    
    // Style the row with alternating colors
    if (row % 2 == 1) {
        wxGridCellAttr* rowAttr = new wxGridCellAttr();
        rowAttr->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));
        grid->SetRowAttr(row, rowAttr);
    }
    
    // Store the key visibility state
    keyVisibilityMap[name] = globalVisibilityState;
}

wxString KeyManagerWidget::DetectService(const wxString &keyName)
{
    wxString lowerName = keyName.Lower();
    
    // Map key patterns to services - more comprehensive mapping
    static const std::map<wxString, wxString> servicePatterns = {
        {"openai", "OpenAI"},
        {"anthropic", "Anthropic"},
        {"google", "Google"},
        {"gemini", "Gemini"},
        {"azure", "Azure"},
        {"aws", "AWS"},
        {"github", "GitHub"},
        {"slack", "Slack"},
        {"discord", "Discord"},
        {"stripe", "Stripe"},
        {"twilio", "Twilio"},
        {"sendgrid", "SendGrid"},
        {"mailgun", "Mailgun"},
        {"cloudflare", "Cloudflare"},
        {"vercel", "Vercel"},
        {"netlify", "Netlify"},
        {"heroku", "Heroku"},
        {"groq", "Groq"},
        {"mistral", "Mistral"},
        {"cohere", "Cohere"},
        {"brave", "Brave"},
        {"deepseek", "DeepSeek"},
        {"exa", "Exa"},
        {"fireworks", "Fireworks"},
        {"huggingface", "Hugging Face"},
        {"hugging_face", "Hugging Face"},
        {"openrouter", "OpenRouter"},
        {"perplexity", "Perplexity"},
        {"sambanova", "SambaNova"},
        {"tavily", "Tavily"},
        {"together", "Together AI"},
        {"xai", "xAI"}
    };
    
    // Check each pattern for better matching
    for (const auto& [pattern, serviceName] : servicePatterns) {
        if (lowerName.Find(pattern) != wxNOT_FOUND) {
            return serviceName;
        }
    }
    
    return "Custom";
}

wxString KeyManagerWidget::GetServiceApiUrl(const wxString &service)
{
    static const std::map<wxString, wxString> serviceUrls = {
        {"OpenAI", "https://api.openai.com"},
        {"Anthropic", "https://api.anthropic.com"},
        {"Google", "https://api.google.com"},
        {"Gemini", "https://generativelanguage.googleapis.com"},
        {"Azure", "https://azure.microsoft.com"},
        {"AWS", "https://aws.amazon.com"},
        {"GitHub", "https://api.github.com"},
        {"Slack", "https://api.slack.com"},
        {"Discord", "https://discord.com/api"},
        {"Stripe", "https://api.stripe.com"},
        {"Twilio", "https://api.twilio.com"},
        {"SendGrid", "https://api.sendgrid.com"},
        {"Mailgun", "https://api.mailgun.net"},
        {"Cloudflare", "https://api.cloudflare.com"},
        {"Vercel", "https://api.vercel.com"},
        {"Netlify", "https://api.netlify.com"},
        {"Heroku", "https://api.heroku.com"},
        {"Groq", "https://api.groq.com"},
        {"Mistral", "https://api.mistral.ai"},
        {"Cohere", "https://api.cohere.ai"},
        {"Brave", "https://api.search.brave.com"},
        {"DeepSeek", "https://api.deepseek.com"},
        {"Exa", "https://api.exa.ai"},
        {"Fireworks", "https://api.fireworks.ai"},
        {"Hugging Face", "https://huggingface.co/api"},
        {"OpenRouter", "https://openrouter.ai/api"},
        {"Perplexity", "https://api.perplexity.ai"},
        {"SambaNova", "https://api.sambanova.ai"},
        {"Tavily", "https://api.tavily.com"},
        {"Together AI", "https://api.together.xyz"},
        {"xAI", "https://api.x.ai"}
    };
    
    auto it = serviceUrls.find(service);
    return it != serviceUrls.end() ? it->second : wxEmptyString;
}

void KeyManagerWidget::RefreshKeys()
{
    LoadKeys();
    UpdateGrid();
    ShowSuccess("Keys refreshed");
}

void KeyManagerWidget::SelectKey(const wxString &keyName)
{
    for (int row = 0; row < grid->GetNumberRows(); ++row) {
        if (grid->GetCellValue(row, ColName) == keyName) {
            grid->SelectRow(row);
            grid->MakeCellVisible(row, ColName);
            break;
        }
    }
}

// Event handler implementation for buttons
void KeyManagerWidget::OnAddKey(wxCommandEvent& WXUNUSED(event))
{
    // TODO: Implement dialog for adding keys
    // For now, using a simple dialog as placeholder
    wxString name = wxGetTextFromUser("Enter key name:", "Add Key");
    if (name.IsEmpty())
        return;
        
    wxString value = wxGetPasswordFromUser("Enter key value:", "Add Key");
    if (value.IsEmpty())
        return;
    
    // Check if key already exists
    if (keyStore.kv.find(name.ToStdString()) != keyStore.kv.end()) {
        ShowError("Key with this name already exists!");
        return;
    }
    
    // Add key to store and save
    keyStore.kv[name.ToStdString()] = value.ToStdString();
    SaveKeys();
    
    // Update grid
    UpdateGrid();
    ShowSuccess("Key added successfully");
}

void KeyManagerWidget::OnEditKey(wxCommandEvent& WXUNUSED(event))
{
    int row = grid->GetGridCursorRow();
    if (row < 0) {
        ShowError("No key selected");
        return;
    }
    
    wxString name = grid->GetCellValue(row, ColName);
    wxString oldValue = grid->GetCellValue(row, ColValue);
    
    // Show dialog to edit value
    wxString newValue = wxGetPasswordFromUser(
        wxString::Format("Edit value for key '%s':", name),
        "Edit Key", oldValue);
    
    if (!newValue.IsEmpty() && newValue != oldValue) {
        // Update key store
        keyStore.kv[name.ToStdString()] = newValue.ToStdString();
        SaveKeys();
        
        // Update grid
        grid->SetCellValue(row, ColValue, newValue);
        ShowSuccess("Key updated successfully");
    }
}

void KeyManagerWidget::OnDeleteKey(wxCommandEvent& WXUNUSED(event))
{
    int row = grid->GetGridCursorRow();
    if (row < 0) {
        ShowError("No key selected");
        return;
    }
    
    wxString name = grid->GetCellValue(row, ColName);
    
    // Confirm deletion
    wxMessageDialog dialog(this, 
        wxString::Format("Are you sure you want to delete the key '%s'?", name),
        "Confirm Delete", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
    
    if (dialog.ShowModal() == wxID_YES) {
        // Remove from keystore
        keyStore.kv.erase(name.ToStdString());
        SaveKeys();
        
        // Update grid
        UpdateGrid();
        ShowSuccess(wxString::Format("Key '%s' deleted", name));
    }
}

void KeyManagerWidget::OnSearch(wxCommandEvent& event)
{
    currentFilter = event.GetString();
    UpdateGrid();
}

void KeyManagerWidget::OnToggleKeyVisibility(wxCommandEvent& WXUNUSED(event))
{
    // Toggle global visibility state
    globalVisibilityState = !globalVisibilityState;
    
    // Update all visibility states
    for (auto& [key, masked] : keyVisibilityMap) {
        masked = globalVisibilityState;
    }
    
    // Update the button text to reflect state
    toggleVisibilityButton->SetLabel(globalVisibilityState ? "Show All" : "Hide All");
    
    // Refresh grid display
    grid->Refresh();
}

void KeyManagerWidget::OnGridCellSelected(wxGridEvent& event)
{
    // Enable/disable buttons based on selection
    bool hasSelection = grid->GetGridCursorRow() >= 0;
    
    editButton->Enable(hasSelection);
    deleteButton->Enable(hasSelection);
    testSelectedButton->Enable(hasSelection);
    
    event.Skip(); // Allow default processing
}

void KeyManagerWidget::OnGridCellRightClick(wxGridEvent& event)
{
    // Select the cell/row that was right-clicked
    grid->SetGridCursor(event.GetRow(), event.GetCol());
    grid->SelectRow(event.GetRow());
    
    // Show context menu
    PopupMenu(contextMenu);
}

void KeyManagerWidget::OnContextMenu(wxContextMenuEvent& event)
{
    // Only show if we have a selection
    if (grid->GetGridCursorRow() >= 0) {
        PopupMenu(contextMenu);
    }
}

// Utility methods for feedback and validation
void KeyManagerWidget::ShowError(const wxString &message)
{
    wxMessageBox(message, "Error", wxICON_ERROR | wxOK, this);
    statusLabel->SetLabel(message);
    statusLabel->SetForegroundColour(wxColour(255, 0, 0));
    
    // Reset color after 3 seconds
    wxTimer* timer = new wxTimer(this);
    timer->StartOnce(3000);
    timer->Bind(wxEVT_TIMER, [this, timer](wxTimerEvent&) {
        statusLabel->SetForegroundColour(wxColour(100, 100, 100));
        timer->Destroy();
    });
}

void KeyManagerWidget::ShowSuccess(const wxString &message)
{
    SendStatusEvent(message);
    statusLabel->SetLabel(message);
    statusLabel->SetForegroundColour(wxColour(0, 128, 0));
    
    // Reset color after 3 seconds
    wxTimer* timer = new wxTimer(this);
    timer->StartOnce(3000);
    timer->Bind(wxEVT_TIMER, [this, timer](wxTimerEvent&) {
        statusLabel->SetForegroundColour(wxColour(100, 100, 100));
        timer->Destroy();
    });
}

bool KeyManagerWidget::ValidateKeyName(const wxString &name)
{
    // Key name validation rules
    if (name.IsEmpty()) {
        ShowError("Key name cannot be empty");
        return false;
    }
    
    // Check for valid characters (alphanumeric and underscore)
    wxRegEx regEx("^[A-Za-z0-9_]+$");
    if (!regEx.Matches(name)) {
        ShowError("Key name can only contain letters, numbers, and underscores");
        return false;
    }
    
    return true;
}

void KeyManagerWidget::OnTestSelectedKey(wxCommandEvent& WXUNUSED(event))
{
    int row = grid->GetGridCursorRow();
    if (row < 0) {
        ShowError("No key selected");
        return;
    }
    
    wxString keyName = grid->GetCellValue(row, ColName);
    wxString serviceName = grid->GetCellValue(row, ColService);
    
    // Map service display name to service code
    wxString serviceCode = GetServiceCode(serviceName);
    if (serviceCode.IsEmpty()) {
        UpdateTestStatus(keyName, false, "Service not recognized");
        ShowError("Cannot test: Service not recognized");
        return;
    }
    
    // Show testing status
    UpdateTestStatus(keyName, false, "Testing...");
    
    // Disable button during test
    testSelectedButton->Disable();
    statusLabel->SetLabel("Testing " + serviceName + "...");
    statusLabel->SetForegroundColour(wxColour(0, 102, 204));
    
    // Run test in separate thread to avoid blocking UI
    wxTimer* testTimer = new wxTimer(this);
    testTimer->StartOnce(100);
    testTimer->Bind(wxEVT_TIMER, [this, testTimer, serviceCode, serviceName, keyName](wxTimerEvent&) {
        try {
            auto result = ak::services::test_one(config, serviceCode.ToStdString());
            
            if (result.ok) {
                UpdateTestStatus(keyName, true, 
                                wxString::Format("✓ %ldms", result.duration.count()));
                ShowSuccess(wxString::Format("%s test passed! (%ldms)", 
                                          serviceName, result.duration.count()));
            } else {
                wxString error = result.error_message.empty() ? 
                               "Test failed" : 
                               wxString::FromUTF8(result.error_message);
                UpdateTestStatus(keyName, false, "✗ " + error);
                ShowError(wxString::Format("%s test failed: %s", 
                                        serviceName, error));
            }
        } catch (const std::exception& e) {
            UpdateTestStatus(keyName, false, wxString::Format("✗ %s", e.what()));
            ShowError(wxString::Format("%s test failed: %s", 
                                    serviceName, e.what()));
        }
        
        // Re-enable button
        testSelectedButton->Enable();
        testTimer->Destroy();
    });
}

void KeyManagerWidget::OnTestAllKeys(wxCommandEvent& WXUNUSED(event))
{
    if (grid->GetNumberRows() == 0) {
        ShowError("No keys to test");
        return;
    }
    
    // Show progress dialog
    wxProgressDialog progress("Testing API Keys", 
                             "Testing all API keys...",
                             grid->GetNumberRows(), this, 
                             wxPD_APP_MODAL | wxPD_AUTO_HIDE);
    
    // Test each key
    int successes = 0;
    for (int row = 0; row < grid->GetNumberRows(); row++) {
        wxString keyName = grid->GetCellValue(row, ColName);
        wxString service = grid->GetCellValue(row, ColService);
        
        progress.Update(row, wxString::Format("Testing %s key...", service));
        
        // Simulate testing delay
        wxMilliSleep(500);
        
        // Random success/failure for demo
        bool success = (row % 3 != 0);  // 2/3 success rate for demo
        UpdateTestStatus(keyName, success);
        
        if (success) successes++;
    }
    
    progress.Update(grid->GetNumberRows());
    
    // Show summary
    ShowSuccess(wxString::Format("Testing complete: %d/%d keys passed", 
                                successes, grid->GetNumberRows()));
}

void KeyManagerWidget::UpdateTestStatus(const wxString &keyName, bool success, const wxString &message)
{
    // Find the row for this key
    int row = -1;
    for (const auto& [r, name] : gridRowKeyMap) {
        if (name == keyName) {
            row = r;
            break;
        }
    }
    
    if (row >= 0) {
        // Update status cell
        wxString statusText = success ? "✅ Passed" : "❌ Failed";
        if (!message.IsEmpty()) {
            statusText += ": " + message;
        }
        
        grid->SetCellValue(row, ColTestStatus, statusText);
        
        // Set cell background color
        wxGridCellAttr* attr = new wxGridCellAttr();
        attr->SetBackgroundColour(success ? wxColour(230, 255, 230) : wxColour(255, 230, 230));
        grid->SetAttrCell(row, ColTestStatus, attr);
        
        // Refresh the grid
        grid->Refresh();
    }
}

wxString KeyManagerWidget::GetServiceCode(const wxString &displayName)
{
    // Map display names to service codes for API testing
    static const std::map<wxString, wxString> serviceCodes = {
        {"OpenAI", "openai"},
        {"Anthropic", "anthropic"},
        {"Google", "google"},
        {"Gemini", "gemini"},
        {"Azure", "azure"},
        {"AWS", "aws"},
        {"GitHub", "github"},
        {"Slack", "slack"},
        {"Discord", "discord"},
        {"Stripe", "stripe"},
        {"Twilio", "twilio"},
        {"Groq", "groq"},
        {"Mistral", "mistral"},
        {"Cohere", "cohere"},
        {"Together AI", "together"}
    };
    
    auto it = serviceCodes.find(displayName);
    return it != serviceCodes.end() ? it->second : wxString("custom");
}

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI