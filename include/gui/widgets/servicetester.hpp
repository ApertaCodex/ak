#pragma once

#ifdef BUILD_GUI

#include "core/config.hpp"
#include "services/services.hpp"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QIcon>
#include <QDateTime>
#include <chrono>

namespace ak {
namespace gui {
namespace widgets {

// Forward declarations
class ServiceTestWorker;

// Custom list item for service display
class ServiceListItem : public QListWidgetItem
{
public:
    enum TestStatus {
        NotTested,
        Testing,
        Success,
        Failed,
        Skipped
    };
    
    explicit ServiceListItem(const QString &serviceName, QListWidget *parent = nullptr);
    
    void setTestStatus(TestStatus status);
    TestStatus getTestStatus() const;
    
    void setResponseTime(std::chrono::milliseconds duration);
    void setErrorMessage(const QString &error);
    
    QString getServiceName() const;
    QString getErrorMessage() const;
    std::chrono::milliseconds getResponseTime() const;

private:
    void updateDisplay();
    QIcon getStatusIcon(TestStatus status) const;
public:
    QString getStatusText(TestStatus status) const;
    
private:
    QString serviceName;
    TestStatus status;
    std::chrono::milliseconds responseTime;
    QString errorMessage;
};

// Worker thread for service testing
class ServiceTestWorker : public QObject
{
    Q_OBJECT

public:
    explicit ServiceTestWorker(const core::Config& config, QObject *parent = nullptr);
    
    void setServices(const QStringList &services);
    void setFailFast(bool failFast);

public slots:
    void testAllServices();
    void testSingleService(const QString &serviceName);

signals:
    void serviceTestStarted(const QString &serviceName);
    void serviceTestCompleted(const QString &serviceName, bool success, 
                             std::chrono::milliseconds duration, const QString &error);
    void allTestsCompleted();
    void progress(int current, int total);

private:
    void testService(const QString &serviceName);
    
    const core::Config& config;
    QStringList servicesToTest;
    bool failFast;
    QMutex mutex;
};

// Service Tester Widget
class ServiceTesterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ServiceTesterWidget(const core::Config& config, QWidget *parent = nullptr);
    ~ServiceTesterWidget();
    
    void refreshServices();

signals:
    void statusMessage(const QString &message);

private slots:
    void testAllServices();
    void testSelectedService();
    void stopTesting();
    void clearResults();
    void exportResults();
    void onServiceSelectionChanged();
    void onServiceTestStarted(const QString &serviceName);
    void onServiceTestCompleted(const QString &serviceName, bool success,
                               std::chrono::milliseconds duration, const QString &error);
    void onAllTestsCompleted();
    void onTestProgress(int current, int total);
    void updateElapsedTime();

private:
    void setupUi();
    void setupServiceList();
    void setupControls();
    void setupResultsPanel();
    void loadAvailableServices();
    void updateServiceItem(const QString &serviceName, ServiceListItem::TestStatus status,
                          std::chrono::milliseconds duration = std::chrono::milliseconds(0),
                          const QString &error = QString());
    void resetAllResults();
    void startTestSession();
    void endTestSession();
    void updateStatusBar();
    void showError(const QString &message);
    void showSuccess(const QString &message);
    
    // Configuration and data
    const core::Config& config;
    QStringList availableServices;
    QStringList configuredServices;
    
    // UI components
    QVBoxLayout *mainLayout;
    QHBoxLayout *controlsLayout;
    QHBoxLayout *topLayout;
    
    // Service list
    QListWidget *serviceList;
    QLabel *serviceListLabel;
    
    // Controls
    QPushButton *testAllButton;
    QPushButton *testSelectedButton;
    QPushButton *stopButton;
    QPushButton *clearButton;
    QPushButton *refreshButton;
    QPushButton *exportButton;
    
    // Progress and status
    QProgressBar *progressBar;
    QLabel *progressLabel;
    QLabel *elapsedTimeLabel;
    QLabel *statusLabel;
    
    // Results panel
    QTextEdit *resultsText;
    
    // Testing infrastructure
    QThread *workerThread;
    ServiceTestWorker *worker;
    QTimer *elapsedTimer;
    
    // Test session state
    bool testingInProgress;
    QDateTime testStartTime;
    int totalTests;
    int completedTests;
    int successfulTests;
    int failedTests;
};

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI