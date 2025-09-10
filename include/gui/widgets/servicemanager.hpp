#pragma once

#ifdef BUILD_GUI

#include "core/config.hpp"
#include "services/services.hpp"
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
#include <vector>
#include <string>

namespace ak {
namespace gui {
namespace widgets {

// Service Editor Dialog
class ServiceEditorDialog : public wxDialog
{
public:
    explicit ServiceEditorDialog(wxWindow *parent = nullptr);
    explicit ServiceEditorDialog(const services::CustomService &service, wxWindow *parent = nullptr);

    services::CustomService getService() const;
    void setService(const services::CustomService &service);

private:
    void setupUi();
    void updateUi();
    void OnTestableChanged(wxCommandEvent& event);
    void OnNameChanged(wxCommandEvent& event);
    void OnKeyNameChanged(wxCommandEvent& event);
    void OnOk(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
    bool validateInput();

    wxBoxSizer *mainSizer;
    wxFlexGridSizer *formSizer;
    wxTextCtrl *nameEdit;
    wxTextCtrl *keyNameEdit;
    wxTextCtrl *descriptionEdit;
    wxTextCtrl *testEndpointEdit;
    wxComboBox *testMethodCombo;
    wxTextCtrl *testHeadersEdit;
    wxCheckBox *testableCheckBox;
    wxButton *okButton;
    wxButton *cancelButton;

    services::CustomService currentService;

    wxDECLARE_EVENT_TABLE();
};

// Custom Service Manager Widget
class ServiceManagerWidget : public wxPanel
{
public:
    explicit ServiceManagerWidget(const core::Config& config, wxWindow *parent = nullptr);

    // Public interface
    void refreshServices();
    void selectService(const wxString &serviceName);

private:
    void setupUi();
    void setupTable();
    void setupToolbar();
    void setupContextMenu();
    void loadServices();
    void saveServices();
    void updateTable();
    void filterTable(const wxString &filter);
    void addServiceToTable(const services::CustomService &service);
    void updateTestStatus(const wxString &serviceName, bool success, const wxString &message = "");
    bool validateServiceName(const wxString &name);
    void showError(const wxString &message);
    void showSuccess(const wxString &message);
    void SendStatusEvent(const wxString &message);

    // Event handlers
    void OnAddService(wxCommandEvent& event);
    void OnEditService(wxCommandEvent& event);
    void OnDeleteService(wxCommandEvent& event);
    void OnSearchServices(wxCommandEvent& event);
    void OnTestSelectedService(wxCommandEvent& event);
    void OnTestAllServices(wxCommandEvent& event);
    void OnContextMenu(wxContextMenuEvent& event);
    void OnTableItemActivated(wxListEvent& event);
    void OnTableSelectionChanged(wxListEvent& event);
    void OnExportServices(wxCommandEvent& event);
    void OnImportServices(wxCommandEvent& event);
    void OnRefreshServices(wxCommandEvent& event);
    void OnDuplicateService(wxCommandEvent& event);
    void OnCopyName(wxCommandEvent& event);

private:
    // Configuration and data
    const core::Config& config;
    std::vector<services::CustomService> customServices;

    // UI components
    wxBoxSizer *mainSizer;
    wxBoxSizer *toolbarSizer;

    // Toolbar components
    wxTextCtrl *searchEdit;
    wxButton *addButton;
    wxButton *editButton;
    wxButton *deleteButton;
    wxButton *refreshButton;
    wxButton *testSelectedButton;
    wxButton *testAllButton;
    wxButton *exportButton;
    wxButton *importButton;
    wxStaticText *statusLabel;

    // Table (ListCtrl in wxWidgets)
    wxListCtrl *table;

    // Context menu
    wxMenu *contextMenu;
    wxMenuItem *addMenuItem;
    wxMenuItem *editMenuItem;
    wxMenuItem *deleteMenuItem;
    wxMenuItem *testMenuItem;
    wxMenuItem *copyNameMenuItem;
    wxMenuItem *duplicateMenuItem;

    // Table columns
    enum TableColumn {
        ColumnName = 0,
        ColumnKeyName = 1,
        ColumnDescription = 2,
        ColumnTestable = 3,
        ColumnTestStatus = 4,
        ColumnCount = 5
    };

    // State
    wxString currentFilter;

    // wxWidgets event table
    wxDECLARE_EVENT_TABLE();
};

// Event IDs for ServiceManagerWidget
enum {
    ID_ADD_SERVICE = wxID_HIGHEST + 3000,
    ID_EDIT_SERVICE,
    ID_DELETE_SERVICE,
    ID_REFRESH_SERVICES,
    ID_TEST_SELECTED_SERVICE,
    ID_TEST_ALL_SERVICES,
    ID_EXPORT_SERVICES,
    ID_IMPORT_SERVICES,
    ID_SEARCH_SERVICES,
    ID_SERVICE_TABLE,
    ID_ADD_SERVICE_MENU,
    ID_EDIT_SERVICE_MENU,
    ID_DELETE_SERVICE_MENU,
    ID_TEST_SERVICE_MENU,
    ID_COPY_NAME_MENU,
    ID_DUPLICATE_SERVICE_MENU
};

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI