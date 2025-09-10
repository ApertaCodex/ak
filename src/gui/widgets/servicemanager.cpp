#ifdef BUILD_GUI

#include "gui/widgets/servicemanager.hpp"
#include "storage/vault.hpp"
#include "services/services.hpp"
#include "core/config.hpp"
#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/menu.h>
#include <wx/dialog.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/regex.h>
#include <wx/timer.h>
#include <algorithm>
#include <fstream>

namespace ak {
namespace gui {
namespace widgets {

// Event table for ServiceEditorDialog
wxBEGIN_EVENT_TABLE(ServiceEditorDialog, wxDialog)
    EVT_CHECKBOX(wxID_ANY, ServiceEditorDialog::OnTestableChanged)
    EVT_TEXT(wxID_ANY, ServiceEditorDialog::OnNameChanged)
    EVT_TEXT(wxID_ANY, ServiceEditorDialog::OnKeyNameChanged)
    EVT_BUTTON(wxID_OK, ServiceEditorDialog::OnOk)
    EVT_BUTTON(wxID_CANCEL, ServiceEditorDialog::OnCancel)
wxEND_EVENT_TABLE()

// ServiceEditorDialog Implementation
ServiceEditorDialog::ServiceEditorDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "Add Custom Service", wxDefaultPosition, wxSize(450, 400)),
      mainSizer(nullptr), formSizer(nullptr), nameEdit(nullptr), keyNameEdit(nullptr),
      descriptionEdit(nullptr), testEndpointEdit(nullptr), testMethodCombo(nullptr),
      testHeadersEdit(nullptr), testableCheckBox(nullptr), okButton(nullptr), cancelButton(nullptr)
{
    setupUi();
    updateUi();
}

ServiceEditorDialog::ServiceEditorDialog(const services::CustomService &service, wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "Edit Custom Service", wxDefaultPosition, wxSize(450, 400)),
      mainSizer(nullptr), formSizer(nullptr), nameEdit(nullptr), keyNameEdit(nullptr),
      descriptionEdit(nullptr), testEndpointEdit(nullptr), testMethodCombo(nullptr),
      testHeadersEdit(nullptr), testableCheckBox(nullptr), okButton(nullptr), cancelButton(nullptr),
      currentService(service)
{
    setupUi();
    setService(service);
    updateUi();
}

void ServiceEditorDialog::setupUi()
{
    mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(mainSizer);

    // Form layout
    formSizer = new wxFlexGridSizer(2, 10, 10);
    formSizer->AddGrowableCol(1);

    // Name field
    formSizer->Add(new wxStaticText(this, wxID_ANY, "Service Name:"), 0, wxALIGN_CENTER_VERTICAL);
    nameEdit = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 0, wxTextValidator(wxFILTER_ALPHANUMERIC));
    nameEdit->SetHint("e.g., myapi");
    formSizer->Add(nameEdit, 1, wxEXPAND);

    // Key Name field
    formSizer->Add(new wxStaticText(this, wxID_ANY, "Key Name:"), 0, wxALIGN_CENTER_VERTICAL);
    keyNameEdit = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 0, wxTextValidator(wxFILTER_ALPHANUMERIC));
    keyNameEdit->SetHint("e.g., MYAPI_API_KEY");
    formSizer->Add(keyNameEdit, 1, wxEXPAND);

    // Description field
    formSizer->Add(new wxStaticText(this, wxID_ANY, "Description:"), 0, wxALIGN_TOP);
    descriptionEdit = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 60), wxTE_MULTILINE);
    descriptionEdit->SetHint("Optional description of the service");
    formSizer->Add(descriptionEdit, 1, wxEXPAND);

    // Testable checkbox
    formSizer->Add(new wxStaticText(this, wxID_ANY, ""), 0);
    testableCheckBox = new wxCheckBox(this, wxID_ANY, "Enable testing for this service");
    formSizer->Add(testableCheckBox, 1, wxEXPAND);

    // Test Endpoint field
    formSizer->Add(new wxStaticText(this, wxID_ANY, "Test Endpoint:"), 0, wxALIGN_CENTER_VERTICAL);
    testEndpointEdit = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize);
    testEndpointEdit->SetHint("https://api.example.com/v1/status");
    formSizer->Add(testEndpointEdit, 1, wxEXPAND);

    // Test Method field
    formSizer->Add(new wxStaticText(this, wxID_ANY, "Test Method:"), 0, wxALIGN_CENTER_VERTICAL);
    testMethodCombo = new wxComboBox(this, wxID_ANY, "GET", wxDefaultPosition, wxDefaultSize,
                                   wxArrayString{"GET", "POST", "PUT", "DELETE", "HEAD"}, wxCB_READONLY);
    formSizer->Add(testMethodCombo, 1, wxEXPAND);

    // Test Headers field
    formSizer->Add(new wxStaticText(this, wxID_ANY, "Additional Headers:"), 0, wxALIGN_CENTER_VERTICAL);
    testHeadersEdit = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize);
    testHeadersEdit->SetHint("-H 'Content-Type: application/json'");
    formSizer->Add(testHeadersEdit, 1, wxEXPAND);

    mainSizer->Add(formSizer, 1, wxEXPAND | wxALL, 10);

    // Button box
    wxSizer *buttonSizer = CreateButtonSizer(wxOK | wxCANCEL);
    mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 10);

    // Get buttons for validation
    wxWindowList &children = GetChildren();
    for (wxWindowList::iterator it = children.begin(); it != children.end(); ++it) {
        wxButton *button = wxDynamicCast(*it, wxButton);
        if (button) {
            if (button->GetId() == wxID_OK) {
                okButton = button;
            } else if (button->GetId() == wxID_CANCEL) {
                cancelButton = button;
            }
        }
    }

    validateInput();
}

void ServiceEditorDialog::setService(const services::CustomService &service)
{
    currentService = service;
    nameEdit->SetValue(wxString::FromUTF8(service.name));
    keyNameEdit->SetValue(wxString::FromUTF8(service.keyName));
    descriptionEdit->SetValue(wxString::FromUTF8(service.description));
    testableCheckBox->SetValue(service.testable);
    testEndpointEdit->SetValue(wxString::FromUTF8(service.testEndpoint));
    testMethodCombo->SetValue(wxString::FromUTF8(service.testMethod));
    testHeadersEdit->SetValue(wxString::FromUTF8(service.testHeaders));
}

services::CustomService ServiceEditorDialog::getService() const
{
    services::CustomService service;
    service.name = nameEdit->GetValue().ToStdString();
    service.keyName = keyNameEdit->GetValue().ToStdString();
    service.description = descriptionEdit->GetValue().ToStdString();
    service.testable = testableCheckBox->GetValue();
    service.testEndpoint = testEndpointEdit->GetValue().ToStdString();
    service.testMethod = testMethodCombo->GetValue().ToStdString();
    service.testHeaders = testHeadersEdit->GetValue().ToStdString();
    return service;
}

void ServiceEditorDialog::validateInput()
{
    bool isValid = !nameEdit->GetValue().IsEmpty() && !keyNameEdit->GetValue().IsEmpty();
    if (okButton) {
        okButton->Enable(isValid);
    }
}

void ServiceEditorDialog::updateUi()
{
    OnTestableChanged(wxCommandEvent());
}

void ServiceEditorDialog::OnTestableChanged(wxCommandEvent& event)
{
    bool testable = testableCheckBox->GetValue();
    testEndpointEdit->Enable(testable);
    testMethodCombo->Enable(testable);
    testHeadersEdit->Enable(testable);
}

void ServiceEditorDialog::OnNameChanged(wxCommandEvent& event)
{
    validateInput();
}

void ServiceEditorDialog::OnKeyNameChanged(wxCommandEvent& event)
{
    validateInput();
}

void ServiceEditorDialog::OnOk(wxCommandEvent& event)
{
    if (validateInput()) {
        EndModal(wxID_OK);
    }
}

void ServiceEditorDialog::OnCancel(wxCommandEvent& event)
{
    EndModal(wxID_CANCEL);
}

// Event table for ServiceManagerWidget
wxBEGIN_EVENT_TABLE(ServiceManagerWidget, wxPanel)
    EVT_BUTTON(ID_ADD_SERVICE, ServiceManagerWidget::OnAddService)
    EVT_BUTTON(ID_EDIT_SERVICE, ServiceManagerWidget::OnEditService)
    EVT_BUTTON(ID_DELETE_SERVICE, ServiceManagerWidget::OnDeleteService)
    EVT_BUTTON(ID_REFRESH_SERVICES, ServiceManagerWidget::OnRefreshServices)
    EVT_BUTTON(ID_TEST_SELECTED_SERVICE, ServiceManagerWidget::OnTestSelectedService)
    EVT_BUTTON(ID_TEST_ALL_SERVICES, ServiceManagerWidget::OnTestAllServices)
    EVT_BUTTON(ID_EXPORT_SERVICES, ServiceManagerWidget::OnExportServices)
    EVT_BUTTON(ID_IMPORT_SERVICES, ServiceManagerWidget::OnImportServices)
    EVT_TEXT(ID_SEARCH_SERVICES, ServiceManagerWidget::OnSearchServices)
    EVT_LIST_ITEM_ACTIVATED(ID_SERVICE_TABLE, ServiceManagerWidget::OnTableItemActivated)
    EVT_LIST_ITEM_SELECTED(ID_SERVICE_TABLE, ServiceManagerWidget::OnTableSelectionChanged)
    EVT_CONTEXT_MENU(ServiceManagerWidget::OnContextMenu)
    EVT_MENU(ID_ADD_SERVICE_MENU, ServiceManagerWidget::OnAddService)
    EVT_MENU(ID_EDIT_SERVICE_MENU, ServiceManagerWidget::OnEditService)
    EVT_MENU(ID_DELETE_SERVICE_MENU, ServiceManagerWidget::OnDeleteService)
    EVT_MENU(ID_TEST_SERVICE_MENU, ServiceManagerWidget::OnTestSelectedService)
    EVT_MENU(ID_COPY_NAME_MENU, ServiceManagerWidget::OnCopyName)
    EVT_MENU(ID_DUPLICATE_SERVICE_MENU, ServiceManagerWidget::OnDuplicateService)
wxEND_EVENT_TABLE()

// ServiceManagerWidget Implementation
ServiceManagerWidget::ServiceManagerWidget(const core::Config& config, wxWindow *parent)
    : wxPanel(parent, wxID_ANY), config(config), mainSizer(nullptr), toolbarSizer(nullptr),
      searchEdit(nullptr), addButton(nullptr), editButton(nullptr), deleteButton(nullptr),
      refreshButton(nullptr), testSelectedButton(nullptr), testAllButton(nullptr),
      exportButton(nullptr), importButton(nullptr), statusLabel(nullptr), table(nullptr),
      contextMenu(nullptr)
{
    setupUi();
    setupTable();
    setupToolbar();
    setupContextMenu();
    loadServices();
}

void ServiceManagerWidget::setupUi()
{
    mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(mainSizer);

    // Create toolbar layout
    toolbarSizer = new wxBoxSizer(wxHORIZONTAL);
    mainSizer->Add(toolbarSizer, 0, wxEXPAND | wxALL, 5);

    // Create table
    table = new wxListCtrl(this, ID_SERVICE_TABLE, wxDefaultPosition, wxDefaultSize,
                          wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);
    mainSizer->Add(table, 1, wxEXPAND | wxALL, 5);

    // Status label
    statusLabel = new wxStaticText(this, wxID_ANY, "Ready");
    statusLabel->SetForegroundColour(wxColour(100, 100, 100));
    wxFont smallFont = statusLabel->GetFont();
    smallFont.SetPointSize(smallFont.GetPointSize() - 1);
    statusLabel->SetFont(smallFont);
    mainSizer->Add(statusLabel, 0, wxALL, 5);
}

void ServiceManagerWidget::setupTable()
{
    // Set up columns
    table->InsertColumn(ColumnName, "Name", wxLIST_FORMAT_LEFT, 120);
    table->InsertColumn(ColumnKeyName, "Key Name", wxLIST_FORMAT_LEFT, 150);
    table->InsertColumn(ColumnDescription, "Description", wxLIST_FORMAT_LEFT, 200);
    table->InsertColumn(ColumnTestable, "Testable", wxLIST_FORMAT_LEFT, 80);
    table->InsertColumn(ColumnTestStatus, "Status", wxLIST_FORMAT_LEFT, 100);
}

void ServiceManagerWidget::setupToolbar()
{
    // Search
    searchEdit = new wxTextCtrl(this, ID_SEARCH_SERVICES, "", wxDefaultPosition, wxSize(200, -1));
    searchEdit->SetHint("Search services...");
    toolbarSizer->Add(searchEdit, 0, wxALL, 2);

    toolbarSizer->AddStretchSpacer();

    // Action buttons
    addButton = new wxButton(this, ID_ADD_SERVICE, "Add Service");
    toolbarSizer->Add(addButton, 0, wxALL, 2);

    editButton = new wxButton(this, ID_EDIT_SERVICE, "Edit");
    editButton->Enable(false);
    toolbarSizer->Add(editButton, 0, wxALL, 2);

    deleteButton = new wxButton(this, ID_DELETE_SERVICE, "Delete");
    deleteButton->Enable(false);
    deleteButton->SetForegroundColour(wxColour(255, 68, 68));
    toolbarSizer->Add(deleteButton, 0, wxALL, 2);

    toolbarSizer->AddSpacer(15);

    testSelectedButton = new wxButton(this, ID_TEST_SELECTED_SERVICE, "Test Selected");
    testSelectedButton->Enable(false);
    toolbarSizer->Add(testSelectedButton, 0, wxALL, 2);

    testAllButton = new wxButton(this, ID_TEST_ALL_SERVICES, "Test All");
    toolbarSizer->Add(testAllButton, 0, wxALL, 2);

    toolbarSizer->AddSpacer(15);

    exportButton = new wxButton(this, ID_EXPORT_SERVICES, "Export");
    toolbarSizer->Add(exportButton, 0, wxALL, 2);

    importButton = new wxButton(this, ID_IMPORT_SERVICES, "Import");
    toolbarSizer->Add(importButton, 0, wxALL, 2);

    refreshButton = new wxButton(this, ID_REFRESH_SERVICES, "Refresh");
    toolbarSizer->Add(refreshButton, 0, wxALL, 2);
}

void ServiceManagerWidget::setupContextMenu()
{
    contextMenu = new wxMenu();

    addMenuItem = contextMenu->Append(ID_ADD_SERVICE_MENU, "Add Service");
    contextMenu->AppendSeparator();

    editMenuItem = contextMenu->Append(ID_EDIT_SERVICE_MENU, "Edit");
    deleteMenuItem = contextMenu->Append(ID_DELETE_SERVICE_MENU, "Delete");

    contextMenu->AppendSeparator();

    testMenuItem = contextMenu->Append(ID_TEST_SERVICE_MENU, "Test Service");

    contextMenu->AppendSeparator();

    copyNameMenuItem = contextMenu->Append(ID_COPY_NAME_MENU, "Copy Name");
    duplicateMenuItem = contextMenu->Append(ID_DUPLICATE_SERVICE_MENU, "Duplicate");
}

void ServiceManagerWidget::loadServices()
{
    try {
        customServices = services::loadCustomServices(config);
        updateTable();
        showSuccess(wxString::Format("Loaded %zu custom services", customServices.size()));
    } catch (const std::exception& e) {
        showError(wxString::Format("Failed to load services: %s", e.what()));
    }
}

void ServiceManagerWidget::saveServices()
{
    try {
        services::saveCustomServices(config, customServices);
        showSuccess("Services saved successfully");
    } catch (const std::exception& e) {
        showError(wxString::Format("Failed to save services: %s", e.what()));
    }
}

void ServiceManagerWidget::updateTable()
{
    table->DeleteAllItems();

    for (size_t i = 0; i < customServices.size(); ++i) {
        const auto& service = customServices[i];

        // Filter check
        if (!currentFilter.IsEmpty()) {
            wxString serviceName = wxString::FromUTF8(service.name);
            wxString keyName = wxString::FromUTF8(service.keyName);
            wxString description = wxString::FromUTF8(service.description);

            if (!serviceName.Contains(currentFilter) &&
                !keyName.Contains(currentFilter) &&
                !description.Contains(currentFilter)) {
                continue;
            }
        }

        addServiceToTable(service);
    }
}

void ServiceManagerWidget::addServiceToTable(const services::CustomService &service)
{
    long index = table->GetItemCount();
    long itemIndex = table->InsertItem(index, wxString::FromUTF8(service.name));

    table->SetItem(itemIndex, ColumnKeyName, wxString::FromUTF8(service.keyName));

    wxString desc = wxString::FromUTF8(service.description);
    if (desc.Length() > 50) {
        desc = desc.Left(47) + "...";
    }
    table->SetItem(itemIndex, ColumnDescription, desc);
    table->SetItemData(itemIndex, index);

    table->SetItem(itemIndex, ColumnTestable, service.testable ? "Yes" : "No");
    table->SetItem(itemIndex, ColumnTestStatus, "Not tested");
}

void ServiceManagerWidget::refreshServices()
{
    loadServices();
}

void ServiceManagerWidget::selectService(const wxString &serviceName)
{
    for (long i = 0; i < table->GetItemCount(); ++i) {
        if (table->GetItemText(i) == serviceName) {
            table->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
            table->EnsureVisible(i);
            break;
        }
    }
}

void ServiceManagerWidget::OnAddService(wxCommandEvent& event)
{
    ServiceEditorDialog dialog(this);
    if (dialog.ShowModal() == wxID_OK) {
        auto service = dialog.getService();

        // Check if service name already exists
        for (const auto& existing : customServices) {
            if (existing.name == service.name) {
                showError("A service with this name already exists");
                return;
            }
        }

        try {
            services::addCustomService(config, service);
            refreshServices();
            selectService(wxString::FromUTF8(service.name));
            showSuccess("Service added successfully");
        } catch (const std::exception& e) {
            showError(wxString::Format("Failed to add service: %s", e.what()));
        }
    }
}

void ServiceManagerWidget::OnEditService(wxCommandEvent& event)
{
    long selectedIndex = table->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (selectedIndex == -1 || selectedIndex >= static_cast<long>(customServices.size())) {
        return;
    }

    auto service = customServices[selectedIndex];
    ServiceEditorDialog dialog(service, this);
    if (dialog.ShowModal() == wxID_OK) {
        auto updatedService = dialog.getService();

        // Check if name conflicts with other services
        for (size_t i = 0; i < customServices.size(); ++i) {
            if (i != static_cast<size_t>(selectedIndex) && customServices[i].name == updatedService.name) {
                showError("A service with this name already exists");
                return;
            }
        }

        try {
            // Remove old service and add updated one
            services::removeCustomService(config, service.name);
            services::addCustomService(config, updatedService);
            refreshServices();
            selectService(wxString::FromUTF8(updatedService.name));
            showSuccess("Service updated successfully");
        } catch (const std::exception& e) {
            showError(wxString::Format("Failed to update service: %s", e.what()));
        }
    }
}

void ServiceManagerWidget::OnDeleteService(wxCommandEvent& event)
{
    long selectedIndex = table->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (selectedIndex == -1 || selectedIndex >= static_cast<long>(customServices.size())) {
        return;
    }

    auto service = customServices[selectedIndex];
    wxString serviceName = wxString::FromUTF8(service.name);

    wxMessageDialog dialog(this,
                          wxString::Format("Are you sure you want to delete the service '%s'?\n\n"
                                         "This action cannot be undone.", serviceName),
                          "Delete Service",
                          wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);

    if (dialog.ShowModal() == wxID_YES) {
        try {
            services::removeCustomService(config, service.name);
            refreshServices();
            showSuccess("Service deleted successfully");
        } catch (const std::exception& e) {
            showError(wxString::Format("Failed to delete service: %s", e.what()));
        }
    }
}

void ServiceManagerWidget::OnSearchServices(wxCommandEvent& event)
{
    currentFilter = searchEdit->GetValue();
    updateTable();
}

void ServiceManagerWidget::OnTestSelectedService(wxCommandEvent& event)
{
    long selectedIndex = table->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (selectedIndex == -1 || selectedIndex >= static_cast<long>(customServices.size())) {
        return;
    }

    auto service = customServices[selectedIndex];
    wxString serviceName = wxString::FromUTF8(service.name);

    table->SetItem(selectedIndex, ColumnTestStatus, "Testing...");

    // TODO: Implement service testing
    // For now, just show a placeholder
    wxTimer *timer = new wxTimer(this);
    timer->StartOnce(1000);
    Bind(wxEVT_TIMER, [this, selectedIndex, serviceName](wxTimerEvent&) {
        updateTestStatus(serviceName, true, "Test completed (placeholder)");
    }, timer->GetId());
}

void ServiceManagerWidget::OnTestAllServices(wxCommandEvent& event)
{
    // TODO: Implement testing all services
    showSuccess("Test all services - feature not yet implemented");
}

void ServiceManagerWidget::OnContextMenu(wxContextMenuEvent& event)
{
    long selectedIndex = table->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    bool hasSelection = selectedIndex != -1;

    editMenuItem->Enable(hasSelection);
    deleteMenuItem->Enable(hasSelection);
    testMenuItem->Enable(hasSelection);
    copyNameMenuItem->Enable(hasSelection);
    duplicateMenuItem->Enable(hasSelection);

    PopupMenu(contextMenu);
}

void ServiceManagerWidget::OnTableItemActivated(wxListEvent& event)
{
    OnEditService(wxCommandEvent());
}

void ServiceManagerWidget::OnTableSelectionChanged(wxListEvent& event)
{
    long selectedIndex = table->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    bool hasSelection = selectedIndex != -1;

    editButton->Enable(hasSelection);
    deleteButton->Enable(hasSelection);
    testSelectedButton->Enable(hasSelection);
}

void ServiceManagerWidget::OnExportServices(wxCommandEvent& event)
{
    wxFileDialog dialog(this, "Export Services", "", "custom_services.json",
                       "JSON Files (*.json)|*.json", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (dialog.ShowModal() == wxID_OK) {
        wxString filePath = dialog.GetPath();

        try {
            // TODO: Implement JSON export
            showSuccess(wxString::Format("Export to %s - feature not yet implemented", filePath));
        } catch (const std::exception& e) {
            showError(wxString::Format("Export failed: %s", e.what()));
        }
    }
}

void ServiceManagerWidget::OnImportServices(wxCommandEvent& event)
{
    wxFileDialog dialog(this, "Import Services", "", "",
                       "JSON Files (*.json)|*.json", wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dialog.ShowModal() == wxID_OK) {
        wxString filePath = dialog.GetPath();

        try {
            // TODO: Implement JSON import
            showSuccess(wxString::Format("Import from %s - feature not yet implemented", filePath));
        } catch (const std::exception& e) {
            showError(wxString::Format("Import failed: %s", e.what()));
        }
    }
}

void ServiceManagerWidget::OnRefreshServices(wxCommandEvent& event)
{
    refreshServices();
}

void ServiceManagerWidget::OnDuplicateService(wxCommandEvent& event)
{
    long selectedIndex = table->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (selectedIndex == -1 || selectedIndex >= static_cast<long>(customServices.size())) {
        return;
    }

    auto service = customServices[selectedIndex];
    service.name += "_copy";

    ServiceEditorDialog dialog(service, this);
    if (dialog.ShowModal() == wxID_OK) {
        auto newService = dialog.getService();

        try {
            services::addCustomService(config, newService);
            refreshServices();
            showSuccess("Service duplicated successfully");
        } catch (const std::exception& e) {
            showError(wxString::Format("Failed to duplicate service: %s", e.what()));
        }
    }
}

void ServiceManagerWidget::OnCopyName(wxCommandEvent& event)
{
    long selectedIndex = table->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (selectedIndex != -1) {
        wxString serviceName = table->GetItemText(selectedIndex);
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(serviceName));
            wxTheClipboard->Close();
            showSuccess("Service name copied to clipboard");
        }
    }
}

void ServiceManagerWidget::updateTestStatus(const wxString &serviceName, bool success, const wxString &message)
{
    for (long i = 0; i < table->GetItemCount(); ++i) {
        if (table->GetItemText(i) == serviceName) {
            wxString statusText = success ? "Success" : "Failed";
            if (!message.IsEmpty()) {
                statusText += ": " + message;
            }
            table->SetItem(i, ColumnTestStatus, statusText);
            break;
        }
    }
}

bool ServiceManagerWidget::validateServiceName(const wxString &name)
{
    if (name.IsEmpty()) {
        return false;
    }

    wxRegEx regEx("^[A-Za-z0-9_-]+$");
    return regEx.Matches(name);
}

void ServiceManagerWidget::showError(const wxString &message)
{
    statusLabel->SetLabel(wxString::Format("Error: %s", message));
    statusLabel->SetForegroundColour(wxColour(255, 68, 68));

    // Reset after 5 seconds
    wxTimer *timer = new wxTimer(this);
    timer->StartOnce(5000);
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
        statusLabel->SetLabel("Ready");
        statusLabel->SetForegroundColour(wxColour(100, 100, 100));
    }, timer->GetId());
}

void ServiceManagerWidget::showSuccess(const wxString &message)
{
    statusLabel->SetLabel(message);
    statusLabel->SetForegroundColour(wxColour(0, 170, 0));

    // Reset after 3 seconds
    wxTimer *timer = new wxTimer(this);
    timer->StartOnce(3000);
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
        statusLabel->SetLabel("Ready");
        statusLabel->SetForegroundColour(wxColour(100, 100, 100));
    }, timer->GetId());
}

void ServiceManagerWidget::SendStatusEvent(const wxString &message)
{
    // Custom status event - could be used by parent window
    wxCommandEvent event(wxEVT_COMMAND_TEXT_UPDATED, wxID_ANY);
    event.SetString(message);
    wxPostEvent(GetParent(), event);
}

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI