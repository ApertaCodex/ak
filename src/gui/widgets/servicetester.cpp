#ifdef BUILD_GUI

#include "gui/widgets/servicetester.hpp"
#include "gui/widgets/common/dialogs.hpp"
#include "services/services.hpp"
#include "storage/vault.hpp"
#include "core/config.hpp"
#include <wx/splitter.h>
#include <wx/statline.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/font.h>
#include <algorithm>

namespace ak {
namespace gui {
namespace widgets {

// ServiceListItemData Implementation
void ServiceListItemData::setTestStatus(TestStatus status)
{
    this->status = status;
}

void ServiceListItemData::setResponseTime(std::chrono::milliseconds duration)
{
    this->responseTime = duration;
}

void ServiceListItemData::setErrorMessage(const wxString& error)
{
    this->errorMessage = error;
}

wxString ServiceListItemData::getDisplayText() const
{
    wxString displayText = serviceName;

    if (status == Testing) {
        displayText += " (Testing...)";
    } else if (status == Success) {
        displayText += wxString::Format(" (✓ %lldms)", responseTime.count());
    } else if (status == Failed) {
        displayText += " (✗ Failed)";
    } else if (status == Skipped) {
        displayText += " (- Skipped)";
    }

    return displayText;
}

wxString ServiceListItemData::getStatusText(TestStatus status) const
{
    switch (status) {
    case NotTested: return "Not Tested";
    case Testing: return "Testing";
    case Success: return "Success";
    case Failed: return "Failed";
    case Skipped: return "Skipped";
    default: return "Unknown";
    }
}

wxString ServiceListItemData::getToolTipText() const
{
    wxString tooltip = wxString::Format("Service: %s\nStatus: %s",
                                       serviceName,
                                       getStatusText(status));

    if (status == Success && responseTime.count() > 0) {
        tooltip += wxString::Format("\nResponse Time: %lldms", responseTime.count());
    }

    if (status == Failed && !errorMessage.IsEmpty()) {
        tooltip += wxString::Format("\nError: %s", errorMessage);
    }

    return tooltip;
}

// ServiceTestWorker Implementation
ServiceTestWorker::ServiceTestWorker(const core::Config& config)
    : wxThread(wxTHREAD_JOINABLE), config(config), failFast(false)
{
}

void ServiceTestWorker::setServices(const std::vector<wxString>& services)
{
    std::lock_guard<std::mutex> lock(mutex);
    servicesToTest = services;
}

void ServiceTestWorker::setFailFast(bool failFast)
{
    std::lock_guard<std::mutex> lock(mutex);
    this->failFast = failFast;
}

wxThread::ExitCode ServiceTestWorker::Entry()
{
    std::vector<wxString> services;
    {
        std::lock_guard<std::mutex> lock(mutex);
        services = servicesToTest;
    }

    int current = 0;
    int total = static_cast<int>(services.size());

    for (const wxString& serviceName : services) {
        // Send progress event
        wxThreadEvent progressEvent(wxEVT_THREAD, wxID_ANY);
        progressEvent.SetInt(current);
        progressEvent.SetExtraLong(total);
        wxQueueEvent(GetEventHandler(), progressEvent.Clone());

        testService(serviceName);
        current++;

        // Check if we should fail fast
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (failFast) {
                // For now, we don't implement early termination logic
            }
        }
    }

    // Send completion event
    wxThreadEvent completionEvent(wxEVT_THREAD, wxID_ANY);
    wxQueueEvent(GetEventHandler(), completionEvent.Clone());

    return nullptr;
}

void ServiceTestWorker::OnServiceTestStarted(const wxString& serviceName)
{
    wxThreadEvent event(wxEVT_THREAD, wxID_ANY);
    event.SetString(serviceName);
    wxQueueEvent(GetEventHandler(), event.Clone());
}

void ServiceTestWorker::OnServiceTestCompleted(const wxString& serviceName, bool success,
                                             std::chrono::milliseconds duration, const wxString& error)
{
    wxThreadEvent event(wxEVT_THREAD, wxID_ANY);
    event.SetString(serviceName);
    event.SetInt(success ? 1 : 0);
    event.SetExtraLong(duration.count());
    wxQueueEvent(GetEventHandler(), event.Clone());
}

void ServiceTestWorker::OnAllTestsCompleted()
{
    wxThreadEvent event(wxEVT_THREAD, wxID_ANY);
    wxQueueEvent(GetEventHandler(), event.Clone());
}

void ServiceTestWorker::OnProgress(int current, int total)
{
    wxThreadEvent event(wxEVT_THREAD, wxID_ANY);
    event.SetInt(current);
    event.SetExtraLong(total);
    wxQueueEvent(GetEventHandler(), event.Clone());
}

void ServiceTestWorker::testService(const wxString& serviceName)
{
    OnServiceTestStarted(serviceName);

    try {
        std::string stdServiceName = serviceName.ToStdString();
        ak::services::TestResult result = ak::services::test_one(config, stdServiceName);

        OnServiceTestCompleted(serviceName, result.ok, result.duration,
                              wxString::FromUTF8(result.error_message));
    } catch (const std::exception& e) {
        OnServiceTestCompleted(serviceName, false, std::chrono::milliseconds(0),
                              wxString::Format("Exception: %s", e.what()));
    }
}

// Event table for ServiceTesterWidget
BEGIN_EVENT_TABLE(ServiceTesterWidget, wxPanel)
    EVT_BUTTON(wxID_ANY, ServiceTesterWidget::OnTestAll)
    EVT_BUTTON(wxID_ANY, ServiceTesterWidget::OnTestSelected)
    EVT_BUTTON(wxID_ANY, ServiceTesterWidget::OnStop)
    EVT_BUTTON(wxID_ANY, ServiceTesterWidget::OnClear)
    EVT_BUTTON(wxID_ANY, ServiceTesterWidget::OnRefresh)
    EVT_BUTTON(wxID_ANY, ServiceTesterWidget::OnExport)
    EVT_LIST_ITEM_SELECTED(wxID_ANY, ServiceTesterWidget::OnServiceSelectionChanged)
    EVT_THREAD(wxID_ANY, ServiceTesterWidget::OnServiceTestStarted)
    EVT_THREAD(wxID_ANY, ServiceTesterWidget::OnServiceTestCompleted)
    EVT_THREAD(wxID_ANY, ServiceTesterWidget::OnAllTestsCompleted)
    EVT_THREAD(wxID_ANY, ServiceTesterWidget::OnTestProgress)
    EVT_TIMER(wxID_ANY, ServiceTesterWidget::OnElapsedTimeUpdate)
END_EVENT_TABLE()

// ServiceTesterWidget Implementation
ServiceTesterWidget::ServiceTesterWidget(const core::Config& config, wxWindow* parent)
    : wxPanel(parent, wxID_ANY), config(config), mainSizer(nullptr), splitter(nullptr),
      serviceList(nullptr), serviceListLabel(nullptr), testAllButton(nullptr),
      testSelectedButton(nullptr), stopButton(nullptr), clearButton(nullptr),
      refreshButton(nullptr), exportButton(nullptr), progressBar(nullptr),
      progressLabel(nullptr), elapsedTimeLabel(nullptr), statusLabel(nullptr),
      resultsText(nullptr), worker(nullptr), elapsedTimer(nullptr),
      testingInProgress(false), totalTests(0), completedTests(0),
      successfulTests(0), failedTests(0)
{
    setupUi();
    loadAvailableServices();
}

ServiceTesterWidget::~ServiceTesterWidget()
{
    if (worker) {
        worker->Delete();
    }
    if (elapsedTimer) {
        elapsedTimer->Stop();
    }
}

void ServiceTesterWidget::setupUi()
{
    mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(mainSizer);

    // Create splitter for main layout
    splitter = new wxSplitterWindow(this, wxID_ANY);

    // Left panel - service list and controls
    wxPanel* leftPanel = new wxPanel(splitter);
    wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);

    setupServiceList();
    setupControls();

    leftSizer->Add(serviceListLabel, 0, wxALL, 5);
    leftSizer->Add(serviceList, 1, wxEXPAND | wxALL, 5);
    leftSizer->Add(setupControls(), 0, wxEXPAND | wxALL, 5);

    leftPanel->SetSizer(leftSizer);

    // Right panel - results
    wxPanel* rightPanel = new wxPanel(splitter);
    wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* resultsLabel = new wxStaticText(rightPanel, wxID_ANY, "Test Results:");
    wxFont font = resultsLabel->GetFont();
    font.SetWeight(wxFONTWEIGHT_BOLD);
    font.SetPointSize(12);
    resultsLabel->SetFont(font);

    resultsText = new wxTextCtrl(rightPanel, wxID_ANY, "No tests run yet.\n\nClick 'Test All' to test all services or select specific services and click 'Test Selected'.",
                                wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
    resultsText->SetFont(wxFont(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

    rightSizer->Add(resultsLabel, 0, wxALL, 5);
    rightSizer->Add(resultsText, 1, wxEXPAND | wxALL, 5);

    rightPanel->SetSizer(rightSizer);

    splitter->SplitVertically(leftPanel, rightPanel);
    splitter->SetMinimumPaneSize(200);
    splitter->SetSashPosition(300);

    mainSizer->Add(splitter, 1, wxEXPAND);

    // Status bar
    wxBoxSizer* statusSizer = new wxBoxSizer(wxHORIZONTAL);
    progressBar = new wxGauge(this, wxID_ANY, 100);
    progressBar->Hide();

    progressLabel = new wxStaticText(this, wxID_ANY, "Ready");
    elapsedTimeLabel = new wxStaticText(this, wxID_ANY, "");
    statusLabel = new wxStaticText(this, wxID_ANY, "");
    statusLabel->SetFont(wxFont(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

    statusSizer->Add(progressLabel, 0, wxALL, 5);
    statusSizer->Add(progressBar, 0, wxALL, 5);
    statusSizer->AddStretchSpacer();
    statusSizer->Add(elapsedTimeLabel, 0, wxALL, 5);
    statusSizer->Add(statusLabel, 0, wxALL, 5);

    mainSizer->Add(statusSizer, 0, wxEXPAND);

    // Setup timer for elapsed time
    elapsedTimer = new wxTimer(this);
}

wxBoxSizer* ServiceTesterWidget::setupControls()
{
    wxBoxSizer* controlsSizer = new wxBoxSizer(wxVERTICAL);

    testAllButton = new wxButton(this, wxID_ANY, "Test All");
    testSelectedButton = new wxButton(this, wxID_ANY, "Test Selected");
    testSelectedButton->Enable(false);
    stopButton = new wxButton(this, wxID_ANY, "Stop");
    stopButton->Enable(false);
    clearButton = new wxButton(this, wxID_ANY, "Clear");
    refreshButton = new wxButton(this, wxID_ANY, "Refresh");
    exportButton = new wxButton(this, wxID_ANY, "Export");

    // Layout buttons in two rows
    wxBoxSizer* topButtonsSizer = new wxBoxSizer(wxHORIZONTAL);
    topButtonsSizer->Add(testAllButton, 0, wxALL, 2);
    topButtonsSizer->Add(testSelectedButton, 0, wxALL, 2);
    topButtonsSizer->Add(stopButton, 0, wxALL, 2);

    wxBoxSizer* bottomButtonsSizer = new wxBoxSizer(wxHORIZONTAL);
    bottomButtonsSizer->Add(clearButton, 0, wxALL, 2);
    bottomButtonsSizer->Add(refreshButton, 0, wxALL, 2);
    bottomButtonsSizer->Add(exportButton, 0, wxALL, 2);

    controlsSizer->Add(topButtonsSizer, 0, wxALIGN_CENTER);
    controlsSizer->Add(bottomButtonsSizer, 0, wxALIGN_CENTER);

    return controlsSizer;
}

void ServiceTesterWidget::setupServiceList()
{
    serviceListLabel = new wxStaticText(this, wxID_ANY, "Available Services:");
    wxFont labelFont = serviceListLabel->GetFont();
    labelFont.SetWeight(wxFONTWEIGHT_BOLD);
    labelFont.SetPointSize(12);
    serviceListLabel->SetFont(labelFont);

    serviceList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_SUNKEN);
    serviceList->InsertColumn(0, "Service", wxLIST_FORMAT_LEFT, 200);
    serviceList->InsertColumn(1, "Status", wxLIST_FORMAT_LEFT, 100);
    serviceList->InsertColumn(2, "Response Time", wxLIST_FORMAT_LEFT, 120);
}

void ServiceTesterWidget::setupResultsPanel()
{
    // This method is not needed in wxWidgets as we set up the results panel in setupUi
}

void ServiceTesterWidget::loadAvailableServices()
{
    try {
        // Get configured services from the updated config detection
        configuredServices.clear();
        auto detectedServices = ak::services::detectConfiguredServices(config);
        for (const auto& service : detectedServices) {
            configuredServices.push_back(wxString::FromUTF8(service));
        }

        // Get all testable services
        availableServices.clear();
        auto testableServices = ak::services::TESTABLE_SERVICES;
        for (const auto& service : testableServices) {
            availableServices.push_back(wxString::FromUTF8(service));
        }

        // Clear and populate list
        serviceList->DeleteAllItems();
        serviceItems.clear();

        // Load vault and profiles to get key source information
        std::vector<wxString> vaultKeys, profileKeys, envKeys;
        try {
            auto vault = ak::storage::loadVault(config);
            for (const auto& [key, value] : vault.kv) {
                if (!value.empty()) {
                    vaultKeys.push_back(wxString::FromUTF8(key));
                }
            }

            auto profiles = ak::storage::listProfiles(config);
            for (const auto& profileName : profiles) {
                auto keys = ak::storage::readProfile(config, profileName);
                for (const auto& key : keys) {
                    profileKeys.push_back(wxString::FromUTF8(key));
                }
            }

            // Check environment variables
            for (const auto& [service, key] : ak::services::SERVICE_KEYS) {
                const char* envValue = getenv(key.c_str());
                if (envValue && *envValue) {
                    envKeys.push_back(wxString::FromUTF8(key));
                }
            }
        } catch (const std::exception& e) {
            // If loading fails, just show basic info
        }

        for (const wxString& serviceName : availableServices) {
            ServiceListItemData itemData(serviceName);

            long itemIndex = serviceList->InsertItem(serviceList->GetItemCount(), serviceName);
            serviceList->SetItem(itemIndex, 1, "Not Tested");
            serviceList->SetItem(itemIndex, 2, "");

            // Enhanced tooltip with key source information
            wxString tooltip = wxString::Format("Service: %s\nStatus: Not Tested", serviceName);

            if (std::find(configuredServices.begin(), configuredServices.end(), serviceName) != configuredServices.end()) {
                // Determine key sources
                std::vector<wxString> sources;
                wxString serviceKey = "";

                // Find the service key name
                auto serviceKeys = ak::services::SERVICE_KEYS;
                for (const auto& [key, value] : serviceKeys) {
                    if (wxString::FromUTF8(key) == serviceName) {
                        serviceKey = wxString::FromUTF8(value);
                        break;
                    }
                }

                if (std::find(vaultKeys.begin(), vaultKeys.end(), serviceKey) != vaultKeys.end()) {
                    sources.push_back("Vault");
                }
                if (std::find(profileKeys.begin(), profileKeys.end(), serviceKey) != profileKeys.end()) {
                    sources.push_back("Profiles");
                }
                if (std::find(envKeys.begin(), envKeys.end(), serviceKey) != envKeys.end()) {
                    sources.push_back("Environment");
                }

                wxFont font = serviceList->GetItemFont(itemIndex);
                font.SetWeight(wxFONTWEIGHT_BOLD);
                serviceList->SetItemFont(itemIndex, font);

                tooltip += "\n✓ Configured";
                if (!sources.empty()) {
                    wxString sourcesStr;
                    for (size_t i = 0; i < sources.size(); ++i) {
                        if (i > 0) sourcesStr += ", ";
                        sourcesStr += sources[i];
                    }
                    tooltip += wxString::Format("\nSources: %s", sourcesStr);
                }

                serviceList->SetItemTextColour(itemIndex, *wxBLACK);
            } else {
                itemData.setTestStatus(ServiceListItemData::Skipped);
                serviceList->SetItem(itemIndex, 1, "Skipped");
                wxFont font = serviceList->GetItemFont(itemIndex);
                font.SetStyle(wxFONTSTYLE_ITALIC);
                serviceList->SetItemFont(itemIndex, font);
                tooltip += "\n⚠ No API key configured";
                serviceList->SetItemTextColour(itemIndex, *wxLIGHT_GREY);
            }

            serviceList->SetItemPtrData(itemIndex, reinterpret_cast<wxUIntPtr>(&itemData));
            serviceItems.push_back(itemData);
        }

        serviceListLabel->SetLabel(wxString::Format("Available Services (%zu configured from vault/profiles/env, %zu total):",
                                                   configuredServices.size(), availableServices.size()));

        updateStatusBar();

    } catch (const std::exception& e) {
        showError(wxString::Format("Failed to load services: %s", e.what()));
    }
}

void ServiceTesterWidget::refreshServices()
{
    loadAvailableServices();
    clearResults();
    showSuccess("Services refreshed");
}

void ServiceTesterWidget::testAllServices()
{
    if (configuredServices.empty()) {
        showError("No services are configured with API keys!");
        return;
    }

    startTestSession();

    // Setup worker thread
    worker = new ServiceTestWorker(config);
    worker->setServices(configuredServices);

    // Connect events
    worker->SetEventHandler(this);

    totalTests = static_cast<int>(configuredServices.size());
    completedTests = 0;
    successfulTests = 0;
    failedTests = 0;

    if (worker->Run() != wxTHREAD_NO_ERROR) {
        showError("Failed to start test worker thread");
        endTestSession();
        return;
    }
}

void ServiceTesterWidget::testSelectedService()
{
    // Implementation for testing selected services
    showError("Test Selected not implemented yet");
}

void ServiceTesterWidget::stopTesting()
{
    if (worker && testingInProgress) {
        worker->Delete();
        worker = nullptr;
        endTestSession();
        showSuccess("Testing stopped");
    }
}

void ServiceTesterWidget::clearResults()
{
    resultsText->Clear();
    resultsText->AppendText("No tests run yet.\n\nClick 'Test All' to test all services or select specific services and click 'Test Selected'.");

    // Reset all service items
    for (size_t i = 0; i < serviceItems.size(); ++i) {
        serviceItems[i].setTestStatus(ServiceListItemData::NotTested);
        serviceItems[i].setResponseTime(std::chrono::milliseconds(0));
        serviceItems[i].setErrorMessage("");

        serviceList->SetItem(i, 1, "Not Tested");
        serviceList->SetItem(i, 2, "");
    }

    updateStatusBar();
}

void ServiceTesterWidget::exportResults()
{
    wxFileDialog saveFileDialog(this, "Export Test Results", "", "",
                               "Text files (*.txt)|*.txt|JSON files (*.json)|*.json",
                               wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (saveFileDialog.ShowModal() == wxID_CANCEL) {
        return;
    }

    wxString fileName = saveFileDialog.GetPath();
    wxString extension = saveFileDialog.GetFilterIndex() == 0 ? ".txt" : ".json";

    if (!fileName.EndsWith(extension)) {
        fileName += extension;
    }

    // For now, just export as text
    wxFile file(fileName, wxFile::write);
    if (!file.IsOpened()) {
        showError("Failed to open file for writing");
        return;
    }

    wxString content = resultsText->GetValue();
    file.Write(content);
    file.Close();

    showSuccess(wxString::Format("Results exported to %s", fileName));
}

void ServiceTesterWidget::onServiceSelectionChanged()
{
    // Update Test Selected button state
    long selected = serviceList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    testSelectedButton->Enable(selected != -1);
}

void ServiceTesterWidget::onServiceTestStarted(const wxString& serviceName)
{
    // Find the service in the list and update its status
    for (size_t i = 0; i < availableServices.size(); ++i) {
        if (availableServices[i] == serviceName) {
            updateServiceItem(i, ServiceListItemData::Testing);
            break;
        }
    }

    resultsText->AppendText(wxString::Format("\nTesting %s...", serviceName));
}

void ServiceTesterWidget::onServiceTestCompleted(const wxString& serviceName, bool success,
                                               std::chrono::milliseconds duration, const wxString& error)
{
    completedTests++;

    if (success) {
        successfulTests++;
        resultsText->AppendText(wxString::Format("\n✓ %s - Success (%lldms)", serviceName, duration.count()));
    } else {
        failedTests++;
        resultsText->AppendText(wxString::Format("\n✗ %s - Failed: %s", serviceName, error));
    }

    // Find the service in the list and update its status
    for (size_t i = 0; i < availableServices.size(); ++i) {
        if (availableServices[i] == serviceName) {
            updateServiceItem(i, success ? ServiceListItemData::Success : ServiceListItemData::Failed,
                            duration, error);
            break;
        }
    }
}

void ServiceTesterWidget::onAllTestsCompleted()
{
    endTestSession();
    showSuccess(wxString::Format("Testing completed: %d successful, %d failed", successfulTests, failedTests));
}

void ServiceTesterWidget::onTestProgress(int current, int total)
{
    if (progressBar->IsShown()) {
        progressBar->SetValue(current);
        progressBar->SetRange(total);
    }
}

void ServiceTesterWidget::updateElapsedTime()
{
    if (testingInProgress) {
        wxTimeSpan elapsed = wxDateTime::Now() - testStartTime;
        elapsedTimeLabel->SetLabel(wxString::Format("Elapsed: %s", elapsed.Format("%H:%M:%S")));
    }
}

void ServiceTesterWidget::updateServiceItem(long itemIndex, ServiceListItemData::TestStatus status,
                                          std::chrono::milliseconds duration, const wxString& error)
{
    if (itemIndex < 0 || static_cast<size_t>(itemIndex) >= serviceItems.size()) {
        return;
    }

    ServiceListItemData& itemData = serviceItems[itemIndex];
    itemData.setTestStatus(status);
    if (duration.count() > 0) {
        itemData.setResponseTime(duration);
    }
    if (!error.IsEmpty()) {
        itemData.setErrorMessage(error);
    }

    wxString statusText = itemData.getStatusText(status);
    wxString responseTimeText = duration.count() > 0 ? wxString::Format("%lldms", duration.count()) : "";

    serviceList->SetItem(itemIndex, 1, statusText);
    serviceList->SetItem(itemIndex, 2, responseTimeText);

    // Update tooltip
    serviceList->SetItemPtrData(itemIndex, reinterpret_cast<wxUIntPtr>(&itemData));
}

void ServiceTesterWidget::resetAllResults()
{
    clearResults();
}

void ServiceTesterWidget::startTestSession()
{
    testingInProgress = true;
    testStartTime = wxDateTime::Now();

    testAllButton->Enable(false);
    testSelectedButton->Enable(false);
    stopButton->Enable(true);
    clearButton->Enable(false);
    refreshButton->Enable(false);
    exportButton->Enable(false);

    progressBar->Show();
    progressBar->SetValue(0);
    progressBar->SetRange(100);

    elapsedTimer->Start(1000); // Update every second

    resultsText->Clear();
    resultsText->AppendText("Starting service tests...\n");

    updateStatusBar();
}

void ServiceTesterWidget::endTestSession()
{
    testingInProgress = false;

    testAllButton->Enable(true);
    testSelectedButton->Enable(false);
    stopButton->Enable(false);
    clearButton->Enable(true);
    refreshButton->Enable(true);
    exportButton->Enable(true);

    progressBar->Hide();
    elapsedTimer->Stop();
    elapsedTimeLabel->SetLabel("");

    updateStatusBar();
}

void ServiceTesterWidget::updateStatusBar()
{
    if (testingInProgress) {
        progressLabel->SetLabel(wxString::Format("Testing... (%d/%d)", completedTests, totalTests));
    } else {
        progressLabel->SetLabel(wxString::Format("Ready - %zu services available", availableServices.size()));
    }

    statusLabel->SetLabel(wxString::Format("Total: %d, Successful: %d, Failed: %d",
                                          totalTests, successfulTests, failedTests));
}

void ServiceTesterWidget::showError(const wxString& message)
{
    wxMessageBox(message, "Error", wxOK | wxICON_ERROR, this);
    statusLabel->SetLabel("Error: " + message);
}

void ServiceTesterWidget::showSuccess(const wxString& message)
{
    statusLabel->SetLabel("Success: " + message);
}

// Event handler implementations
void ServiceTesterWidget::OnTestAll(wxCommandEvent& event)
{
    testAllServices();
}

void ServiceTesterWidget::OnTestSelected(wxCommandEvent& event)
{
    testSelectedService();
}

void ServiceTesterWidget::OnStop(wxCommandEvent& event)
{
    stopTesting();
}

void ServiceTesterWidget::OnClear(wxCommandEvent& event)
{
    clearResults();
}

void ServiceTesterWidget::OnRefresh(wxCommandEvent& event)
{
    refreshServices();
}

void ServiceTesterWidget::OnExport(wxCommandEvent& event)
{
    exportResults();
}

void ServiceTesterWidget::OnServiceSelectionChanged(wxListEvent& event)
{
    onServiceSelectionChanged();
}

void ServiceTesterWidget::OnServiceTestStarted(wxThreadEvent& event)
{
    onServiceTestStarted(event.GetString());
}

void ServiceTesterWidget::OnServiceTestCompleted(wxThreadEvent& event)
{
    wxString serviceName = event.GetString();
    bool success = event.GetInt() != 0;
    std::chrono::milliseconds duration(event.GetExtraLong());
    // Note: Error message would need to be passed differently in wxThreadEvent
    onServiceTestCompleted(serviceName, success, duration, "");
}

void ServiceTesterWidget::OnAllTestsCompleted(wxThreadEvent& event)
{
    onAllTestsCompleted();
}

void ServiceTesterWidget::OnTestProgress(wxThreadEvent& event)
{
    onTestProgress(event.GetInt(), static_cast<int>(event.GetExtraLong()));
}

void ServiceTesterWidget::OnElapsedTimeUpdate(wxTimerEvent& event)
{
    updateElapsedTime();
}

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI