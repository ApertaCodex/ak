#ifdef BUILD_GUI

#include "gui/widgets/servicetester.hpp"
#include "gui/widgets/common/dialogs.hpp"
#include "services/services.hpp"
#include "storage/vault.hpp"
#include "core/config.hpp"
#include <QApplication>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QSplitter>
#include <QGroupBox>
#include <QScrollArea>
#include <QMessageBox>
#include <algorithm>

namespace ak {
namespace gui {
namespace widgets {

// ServiceListItem Implementation
ServiceListItem::ServiceListItem(const QString &serviceName, QListWidget *parent)
    : QListWidgetItem(parent), serviceName(serviceName), status(NotTested),
      responseTime(0), errorMessage()
{
    updateDisplay();
}

void ServiceListItem::setTestStatus(TestStatus status)
{
    this->status = status;
    updateDisplay();
}

ServiceListItem::TestStatus ServiceListItem::getTestStatus() const
{
    return status;
}

void ServiceListItem::setResponseTime(std::chrono::milliseconds duration)
{
    this->responseTime = duration;
    updateDisplay();
}

void ServiceListItem::setErrorMessage(const QString &error)
{
    this->errorMessage = error;
    updateDisplay();
}

QString ServiceListItem::getServiceName() const
{
    return serviceName;
}

QString ServiceListItem::getErrorMessage() const
{
    return errorMessage;
}

std::chrono::milliseconds ServiceListItem::getResponseTime() const
{
    return responseTime;
}

void ServiceListItem::updateDisplay()
{
    QString displayText = serviceName;

    if (status == Testing) {
        displayText += " (Testing...)";
    } else if (status == Success) {
        displayText += QString(" (✓ %1ms)").arg(responseTime.count());
    } else if (status == Failed) {
        displayText += " (✗ Failed)";
    } else if (status == Skipped) {
        displayText += " (- Skipped)";
    }

    setText(displayText);
    setIcon(getStatusIcon(status));

    // Set tooltip with detailed information
    QString tooltip = QString("Service: %1\nStatus: %2")
                      .arg(serviceName)
                      .arg(getStatusText(status));

    if (status == Success && responseTime.count() > 0) {
        tooltip += QString("\nResponse Time: %1ms").arg(responseTime.count());
    }

    if (status == Failed && !errorMessage.isEmpty()) {
        tooltip += QString("\nError: %1").arg(errorMessage);
    }

    setToolTip(tooltip);
}

QIcon ServiceListItem::getStatusIcon(TestStatus status) const
{
    switch (status) {
    case NotTested:
        return QApplication::style()->standardIcon(QStyle::SP_DialogHelpButton);
    case Testing:
        return QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    case Success:
        return QApplication::style()->standardIcon(QStyle::SP_DialogApplyButton);
    case Failed:
        return QApplication::style()->standardIcon(QStyle::SP_DialogCancelButton);
    case Skipped:
        return QApplication::style()->standardIcon(QStyle::SP_DialogDiscardButton);
    default:
        return QIcon();
    }
}

QString ServiceListItem::getStatusText(TestStatus status) const
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

// ServiceTestWorker Implementation
ServiceTestWorker::ServiceTestWorker(const core::Config& config, QObject *parent)
    : QObject(parent), config(config), failFast(false)
{
}

void ServiceTestWorker::setServices(const QStringList &services)
{
    QMutexLocker locker(&mutex);
    servicesToTest = services;
}

void ServiceTestWorker::setFailFast(bool failFast)
{
    QMutexLocker locker(&mutex);
    this->failFast = failFast;
}

void ServiceTestWorker::testAllServices()
{
    QMutexLocker locker(&mutex);
    QStringList services = servicesToTest;
    locker.unlock();

    int current = 0;
    int total = services.size();

    for (const QString& serviceName : services) {
        emit progress(current, total);
        testService(serviceName);
        current++;

        // Check if we should fail fast
        locker.relock();
        if (failFast) {
            // For now, we don't implement early termination logic
        }
        locker.unlock();
    }

    emit progress(total, total);
    emit allTestsCompleted();
}

void ServiceTestWorker::testSingleService(const QString &serviceName)
{
    testService(serviceName);
    emit allTestsCompleted();
}

void ServiceTestWorker::testService(const QString &serviceName)
{
    emit serviceTestStarted(serviceName);

    try {
        std::string stdServiceName = serviceName.toStdString();
        ak::services::TestResult result = ak::services::test_one(config, stdServiceName);

        emit serviceTestCompleted(serviceName, result.ok, result.duration,
                                QString::fromStdString(result.error_message));
    } catch (const std::exception& e) {
        emit serviceTestCompleted(serviceName, false, std::chrono::milliseconds(0),
                                QString("Exception: %1").arg(e.what()));
    }
}

// ServiceTesterWidget Implementation
ServiceTesterWidget::ServiceTesterWidget(const core::Config& config, QWidget *parent)
    : QWidget(parent), config(config), mainLayout(nullptr), controlsLayout(nullptr),
      topLayout(nullptr), serviceList(nullptr), serviceListLabel(nullptr),
      testAllButton(nullptr), testSelectedButton(nullptr), stopButton(nullptr),
      clearButton(nullptr), refreshButton(nullptr), exportButton(nullptr),
      progressBar(nullptr), progressLabel(nullptr), elapsedTimeLabel(nullptr),
      statusLabel(nullptr), resultsText(nullptr), workerThread(nullptr),
      worker(nullptr), elapsedTimer(nullptr), testingInProgress(false),
      totalTests(0), completedTests(0), successfulTests(0), failedTests(0)
{
    setupUi();
    loadAvailableServices();
}

ServiceTesterWidget::~ServiceTesterWidget()
{
    if (workerThread) {
        workerThread->quit();
        workerThread->wait();
    }
}

void ServiceTesterWidget::setupUi()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Create splitter for main layout
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // Left panel - service list and controls
    QWidget *leftPanel = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);

    setupServiceList();
    setupControls();

    leftLayout->addWidget(serviceListLabel);
    leftLayout->addWidget(serviceList);
    leftLayout->addLayout(controlsLayout);

    // Right panel - results
    QWidget *rightPanel = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);

    QLabel *resultsLabel = new QLabel("Test Results:", this);
    resultsLabel->setStyleSheet("font-weight: bold; font-size: 14px;");

    resultsText = new QTextEdit(this);
    resultsText->setReadOnly(true);
    resultsText->setFont(QFont("Consolas", 10));
    resultsText->setPlainText("No tests run yet.\n\nClick 'Test All' to test all services or select specific services and click 'Test Selected'.");

    rightLayout->addWidget(resultsLabel);
    rightLayout->addWidget(resultsText);

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setSizes({300, 500});

    mainLayout->addWidget(splitter);

    // Status bar
    QHBoxLayout *statusLayout = new QHBoxLayout();
    progressBar = new QProgressBar(this);
    progressBar->setVisible(false);

    progressLabel = new QLabel("Ready", this);
    elapsedTimeLabel = new QLabel("", this);
    statusLabel = new QLabel("", this);
    statusLabel->setStyleSheet("color: #666; font-size: 12px;");

    statusLayout->addWidget(progressLabel);
    statusLayout->addWidget(progressBar);
    statusLayout->addStretch();
    statusLayout->addWidget(elapsedTimeLabel);
    statusLayout->addWidget(statusLabel);

    mainLayout->addLayout(statusLayout);

    // Setup timer for elapsed time
    elapsedTimer = new QTimer(this);
    connect(elapsedTimer, &QTimer::timeout, this, &ServiceTesterWidget::updateElapsedTime);
}

void ServiceTesterWidget::setupServiceList()
{
    serviceListLabel = new QLabel("Available Services:", this);
    serviceListLabel->setStyleSheet("font-weight: bold; font-size: 14px;");

    serviceList = new QListWidget(this);
    serviceList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    serviceList->setSortingEnabled(true);

    connect(serviceList, &QListWidget::itemSelectionChanged,
            this, &ServiceTesterWidget::onServiceSelectionChanged);
}

void ServiceTesterWidget::setupControls()
{
    controlsLayout = new QHBoxLayout();

    testAllButton = new QPushButton("Test All", this);
    testAllButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
    connect(testAllButton, &QPushButton::clicked, this, &ServiceTesterWidget::testAllServices);

    testSelectedButton = new QPushButton("Test Selected", this);
    testSelectedButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaSeekForward));
    testSelectedButton->setEnabled(false);
    connect(testSelectedButton, &QPushButton::clicked, this, &ServiceTesterWidget::testSelectedService);

    stopButton = new QPushButton("Stop", this);
    stopButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaStop));
    stopButton->setEnabled(false);
    connect(stopButton, &QPushButton::clicked, this, &ServiceTesterWidget::stopTesting);

    clearButton = new QPushButton("Clear", this);
    clearButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogResetButton));
    connect(clearButton, &QPushButton::clicked, this, &ServiceTesterWidget::clearResults);

    refreshButton = new QPushButton("Refresh", this);
    refreshButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserReload));
    connect(refreshButton, &QPushButton::clicked, this, &ServiceTesterWidget::refreshServices);

    exportButton = new QPushButton("Export", this);
    exportButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton));
    connect(exportButton, &QPushButton::clicked, this, &ServiceTesterWidget::exportResults);

    // Layout buttons in two rows
    QVBoxLayout *buttonsLayout = new QVBoxLayout();

    QHBoxLayout *topButtons = new QHBoxLayout();
    topButtons->addWidget(testAllButton);
    topButtons->addWidget(testSelectedButton);
    topButtons->addWidget(stopButton);

    QHBoxLayout *bottomButtons = new QHBoxLayout();
    bottomButtons->addWidget(clearButton);
    bottomButtons->addWidget(refreshButton);
    bottomButtons->addWidget(exportButton);

    buttonsLayout->addLayout(topButtons);
    buttonsLayout->addLayout(bottomButtons);

    controlsLayout->addLayout(buttonsLayout);
}

void ServiceTesterWidget::loadAvailableServices()
{
    try {
        // Get configured services from the updated config detection
        configuredServices.clear();
        auto detectedServices = ak::services::detectConfiguredServices(config, ak::storage::getDefaultProfileName());
        for (const auto& service : detectedServices) {
            configuredServices << QString::fromStdString(service);
        }

        // Get all testable services
        availableServices.clear();
        auto testableServices = ak::services::TESTABLE_SERVICES;
        for (const auto& service : testableServices) {
            availableServices << QString::fromStdString(service);
        }

        // Clear and populate list with enhanced key source detection
        serviceList->clear();
        
        // Load vault and profiles to get key source information
        QStringList vaultKeys, profileKeys, envKeys;
        try {
            auto vault = ak::storage::loadVault(config);
            for (const auto& [key, value] : vault.kv) {
                if (!value.empty()) {
                    vaultKeys << QString::fromStdString(key);
                }
            }
            
            auto profiles = ak::storage::listProfiles(config);
            for (const auto& profileName : profiles) {
                auto keys = ak::storage::readProfile(config, profileName);
                for (const auto& key : keys) {
                    profileKeys << QString::fromStdString(key);
                }
            }
            
            // Check environment variables
            for (const auto& [service, key] : ak::services::SERVICE_KEYS) {
                const char* envValue = getenv(key.c_str());
                if (envValue && *envValue) {
                    envKeys << QString::fromStdString(key);
                }
            }
        } catch (const std::exception& e) {
            // If loading fails, just show basic info
        }

        for (const QString& serviceName : availableServices) {
            ServiceListItem *item = new ServiceListItem(serviceName, serviceList);
            
            // Enhanced tooltip with key source information
            QString tooltip = QString("Service: %1\nStatus: Not Tested").arg(serviceName);
            
            if (configuredServices.contains(serviceName)) {
                // Determine key sources
                QStringList sources;
                QString serviceKey = "";
                
                // Find the service key name
                auto serviceKeys = ak::services::SERVICE_KEYS;
                for (const auto& [key, value] : serviceKeys) {
                    if (QString::fromStdString(key) == serviceName) {
                        serviceKey = QString::fromStdString(value);
                        break;
                    }
                }
                
                if (vaultKeys.contains(serviceKey)) {
                    sources << "Vault";
                }
                if (profileKeys.contains(serviceKey)) {
                    sources << "Profiles";
                }
                if (envKeys.contains(serviceKey)) {
                    sources << "Environment";
                }
                
                QFont font = item->font();
                font.setBold(true);
                item->setFont(font);
                
                tooltip += "\n✓ Configured";
                if (!sources.isEmpty()) {
                    tooltip += QString("\nSources: %1").arg(sources.join(", "));
                }
                
                item->setToolTip(tooltip);
            } else {
                item->setTestStatus(ServiceListItem::Skipped);
                QFont font = item->font();
                font.setItalic(true);
                item->setFont(font);
                tooltip += "\n⚠ No API key configured";
                item->setToolTip(tooltip);
            }
        }

        serviceListLabel->setText(QString("Available Services (%1 configured from vault/profiles/env, %2 total):")
                                  .arg(configuredServices.size())
                                  .arg(availableServices.size()));

        updateStatusBar();

    } catch (const std::exception& e) {
        showError(QString("Failed to load services: %1").arg(e.what()));
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
    if (configuredServices.isEmpty()) {
        showError("No services are configured with API keys!");
        return;
    }

    startTestSession();

    // Setup worker thread
    workerThread = new QThread(this);
    worker = new ServiceTestWorker(config);
    worker->moveToThread(workerThread);
    worker->setServices(configuredServices);

    // Connect signals
    connect(worker, &ServiceTestWorker::serviceTestStarted,
            this, &ServiceTesterWidget::onServiceTestStarted);
    connect(worker, &ServiceTestWorker::serviceTestCompleted,
            this, &ServiceTesterWidget::onServiceTestCompleted);
    connect(worker, &ServiceTestWorker::allTestsCompleted,
            this, &ServiceTesterWidget::onAllTestsCompleted);
    connect(worker, &ServiceTestWorker::progress,
            this, &ServiceTesterWidget::onTestProgress);

    connect(workerThread, &QThread::started, worker, &ServiceTestWorker::testAllServices);
    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);

    totalTests = configuredServices.size();
    completedTests = 0;
    successfulTests = 0;
    failedTests = 0;

    progressBar->setMaximum(totalTests);
    progressBar->setValue(0);

    workerThread->start();
}

void ServiceTesterWidget::testSelectedService()
{
    QList<QListWidgetItem*> selectedItems = serviceList->selectedItems();
    if (selectedItems.isEmpty()) {
        showError("No services selected!");
        return;
    }

    QStringList servicesToTest;
    for (QListWidgetItem* item : selectedItems) {
        ServiceListItem* serviceItem = static_cast<ServiceListItem*>(item);
        QString serviceName = serviceItem->getServiceName();

        // Only test configured services
        if (configuredServices.contains(serviceName)) {
            servicesToTest << serviceName;
        }
    }

    if (servicesToTest.isEmpty()) {
        showError("Selected services are not configured with API keys!");
        return;
    }

    startTestSession();

    // Setup worker thread
    workerThread = new QThread(this);
    worker = new ServiceTestWorker(config);
    worker->moveToThread(workerThread);
    worker->setServices(servicesToTest);

    // Connect signals
    connect(worker, &ServiceTestWorker::serviceTestStarted,
            this, &ServiceTesterWidget::onServiceTestStarted);
    connect(worker, &ServiceTestWorker::serviceTestCompleted,
            this, &ServiceTesterWidget::onServiceTestCompleted);
    connect(worker, &ServiceTestWorker::allTestsCompleted,
            this, &ServiceTesterWidget::onAllTestsCompleted);
    connect(worker, &ServiceTestWorker::progress,
            this, &ServiceTesterWidget::onTestProgress);

    connect(workerThread, &QThread::started, worker, &ServiceTestWorker::testAllServices);
    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);

    totalTests = servicesToTest.size();
    completedTests = 0;
    successfulTests = 0;
    failedTests = 0;

    progressBar->setMaximum(totalTests);
    progressBar->setValue(0);

    workerThread->start();
}

void ServiceTesterWidget::stopTesting()
{
    if (workerThread && workerThread->isRunning()) {
        workerThread->requestInterruption();
        workerThread->quit();
        workerThread->wait(5000); // Wait up to 5 seconds

        if (workerThread->isRunning()) {
            workerThread->terminate();
            workerThread->wait();
        }

        endTestSession();
        showError("Testing stopped by user");
    }
}

void ServiceTesterWidget::clearResults()
{
    resetAllResults();
    resultsText->setPlainText("Results cleared.\n\nClick 'Test All' to test all services or select specific services and click 'Test Selected'.");
    showSuccess("Results cleared");
}

