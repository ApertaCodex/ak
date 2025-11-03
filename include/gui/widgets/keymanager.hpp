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

namespace ak {
namespace gui {
namespace widgets {

// Forward declarations
class SecureInputWidget;

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
    
    // Configuration and data
    const core::Config& config;
    QString currentProfile;
    std::map<std::string, std::string> profileKeys;
    bool isDirty; // Track if keys have been modified
    
    // UI components
    QVBoxLayout *mainLayout;
    QHBoxLayout *toolbarLayout;
    
    // Toolbar components
    QComboBox *profileCombo;
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
};

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI