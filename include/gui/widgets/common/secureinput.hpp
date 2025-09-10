#pragma once

#ifdef BUILD_GUI

#include <wx/wx.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/sizer.h>

namespace ak {
namespace gui {
namespace widgets {

class SecureInputWidget : public wxPanel
{
public:
    explicit SecureInputWidget(wxWindow *parent = nullptr);
    
    void SetText(const wxString &text);
    wxString GetText() const;
    void SetPlaceholderText(const wxString &text);
    void SetReadOnly(bool readOnly);
    bool IsReadOnly() const;
    
    // Show/hide functionality
    void SetMasked(bool masked);
    bool IsMasked() const;
    
    // Validation
    void SetValid(bool valid);
    bool IsValid() const;

    // Event handlers
    void OnToggleVisibility(wxCommandEvent &event);
    void OnTextChanged(wxCommandEvent &event);

    // Events
    wxDECLARE_EVENT_TABLE();

private:
    void setupUi();
    void updateVisibility();
    void updateStyleSheet();
    
    wxTextCtrl *lineEdit;
    wxButton *toggleButton;
    wxBoxSizer *layout;
    
    bool masked;
    bool valid;
    wxString originalText;
};

// Event IDs
enum {
    ID_SECURE_TEXT = wxID_HIGHEST + 1000,
    ID_TOGGLE_VISIBILITY
};

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI