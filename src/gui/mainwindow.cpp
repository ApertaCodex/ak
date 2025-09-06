#ifdef BUILD_GUI

#include "gui/mainwindow.hpp"
#include <QApplication>
#include <QMessageBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QGroupBox>
#include <QFormLayout>

namespace ak {
namespace gui {

MainWindow::MainWindow(const core::Config& cfg, QWidget *parent)
    : QMainWindow(parent), config(cfg), tabWidget(nullptr),
      keyManagerWidget(nullptr), profileManagerWidget(nullptr),
      serviceManagerWidget(nullptr), settingsTab(nullptr), exitAction(nullptr), aboutAction(nullptr),
      helpAction(nullptr)
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

    // Profile Manager Tab
    profileManagerWidget = new widgets::ProfileManagerWidget(config, this);
    connect(profileManagerWidget, &widgets::ProfileManagerWidget::statusMessage,
            this, &MainWindow::onStatusMessage);
    tabWidget->addTab(profileManagerWidget, "Profile Manager");

    // Service Manager Tab
    serviceManagerWidget = new widgets::ServiceManagerWidget(config, this);
    connect(serviceManagerWidget, &widgets::ServiceManagerWidget::statusMessage,
            this, &MainWindow::onStatusMessage);
    tabWidget->addTab(serviceManagerWidget, "Service Manager");

    // Settings Tab - Full Implementation
    settingsTab = new QWidget();
    QVBoxLayout *settingsLayout = new QVBoxLayout(settingsTab);
    settingsLayout->setContentsMargins(20, 20, 20, 20);
    settingsLayout->setSpacing(15);
    
    // Backend Settings Group
    QGroupBox *backendGroup = new QGroupBox("Security Backend", settingsTab);
    QVBoxLayout *backendLayout = new QVBoxLayout(backendGroup);
    
    QLabel *backendStatus = new QLabel(QString("Current Backend: <b>%1</b>")
        .arg(config.gpgAvailable && !config.forcePlain ? "GPG Encryption" : "Plain Text"));
    backendLayout->addWidget(backendStatus);
    
    if (config.gpgAvailable) {
        QLabel *gpgInfo = new QLabel("✅ GPG encryption is available and active");
        gpgInfo->setStyleSheet("color: #00aa00;");
        backendLayout->addWidget(gpgInfo);
    } else {
        QLabel *gpgWarning = new QLabel("⚠️ GPG not available - secrets stored as plain text");
        gpgWarning->setStyleSheet("color: #ff6600;");
        backendLayout->addWidget(gpgWarning);
    }
    
    // File Paths Group
    QGroupBox *pathsGroup = new QGroupBox("File Locations", settingsTab);
    QFormLayout *pathsLayout = new QFormLayout(pathsGroup);
    
    QLabel *configDirLabel = new QLabel(QString::fromStdString(config.configDir));
    configDirLabel->setStyleSheet("font-family: monospace;");
    pathsLayout->addRow("Config Directory:", configDirLabel);
    
    QLabel *vaultPathLabel = new QLabel(QString::fromStdString(config.vaultPath));
    vaultPathLabel->setStyleSheet("font-family: monospace;");
    pathsLayout->addRow("Vault Path:", vaultPathLabel);
    
    QLabel *profilesDirLabel = new QLabel(QString::fromStdString(config.profilesDir));
    profilesDirLabel->setStyleSheet("font-family: monospace;");
    pathsLayout->addRow("Profiles Directory:", profilesDirLabel);
    
    QLabel *auditLogLabel = new QLabel(QString::fromStdString(config.auditLogPath));
    auditLogLabel->setStyleSheet("font-family: monospace;");
    pathsLayout->addRow("Audit Log:", auditLogLabel);
    
    // Application Info Group
    QGroupBox *infoGroup = new QGroupBox("Application Information", settingsTab);
    QFormLayout *infoLayout = new QFormLayout(infoGroup);
    
    QLabel *versionLabel = new QLabel("v" AK_VERSION_STRING);
    versionLabel->setStyleSheet("font-weight: bold;");
    infoLayout->addRow("Version:", versionLabel);
    
    QLabel *instanceLabel = new QLabel(QString::fromStdString(config.instanceId).left(8) + "...");
    instanceLabel->setStyleSheet("font-family: monospace;");
    infoLayout->addRow("Instance ID:", instanceLabel);
    
    // Clipboard Tools Detection
    QGroupBox *toolsGroup = new QGroupBox("System Tools", settingsTab);
    QVBoxLayout *toolsLayout = new QVBoxLayout(toolsGroup);
    
    // Check for clipboard tools
    QStringList clipboardTools = {"pbcopy", "wl-copy", "xclip"};
    bool foundClipboard = false;
    for (const auto& tool : clipboardTools) {
        if (core::commandExists(tool.toStdString())) {
            QLabel *toolLabel = new QLabel(QString("✅ Clipboard: %1").arg(tool));
            toolLabel->setStyleSheet("color: #00aa00;");
            toolsLayout->addWidget(toolLabel);
            foundClipboard = true;
            break;
        }
    }
    if (!foundClipboard) {
        QLabel *noClipboard = new QLabel("⚠️ No clipboard tools found (install xclip, wl-clipboard, or pbcopy)");
        noClipboard->setStyleSheet("color: #ff6600;");
        toolsLayout->addWidget(noClipboard);
    }
    
    // Add all groups to layout
    settingsLayout->addWidget(backendGroup);
    settingsLayout->addWidget(pathsGroup);
    settingsLayout->addWidget(infoGroup);
    settingsLayout->addWidget(toolsGroup);
    settingsLayout->addStretch(); // Push content to top
    
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
        "• Key Manager: Manage your API keys and test endpoints\n"
        "• Profile Manager: Create and manage key profiles\n"
        "• Service Manager: Add and manage custom API services\n"
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