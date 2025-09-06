#ifdef BUILD_GUI

#include "gui/mainwindow.hpp"
#include <QApplication>
#include <QMessageBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QStatusBar>

namespace ak {
namespace gui {

MainWindow::MainWindow(const core::Config& cfg, QWidget *parent)
    : QMainWindow(parent), config(cfg), tabWidget(nullptr),
      keyManagerWidget(nullptr), serviceTesterWidget(nullptr),
      profileManagerWidget(nullptr), settingsTab(nullptr),
      exitAction(nullptr), aboutAction(nullptr), helpAction(nullptr)
{
    setupUi();
    setupMenuBar();
    setupStatusBar();
    setupTabs();

    // Set window properties
    setWindowTitle("AK - API Key Manager");
    setMinimumSize(800, 600);
    resize(1000, 700);

    // Center window on screen
    QRect screenGeometry = QApplication::primaryScreen()->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);
}

MainWindow::~MainWindow()
{
    // Qt handles cleanup automatically for child widgets
}

void MainWindow::setupUi()
{
    // Create central widget and main layout
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Create tab widget
    tabWidget = new QTabWidget(this);
    mainLayout->addWidget(tabWidget);
}

void MainWindow::setupMenuBar()
{
    // File menu
    QMenu *fileMenu = menuBar()->addMenu("&File");
    
    exitAction = new QAction("E&xit", this);
    exitAction->setShortcut(QKeySequence::Quit);
    exitAction->setStatusTip("Exit the application");
    connect(exitAction, &QAction::triggered, this, &MainWindow::exitApplication);
    fileMenu->addAction(exitAction);
    
    // Help menu
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    
    helpAction = new QAction("&Help", this);
    helpAction->setShortcut(QKeySequence::HelpContents);
    helpAction->setStatusTip("Show help documentation");
    connect(helpAction, &QAction::triggered, this, &MainWindow::showHelp);
    helpMenu->addAction(helpAction);
    
    helpMenu->addSeparator();
    
    aboutAction = new QAction("&About AK", this);
    aboutAction->setStatusTip("Show information about AK");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    helpMenu->addAction(aboutAction);
}

void MainWindow::setupStatusBar()
{
    // Create status bar with basic info
    statusBar()->showMessage("Ready", 2000);
    
    // Add permanent widgets to status bar
    QLabel *backendLabel = new QLabel(config.gpgAvailable ? "Backend: GPG" : "Backend: Plain");
    statusBar()->addPermanentWidget(backendLabel);
    
    QLabel *versionLabel = new QLabel("v" AK_VERSION_STRING);
    statusBar()->addPermanentWidget(versionLabel);
}

void MainWindow::setupTabs()
{
    // Key Manager Tab
    keyManagerWidget = new widgets::KeyManagerWidget(config, this);
    connect(keyManagerWidget, &widgets::KeyManagerWidget::statusMessage,
            this, &MainWindow::onStatusMessage);
    tabWidget->addTab(keyManagerWidget, "Key Manager");

    // Service Tester Tab
    serviceTesterWidget = new widgets::ServiceTesterWidget(config, this);
    connect(serviceTesterWidget, &widgets::ServiceTesterWidget::statusMessage,
            this, &MainWindow::onStatusMessage);
    tabWidget->addTab(serviceTesterWidget, "Service Tester");

    // Profile Manager Tab
    profileManagerWidget = new widgets::ProfileManagerWidget(config, this);
    connect(profileManagerWidget, &widgets::ProfileManagerWidget::statusMessage,
            this, &MainWindow::onStatusMessage);
    tabWidget->addTab(profileManagerWidget, "Profile Manager");

    // Settings Tab (placeholder for now)
    settingsTab = new QWidget();
    QVBoxLayout *settingsLayout = new QVBoxLayout(settingsTab);
    QLabel *settingsLabel = new QLabel("Settings functionality will be implemented in Phase 3");
    settingsLabel->setAlignment(Qt::AlignCenter);
    settingsLabel->setStyleSheet("color: #666; font-size: 14px; font-style: italic;");
    settingsLayout->addWidget(settingsLabel);
    tabWidget->addTab(settingsTab, "Settings");

    // Set default tab
    tabWidget->setCurrentIndex(0);
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, "About AK",
        "AK - API Key Manager v" AK_VERSION_STRING "\n\n"
        "A secure tool for managing API keys and environment variables.\n\n"
        "Backend: " + QString(config.gpgAvailable ? "GPG Encryption" : "Plain Text") + "\n"
        "Config Directory: " + QString::fromStdString(config.configDir));
}

void MainWindow::showHelp()
{
    QMessageBox::information(this, "Help",
        "AK GUI Help\n\n"
        "Use the tabs to navigate between different features:\n\n"
        "• Key Manager: Manage your API keys and secrets\n"
        "• Service Tester: Test API endpoints with your keys\n"
        "• Profile Manager: Create and manage key profiles\n"
        "• Settings: Configure application preferences\n\n"
        "For detailed documentation, visit the project repository.");
}

void MainWindow::exitApplication()
{
    QApplication::quit();
}

void MainWindow::onStatusMessage(const QString &message)
{
    statusBar()->showMessage(message, 3000);
}

} // namespace gui
} // namespace ak

#endif // BUILD_GUI