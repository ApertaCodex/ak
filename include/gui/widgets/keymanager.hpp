#pragma once

#ifdef BUILD_GUI

#include "core/config.hpp"
#include <wx/wx.h>
#include <wx/grid.h>
#include <wx/listctrl.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/menu.h>
#include <wx/timer.h>
#include <wx/event.h>
#include <wx/string.h>
#include <wx/clipbrd.h>
#include <vector>
#include <string>
#include <map>

namespace ak {
namespace gui {
namespace widgets {

// Forward declarations
class SecureInputWidget;

// Forward declarations
class KeyManagerWidget;

// Custom grid renderer for masked values
class MaskedCellRenderer : public wxGridCellStringRenderer
{
public:
    MaskedCellRenderer(KeyManagerWidget* widget = nullptr);
    void Draw(wxGrid& grid, wxGridCellAttr& attr, wxDC& dc,
              const wxRect& rect, int row, int col, bool isSelected) override;

private:
    KeyManagerWidget* widget;
};

// Key Manager Widget
class KeyManagerWidget : public wxPanel
{
    friend class MaskedCellRenderer;
public:
    explicit KeyManagerWidget(const core::Config& config, wxWindow *parent = nullptr);
    virtual ~KeyManagerWidget();
    
    // Public interface
    void RefreshKeys();
    void SelectKey(const wxString &keyName);

    // Event handlers
    void OnAddKey(wxCommandEvent& event);
    void OnEditKey(wxCommandEvent& event);
    void OnDeleteKey(wxCommandEvent& event);
    void OnSearch(wxCommandEvent& event);
    void OnToggleKeyVisibility(wxCommandEvent& event);
    void OnTestSelectedKey(wxCommandEvent& event);
    void OnTestAllKeys(wxCommandEvent& event);
    void OnContextMenu(wxContextMenuEvent& event);
    void OnGridCellChanged(wxGridEvent& event);
    void OnGridCellSelected(wxGridEvent& event);
    void OnGridCellRightClick(wxGridEvent& event);
    void OnCopyKeyName(wxCommandEvent& event);
    void OnCopyKeyValue(wxCommandEvent& event);

private:
    void SetupUi();
    void SetupGrid();
    void SetupToolbar();
    void SetupContextMenu();
    void LoadKeys();
    void SaveKeys();
    void UpdateGrid();
    void FilterGrid(const wxString &filter);
    
    // Utility methods
    void AddKeyToGrid(const wxString &name, const wxString &value,
                     const wxString &service, const wxString &apiUrl);
    wxString DetectService(const wxString &keyName);
    wxString GetServiceApiUrl(const wxString &service);
    wxString GetServiceCode(const wxString &displayName);
    void UpdateTestStatus(const wxString &keyName, bool success, const wxString &message = wxEmptyString);
    bool ValidateKeyName(const wxString &name);
    void ShowError(const wxString &message);
    void ShowSuccess(const wxString &message);
    void SendStatusEvent(const wxString &message);
    
    // Configuration and data
    const core::Config& config;
    core::KeyStore keyStore;
    
    // UI components
    wxBoxSizer *mainSizer;
    wxBoxSizer *toolbarSizer;
    
    // Toolbar components
    wxTextCtrl *searchEdit;
    wxButton *addButton;
    wxButton *editButton;
    wxButton *deleteButton;
    wxButton *toggleVisibilityButton;
    wxButton *refreshButton;
    wxButton *testSelectedButton;
    wxButton *testAllButton;
    wxStaticText *statusLabel;
    
    // Grid for keys
    wxGrid *grid;
    
    // Context menu
    wxMenu *contextMenu;
    
    // State
    bool globalVisibilityState;
    wxString currentFilter;
    std::map<int, wxString> gridRowKeyMap; // Maps grid rows to key names
    std::map<wxString, bool> keyVisibilityMap; // Maps key names to visibility state

    // Grid columns
    enum GridColumn {
        ColName = 0,
        ColService = 1,
        ColUrl = 2,
        ColValue = 3,
        ColTestStatus = 4,
        ColCount = 5
    };
    
    // Custom event IDs
    enum {
        ID_ADD_KEY = wxID_HIGHEST + 100,
        ID_EDIT_KEY,
        ID_DELETE_KEY,
        ID_SEARCH,
        ID_TOGGLE_VISIBILITY,
        ID_TEST_SELECTED,
        ID_TEST_ALL,
        ID_COPY_NAME,
        ID_COPY_VALUE,
        ID_GRID
    };

    // wxWidgets event table
    wxDECLARE_EVENT_TABLE();
};

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI