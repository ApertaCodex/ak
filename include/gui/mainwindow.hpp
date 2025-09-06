#pragma once

#ifdef BUILD_GUI

#include "core/config.hpp"
#include "gui/widgets/keymanager.hpp"
#include "gui/widgets/profilemanager.hpp"
#include "gui/widgets/servicemanager.hpp"
#include <QMainWindow>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QAction>

namespace ak {
namespace gui {

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const core::Config& cfg, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void showAbout();
    void showHelp();
    void exitApplication();
    void onStatusMessage(const QString &message);

private:
    void setupUi();
    void setupMenuBar();
    void setupStatusBar();
    void setupTabs();

    const core::Config& config;

    // UI Components
    QTabWidget *tabWidget;
    widgets::KeyManagerWidget *keyManagerWidget;
    widgets::ProfileManagerWidget *profileManagerWidget;
    widgets::ServiceManagerWidget *serviceManagerWidget;
    QWidget *settingsTab;

    // Menu actions
    QAction *exitAction;
    QAction *aboutAction;
    QAction *helpAction;
};

} // namespace gui
} // namespace ak

#endif // BUILD_GUI