void ServiceTesterWidget::exportResults()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Export Test Results",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/service_test_results.json",
        "JSON Files (*.json);;Text Files (*.txt)");

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        showError("Failed to open file for writing");
        return;
    }

    if (fileName.endsWith(".json")) {
        // Export as JSON
        QJsonObject root;
        root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        root["total_tests"] = totalTests;
        root["successful_tests"] = successfulTests;
        root["failed_tests"] = failedTests;

        QJsonArray services;
        for (int i = 0; i < serviceList->count(); ++i) {
            ServiceListItem* item = static_cast<ServiceListItem*>(serviceList->item(i));
            if (item->getTestStatus() != ServiceListItem::NotTested) {
                QJsonObject service;
                service["name"] = item->getServiceName();
                service["status"] = item->getStatusText(item->getTestStatus());
                service["response_time_ms"] = static_cast<qint64>(item->getResponseTime().count());
                service["error_message"] = item->getErrorMessage();
                services.append(service);
            }
        }
        root["services"] = services;

        QJsonDocument doc(root);
        file.write(doc.toJson());
    } else {
        // Export as plain text
        QTextStream stream(&file);
        stream << "Service Test Results\n";
        stream << "Generated: " << QDateTime::currentDateTime().toString() << "\n";
        stream << "===========================================\n\n";

        stream << resultsText->toPlainText();
    }

    file.close();
    showSuccess(QString("Results exported to %1").arg(fileName));
}

void ServiceTesterWidget::onServiceSelectionChanged()
{
    bool hasSelection = !serviceList->selectedItems().isEmpty();
    testSelectedButton->setEnabled(hasSelection && !testingInProgress);
}

void ServiceTesterWidget::onServiceTestStarted(const QString &serviceName)
{
    updateServiceItem(serviceName, ServiceListItem::Testing);

    QString message = QString("Testing %1...").arg(serviceName);
    progressLabel->setText(message);
    resultsText->append(QString("[%1] %2")
                       .arg(QTime::currentTime().toString("hh:mm:ss"))
                       .arg(message));
}

void ServiceTesterWidget::onServiceTestCompleted(const QString &serviceName, bool success,
                                                std::chrono::milliseconds duration, const QString &error)
{
    completedTests++;

    if (success) {
        successfulTests++;
        updateServiceItem(serviceName, ServiceListItem::Success, duration);

        QString message = QString("✓ %1 - Success (%2ms)")
                         .arg(serviceName)
                         .arg(duration.count());
        resultsText->append(QString("[%1] %2")
                           .arg(QTime::currentTime().toString("hh:mm:ss"))
                           .arg(message));
    } else {
        failedTests++;
        updateServiceItem(serviceName, ServiceListItem::Failed, duration, error);

        QString message = QString("✗ %1 - Failed: %2")
                         .arg(serviceName)
                         .arg(error.isEmpty() ? "Unknown error" : error);
        resultsText->append(QString("[%1] %2")
                           .arg(QTime::currentTime().toString("hh:mm:ss"))
                           .arg(message));
    }

    // Auto-scroll to bottom
    QTextCursor cursor = resultsText->textCursor();
    cursor.movePosition(QTextCursor::End);
    resultsText->setTextCursor(cursor);

    updateStatusBar();
}

void ServiceTesterWidget::onAllTestsCompleted()
{
    endTestSession();

    QString summary = QString("\n=== Test Summary ===\n"
                             "Total: %1, Successful: %2, Failed: %3")
                     .arg(completedTests)
                     .arg(successfulTests)
                     .arg(failedTests);

    resultsText->append(summary);

    if (failedTests == 0) {
        showSuccess(QString("All tests completed successfully! (%1/%2)")
                   .arg(successfulTests).arg(totalTests));
    } else {
        showError(QString("Tests completed with %1 failures out of %2 tests")
                 .arg(failedTests).arg(totalTests));
    }
}

void ServiceTesterWidget::onTestProgress(int current, int total)
{
    progressBar->setValue(current);
    progressBar->setMaximum(total);
    updateStatusBar();
}

void ServiceTesterWidget::updateElapsedTime()
{
    if (testingInProgress && testStartTime.isValid()) {
        qint64 elapsed = testStartTime.msecsTo(QDateTime::currentDateTime());
        int seconds = elapsed / 1000;
        int minutes = seconds / 60;
        seconds = seconds % 60;

        elapsedTimeLabel->setText(QString("Elapsed: %1:%2")
                                 .arg(minutes, 2, 10, QChar('0'))
                                 .arg(seconds, 2, 10, QChar('0')));
    }
}

void ServiceTesterWidget::updateServiceItem(const QString &serviceName, ServiceListItem::TestStatus status,
                                          std::chrono::milliseconds duration, const QString &error)
{
    for (int i = 0; i < serviceList->count(); ++i) {
        ServiceListItem* item = static_cast<ServiceListItem*>(serviceList->item(i));
        if (item->getServiceName() == serviceName) {
            item->setTestStatus(status);
            if (duration.count() > 0) {
                item->setResponseTime(duration);
            }
            if (!error.isEmpty()) {
                item->setErrorMessage(error);
            }
            break;
        }
    }
}

void ServiceTesterWidget::resetAllResults()
{
    for (int i = 0; i < serviceList->count(); ++i) {
        ServiceListItem* item = static_cast<ServiceListItem*>(serviceList->item(i));

        if (configuredServices.contains(item->getServiceName())) {
            item->setTestStatus(ServiceListItem::NotTested);
        } else {
            item->setTestStatus(ServiceListItem::Skipped);
        }

        item->setResponseTime(std::chrono::milliseconds(0));
        item->setErrorMessage("");
    }

    totalTests = 0;
    completedTests = 0;
    successfulTests = 0;
    failedTests = 0;

    updateStatusBar();
}

void ServiceTesterWidget::startTestSession()
{
    testingInProgress = true;
    testStartTime = QDateTime::currentDateTime();

    testAllButton->setEnabled(false);
    testSelectedButton->setEnabled(false);
    stopButton->setEnabled(true);
    clearButton->setEnabled(false);
    refreshButton->setEnabled(false);

    progressBar->setVisible(true);
    progressLabel->setText("Starting tests...");
    elapsedTimeLabel->setText("Elapsed: 0:00");

    elapsedTimer->start(1000); // Update every second

    resultsText->append(QString("\n=== Test Session Started at %1 ===")
                       .arg(testStartTime.toString("yyyy-MM-dd hh:mm:ss")));
}

void ServiceTesterWidget::endTestSession()
{
    testingInProgress = false;

    testAllButton->setEnabled(true);
    stopButton->setEnabled(false);
    clearButton->setEnabled(true);
    refreshButton->setEnabled(true);

    progressBar->setVisible(false);
    progressLabel->setText("Ready");

    elapsedTimer->stop();

    onServiceSelectionChanged(); // Update testSelectedButton state

    // Clean up worker thread
    if (workerThread) {
        workerThread->quit();
        workerThread->wait();
        workerThread->deleteLater();
        workerThread = nullptr;
        worker = nullptr;
    }
}

void ServiceTesterWidget::updateStatusBar()
{
    if (testingInProgress) {
        statusLabel->setText(QString("Testing... (%1/%2 completed, %3 successful, %4 failed)")
                           .arg(completedTests).arg(totalTests)
                           .arg(successfulTests).arg(failedTests));
    } else {
        statusLabel->setText(QString("%1 services configured").arg(configuredServices.size()));
    }
}

void ServiceTesterWidget::showError(const QString &message)
{
    statusLabel->setText(QString("Error: %1").arg(message));
    statusLabel->setStyleSheet("color: #ff4444; font-size: 12px;");

    // Reset after 5 seconds
    QTimer::singleShot(5000, [this]() {
        statusLabel->setText("Ready");
        statusLabel->setStyleSheet("color: #666; font-size: 12px;");
    });

    emit statusMessage(QString("Error: %1").arg(message));
}

void ServiceTesterWidget::showSuccess(const QString &message)
{
    statusLabel->setText(message);
    statusLabel->setStyleSheet("color: #00aa00; font-size: 12px;");

    // Reset after 3 seconds
    QTimer::singleShot(3000, [this]() {
        statusLabel->setText("Ready");
        statusLabel->setStyleSheet("color: #666; font-size: 12px;");
    });

    emit statusMessage(message);
}

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI