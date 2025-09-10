#pragma once

#ifdef BUILD_GUI

#include "core/config.hpp"
#include "services/services.hpp"
#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/gauge.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/thread.h>
#include <wx/datetime.h>
#include <chrono>
#include <mutex>
#include <vector>

namespace ak {
namespace gui {
namespace widgets {

// Forward declarations
class ServiceTestWorker;

// Custom list item data for service display
struct ServiceListItemData
{
    enum TestStatus {
        NotTested,
        Testing,
        Success,
        Failed,
        Skipped
    };

    wxString serviceName;
    TestStatus status;
    std::chrono::milliseconds responseTime;
    wxString errorMessage;

    ServiceListItemData(const wxString& name)
        : serviceName(name), status(NotTested), responseTime(0) {}

    void setTestStatus(TestStatus status);
    TestStatus getTestStatus() const { return status; }

    void setResponseTime(std::chrono::milliseconds duration);
    void setErrorMessage(const wxString& error);

    wxString getServiceName() const { return serviceName; }
    wxString getErrorMessage() const { return errorMessage; }
    std::chrono::milliseconds getResponseTime() const { return responseTime; }

    wxString getDisplayText() const;
    wxString getStatusText(TestStatus status) const;
    wxString getToolTipText() const;
};

// Worker thread for service testing
class ServiceTestWorker : public wxThread
{
public:
    explicit ServiceTestWorker(const core::Config& config);

    void setServices(const std::vector<wxString>& services);
    void setFailFast(bool failFast);

    // Thread entry point
    virtual ExitCode Entry() override;

    // Event handling
    void OnServiceTestStarted(const wxString& serviceName);
    void OnServiceTestCompleted(const wxString& serviceName, bool success,
                               std::chrono::milliseconds duration, const wxString& error);
    void OnAllTestsCompleted();
    void OnProgress(int current, int total);

private:
    void testService(const wxString& serviceName);

    const core::Config& config;
    std::vector<wxString> servicesToTest;
    bool failFast;
    std::mutex mutex;
};

// Service Tester Widget
class ServiceTesterWidget : public wxPanel
{
public:
    explicit ServiceTesterWidget(const core::Config& config, wxWindow* parent = nullptr);
    ~ServiceTesterWidget();

    void refreshServices();

    // Event handlers
    void OnTestAll(wxCommandEvent& event);
    void OnTestSelected(wxCommandEvent& event);
    void OnStop(wxCommandEvent& event);
    void OnClear(wxCommandEvent& event);
    void OnRefresh(wxCommandEvent& event);
    void OnExport(wxCommandEvent& event);
    void OnServiceSelectionChanged(wxListEvent& event);
    void OnServiceTestStarted(wxThreadEvent& event);
    void OnServiceTestCompleted(wxThreadEvent& event);
    void OnAllTestsCompleted(wxThreadEvent& event);
    void OnTestProgress(wxThreadEvent& event);
    void OnElapsedTimeUpdate(wxTimerEvent& event);

private:
    void setupUi();
    void setupServiceList();
    void setupControls();
    void setupResultsPanel();
    void loadAvailableServices();
    void updateServiceItem(long itemIndex, ServiceListItemData::TestStatus status,
                          std::chrono::milliseconds duration = std::chrono::milliseconds(0),
                          const wxString& error = wxEmptyString);
    void resetAllResults();
    void startTestSession();
    void endTestSession();
    void updateStatusBar();
    void showError(const wxString& message);
    void showSuccess(const wxString& message);

    // Configuration and data
    const core::Config& config;
    std::vector<wxString> availableServices;
    std::vector<wxString> configuredServices;
    std::vector<ServiceListItemData> serviceItems;

    // UI components
    wxBoxSizer* mainSizer;
    wxSplitterWindow* splitter;

    // Service list
    wxListCtrl* serviceList;
    wxStaticText* serviceListLabel;

    // Controls
    wxButton* testAllButton;
    wxButton* testSelectedButton;
    wxButton* stopButton;
    wxButton* clearButton;
    wxButton* refreshButton;
    wxButton* exportButton;

    // Progress and status
    wxGauge* progressBar;
    wxStaticText* progressLabel;
    wxStaticText* elapsedTimeLabel;
    wxStaticText* statusLabel;

    // Results panel
    wxTextCtrl* resultsText;

    // Testing infrastructure
    ServiceTestWorker* worker;
    wxTimer* elapsedTimer;

    // Test session state
    bool testingInProgress;
    wxDateTime testStartTime;
    int totalTests;
    int completedTests;
    int successfulTests;
    int failedTests;

    // Event table
    DECLARE_EVENT_TABLE()
};

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI