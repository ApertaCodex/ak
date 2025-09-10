#ifdef BUILD_GUI

#include "gui/widgets/common/secureinput.hpp"

namespace ak {
namespace gui {
namespace widgets {

// Event table implementation
wxBEGIN_EVENT_TABLE(SecureInputWidget, wxPanel)
    EVT_BUTTON(wxID_ANY, SecureInputWidget::OnToggleVisibility)
    EVT_TEXT(wxID_ANY, SecureInputWidget::OnTextChanged)
wxEND_EVENT_TABLE()

SecureInputWidget::SecureInputWidget(wxWindow *parent)
    : wxPanel(parent, wxID_ANY), 
      lineEdit(nullptr), toggleButton(nullptr), layout(nullptr),
      masked(true), valid(true)
{
    setupUi();
    updateVisibility();
    updateStyleSheet();
}

void SecureInputWidget::setupUi()
{
    layout = new wxBoxSizer(wxHORIZONTAL);
    
    lineEdit = new wxTextCtrl(this, wxID_ANY, wxEmptyString, 
                              wxDefaultPosition, wxDefaultSize, 
                              wxTE_PASSWORD);
    
    toggleButton = new wxButton(this, wxID_ANY, "👁", 
                                wxDefaultPosition, wxSize(24, 24),
                                wxBORDER_NONE);
    toggleButton->SetToolTip("Toggle visibility");
    
    layout->Add(lineEdit, 1, wxEXPAND | wxRIGHT, 2);
    layout->Add(toggleButton, 0, wxALIGN_CENTER_VERTICAL);
    
    SetSizer(layout);
}

void SecureInputWidget::SetText(const wxString &text)
{
    originalText = text;
    lineEdit->SetValue(text);
}

wxString SecureInputWidget::GetText() const
{
    return lineEdit->GetValue();
}

void SecureInputWidget::SetPlaceholderText(const wxString &text)
{
    // wxWidgets doesn't have built-in placeholder, but we could implement it
    // with event handling if needed. For now, just store the value.
    lineEdit->SetHint(text);
}

void SecureInputWidget::SetReadOnly(bool readOnly)
{
    lineEdit->SetEditable(!readOnly);
    toggleButton->Enable(!readOnly);
}

bool SecureInputWidget::IsReadOnly() const
{
    return !lineEdit->IsEditable();
}

void SecureInputWidget::SetMasked(bool masked)
{
    this->masked = masked;
    updateVisibility();
}

bool SecureInputWidget::IsMasked() const
{
    return masked;
}

void SecureInputWidget::SetValid(bool valid)
{
    this->valid = valid;
    updateStyleSheet();
}

bool SecureInputWidget::IsValid() const
{
    return valid;
}

void SecureInputWidget::OnToggleVisibility(wxCommandEvent& event)
{
    SetMasked(!masked);
    
    // Allow event to propagate to parent handlers
    event.Skip();
}

void SecureInputWidget::OnTextChanged(wxCommandEvent& event)
{
    originalText = lineEdit->GetValue();
    
    // Create and send a custom event
    wxCommandEvent textEvent(wxEVT_COMMAND_TEXT_UPDATED, GetId());
    textEvent.SetString(originalText);
    GetEventHandler()->ProcessEvent(textEvent);
    
    // Allow event to propagate to parent handlers
    event.Skip();
}

void SecureInputWidget::updateVisibility()
{
    if (masked) {
        // Set password mode
        long style = lineEdit->GetWindowStyle();
        lineEdit->SetWindowStyle(style | wxTE_PASSWORD);
        toggleButton->SetLabel("👁");
        toggleButton->SetToolTip("Show value");
    } else {
        // Remove password mode
        long style = lineEdit->GetWindowStyle();
        lineEdit->SetWindowStyle(style & ~wxTE_PASSWORD);
        toggleButton->SetLabel("🙈");
        toggleButton->SetToolTip("Hide value");
    }
    
    // Need to refresh to apply style changes
    lineEdit->Refresh();
}

void SecureInputWidget::updateStyleSheet()
{
    if (valid) {
        lineEdit->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        lineEdit->SetToolTip("");
    } else {
        lineEdit->SetBackgroundColour(wxColour(255, 107, 107)); // #ff6b6b
        lineEdit->SetToolTip("Invalid value");
    }
    
    lineEdit->Refresh();
}

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI