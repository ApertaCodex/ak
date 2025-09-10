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

// Event handlers for context menu actions
void KeyManagerWidget::OnCopyKeyName(wxCommandEvent& WXUNUSED(event))
{
    int row = grid->GetGridCursorRow();
    if (row >= 0) {
        wxString name = grid->GetCellValue(row, ColName);
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(name));
            wxTheClipboard->Close();
            ShowSuccess("Key name copied to clipboard");
        }
    }
}

void KeyManagerWidget::OnCopyKeyValue(wxCommandEvent& WXUNUSED(event))
{
    int row = grid->GetGridCursorRow();
    if (row >= 0) {
        wxString value = grid->GetCellValue(row, ColValue);
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(value));
            wxTheClipboard->Close();
            ShowSuccess("Key value copied to clipboard");
        }
    }
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

void KeyManagerWidget::testSelectedKey()
{
    int row = table->currentRow();
    if (row < 0) return;
    
    QString keyName = table->item(row, ColumnName)->text();
    QString serviceName = table->item(row, ColumnService)->text();
    
    // Map service display name to service code
    QString serviceCode = getServiceCode(serviceName);
    if (serviceCode.isEmpty()) {
        updateTestStatus(keyName, false, "Service not recognized");
        showError("Cannot test: Service not recognized");
        return;
    }
    
    // Show testing status
    updateTestStatus(keyName, false, "Testing...");
    
    // Disable button during test
    testSelectedButton->setEnabled(false);
    statusLabel->setText("Testing " + serviceName + "...");
    statusLabel->setStyleSheet("color: #0066cc; font-size: 12px;");
    
    // Run test in separate thread to avoid blocking UI
    QTimer::singleShot(100, [this, serviceCode, serviceName, keyName]() {
        try {
            auto result = ak::services::test_one(config, serviceCode.toStdString());
            
            if (result.ok) {
                updateTestStatus(keyName, true, QString("✓ %1ms").arg(result.duration.count()));
                showSuccess(QString("%1 test passed! (%2ms)").arg(serviceName).arg(result.duration.count()));
            } else {
                QString error = result.error_message.empty() ? "Test failed" : QString::fromStdString(result.error_message);
                updateTestStatus(keyName, false, "✗ " + error);
                showError(QString("%1 test failed: %2").arg(serviceName).arg(error));
            }
        } catch (const std::exception& e) {
            updateTestStatus(keyName, false, QString("✗ %1").arg(e.what()));
            showError(QString("%1 test failed: %2").arg(serviceName).arg(e.what()));
        }
        
        testSelectedButton->setEnabled(table->currentRow() >= 0);
    });
}

void KeyManagerWidget::updateTestStatus(const QString &keyName, bool success, const QString &message)
{
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->item(row, ColumnName)->text() == keyName) {
            QTableWidgetItem *statusItem = table->item(row, ColumnTestStatus);
            if (statusItem) {
                statusItem->setText(message);
                if (success) {
                    statusItem->setForeground(QBrush(QColor(0, 170, 0))); // Green
                } else if (message.startsWith("✗")) {
                    statusItem->setForeground(QBrush(QColor(204, 68, 68))); // Red
                } else {
                    statusItem->setForeground(QBrush(QColor(102, 102, 102))); // Gray for testing
                }
            }
            break;
        }
    }
}

void KeyManagerWidget::testAllKeys()
{
    // Get all configured services
    auto configuredServices = ak::services::detectConfiguredServices(config);
    if (configuredServices.empty()) {
        showError("No API keys configured for testing");
        return;
    }
    
    // Reset all test statuses to "Testing..."
    for (int row = 0; row < table->rowCount(); ++row) {
        QString keyName = table->item(row, ColumnName)->text();
        QString serviceName = table->item(row, ColumnService)->text();
        QString serviceCode = getServiceCode(serviceName);
        
        if (!serviceCode.isEmpty() && std::find(configuredServices.begin(), configuredServices.end(), serviceCode.toStdString()) != configuredServices.end()) {
            updateTestStatus(keyName, false, "Testing...");
        }
    }
    
    // Disable buttons during test
    testAllButton->setEnabled(false);
    testSelectedButton->setEnabled(false);
    
    statusLabel->setText(QString("Testing %1 services...").arg(configuredServices.size()));
    statusLabel->setStyleSheet("color: #0066cc; font-size: 12px;");
    
    // Run tests
    QTimer::singleShot(100, [this, configuredServices]() {
        try {
            auto results = ak::services::run_tests_parallel(config, configuredServices, false);
            
            // Update status for each service
            for (const auto& result : results) {
                // Find the key name for this service
                for (int row = 0; row < table->rowCount(); ++row) {
                    QString keyName = table->item(row, ColumnName)->text();
                    QString serviceName = table->item(row, ColumnService)->text();
                    QString serviceCode = getServiceCode(serviceName);
                    
                    if (serviceCode.toStdString() == result.service) {
                        if (result.ok) {
                            updateTestStatus(keyName, true, QString("✓ %1ms").arg(result.duration.count()));
                        } else {
                            QString error = result.error_message.empty() ? "Failed" : QString::fromStdString(result.error_message);
                            updateTestStatus(keyName, false, "✗ " + error);
                        }
                    }
                }
            }
            
            int passed = 0;
            int failed = 0;
            for (const auto& result : results) {
                if (result.ok) passed++;
                else failed++;
            }
            
            if (failed == 0) {
                showSuccess(QString("All %1 API tests passed!").arg(passed));
            } else {
                showError(QString("Tests completed: %1 passed, %2 failed").arg(passed).arg(failed));
            }
        } catch (const std::exception& e) {
            showError(QString("Test failed: %1").arg(e.what()));
        }
        
        testAllButton->setEnabled(true);
        testSelectedButton->setEnabled(table->currentRow() >= 0);
    });
}

QString KeyManagerWidget::getServiceCode(const QString &displayName)
{
    static const QMap<QString, QString> serviceCodes = {
        {"OpenAI", "openai"},
        {"Anthropic", "anthropic"},
        {"Google", "gemini"},
        {"Gemini", "gemini"},
        {"Azure", "azure_openai"},
        {"AWS", "aws"},
        {"GitHub", "github"},
        {"Slack", "slack"},
        {"Discord", "discord"},
        {"Stripe", "stripe"},
        {"Twilio", "twilio"},
        {"SendGrid", "sendgrid"},
        {"Mailgun", "mailgun"},
        {"Cloudflare", "cloudflare"},
        {"Vercel", "vercel"},
        {"Netlify", "netlify"},
        {"Heroku", "heroku"},
        {"Groq", "groq"},
        {"Mistral", "mistral"},
        {"Cohere", "cohere"},
        {"Brave", "brave"},
        {"DeepSeek", "deepseek"},
        {"Exa", "exa"},
        {"Fireworks", "fireworks"},
        {"Hugging Face", "huggingface"},
        {"OpenRouter", "openrouter"},
        {"Perplexity", "perplexity"},
        {"SambaNova", "sambanova"},
        {"Tavily", "tavily"},
        {"Together AI", "together"},
        {"xAI", "xai"}
    };
    
    auto it = serviceCodes.find(displayName);
    return it != serviceCodes.end() ? it.value() : "";
}

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI