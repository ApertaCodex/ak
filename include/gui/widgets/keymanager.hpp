#pragma once

#ifdef BUILD_GUI

#include "core/config.hpp"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QMenu>
#include <QContextMenuEvent>
#include <QString>
#include <QStringList>
#include <QComboBox>
#include <QThread>
#include <QMutex>
#include <QInputDialog>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <fstream>

namespace ak {
namespace gui {
namespace widgets {

// Forward declarations
class SecureInputWidget;

// Worker class for loading profile keys in background thread
class ProfileKeysLoaderWorker : public QObject
{
    Q_OBJECT

public:
    ProfileKeysLoaderWorker(const core::Config& config, const QString& profileName, const QString& passphrase, QObject *parent = nullptr);
    
public slots:
    void loadKeys();

signals:
    void keysLoaded(const QString& profileName, const std::map<std::string, std::string>& keys);
    void loadFailed(const QString& profileName, const QString& error);

private:
    core::Config config;
    QString profileName;
    std::string passphrase;
};

// Worker class for saving profile keys in background thread
class ProfileKeysSaverWorker : public QObject
{
    Q_OBJECT

public:
    ProfileKeysSaverWorker(const core::Config& config,
                           const QString& profileName,
                           const std::map<std::string, std::string>& keys,
                           const QString& passphrase,
                           QObject *parent = nullptr);

public slots:
    void saveKeys();

signals:
    void saveCompleted(const QString& profileName);
    void saveFailed(const QString& profileName, const QString& error);

private:
    core::Config config;
    QString profileName;
    std::map<std::string, std::string> keysToSave;
    std::string passphrase;
};

// Custom table item for masked values
class MaskedTableItem : public QTableWidgetItem
{
public:
    explicit MaskedTableItem(const QString &actualValue);
    
    void setMasked(bool masked);
    bool isMasked() const;
    QString getActualValue() const;
    
private:
    void updateDisplayValue();
    
    QString actualValue;
    bool masked;
};

// Key Manager Widget
class KeyManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit KeyManagerWidget(const core::Config& config, QWidget *parent = nullptr);
    ~KeyManagerWidget() override;
    
    // Public interface
    void refreshKeys();
    void selectKey(const QString &keyName);
    void setCurrentProfile(const QString &profileName);
    QString getCurrentProfile() const;
    void refreshProfileList();

signals:
    void statusMessage(const QString &message);
    void profileChanged(const QString &profileName);

private slots:
    void addKey();
    void editKey();
    void deleteKey();
    void searchKeys(const QString &text);
    void toggleKeyVisibility();
    void testSelectedKey();
    void testAllKeys();
    void showContextMenu(const QPoint &pos);
    void onTableItemChanged(QTableWidgetItem *item);
    void onSelectionChanged();
    void onProfileChanged(const QString &profileName);
    void onKeysLoaded(const QString& profileName, const std::map<std::string, std::string>& keys);
    void onKeysLoadFailed(const QString& profileName, const QString& error);
    void onKeysSaved(const QString& profileName);
    void onKeysSaveFailed(const QString& profileName, const QString& error);
    // Profile management actions
    void createProfile();
    void deleteProfile();
    void renameProfile();
    void duplicateProfile();
    void importProfile();
    void exportProfile();

private:
    void setupUi();
    void setupTable();
    void setupToolbar();
    void setupContextMenu();
    void loadKeys();
    void saveKeys();
    void updateTable();
    void filterTable(const QString &filter);
    void loadProfileKeys(const QString &profileName);
    void saveProfileKeys(const QString &profileName);
    // Utility methods
    void addKeyToTable(const QString &name, const QString &value, const QString &service, const QString &apiUrl);
    QString detectService(const QString &keyName);
    QString getServiceApiUrl(const QString &service);
    QString getServiceCode(const QString &displayName);
    void updateTestStatus(const QString &keyName, bool success, const QString &message = "");
    bool validateKeyName(const QString &name);
    void showError(const QString &message);
    void showSuccess(const QString &message);
    void updateButtonStates();
    QString currentPassphrase() const;
    bool ensurePassphrase(bool requireConfirmation, QString &outPassphrase);
    void setCurrentPassphrase(const QString &passphrase, bool remember);
    void clearPassphrase();
    bool validateProfileName(const QString &name);
    
    void cleanupLoadThread(bool waitForFinish = true);
    void cleanupSaveThread(bool waitForFinish = true);
    
    // Configuration and data
    const core::Config& config;
    QString currentProfile;
    std::map<std::string, std::string> profileKeys;
    std::map<QString, std::map<std::string, std::string>> cachedProfileKeys; // Cache loaded keys to avoid repeated GPG prompts
    bool keysModified; // Track if keys have been modified since last load
    bool loadingInProgress;
    bool savingInProgress;
    QString sessionPassphrase;
    QString rememberedPassphrase;
    bool rememberPassphrase;
    
    // UI components
    QVBoxLayout *mainLayout;
    QHBoxLayout *toolbarLayout;
    
    // Toolbar components
    QComboBox *profileCombo;
    QPushButton *profileActionsButton;  // Menu button for profile actions
    QMenu *profileActionsMenu;  // Menu for profile management actions
    QLineEdit *searchEdit;
    QPushButton *addButton;
    QPushButton *editButton;
    QPushButton *deleteButton;
    QPushButton *toggleVisibilityButton;
    QPushButton *refreshButton;
    QPushButton *testSelectedButton;
    QPushButton *testAllButton;
    QLabel *statusLabel;
    
    // Table
    QTableWidget *table;
    
    // Context menu
    QMenu *contextMenu;
    QAction *addAction;
    QAction *editAction;
    QAction *deleteAction;
    QAction *copyNameAction;
    QAction *copyValueAction;
    QAction *toggleVisibilityAction;
    
    // Table columns
    enum TableColumn {
        ColumnName = 0,
        ColumnService = 1,
        ColumnUrl = 2,
        ColumnValue = 3,
        ColumnTestStatus = 4,
        ColumnActions = 5,
        ColumnCount = 6
    };
    
    // State
    bool globalVisibilityState;
    QString currentFilter;
    
    // Background loading/saving
    QThread *loadKeysThread;
    ProfileKeysLoaderWorker *loadKeysWorker;
    QThread *saveKeysThread;
    ProfileKeysSaverWorker *saveKeysWorker;
};

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI