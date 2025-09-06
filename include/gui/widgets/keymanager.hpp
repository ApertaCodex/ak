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

signals:
    void statusMessage(const QString &message);

private slots:
    void addKey();
    void editKey();
    void deleteKey();
    void searchKeys(const QString &text);
    void toggleKeyVisibility();
    void showContextMenu(const QPoint &pos);
    void onTableItemChanged(QTableWidgetItem *item);
    void onSelectionChanged();

private:
    void setupUi();
    void setupTable();
    void setupToolbar();
    void setupContextMenu();
    void loadKeys();
    void saveKeys();
    void updateTable();
    void filterTable(const QString &filter);
    
    // Utility methods
    void addKeyToTable(const QString &name, const QString &value, const QString &service);
    QString detectService(const QString &keyName);
    bool validateKeyName(const QString &name);
    void showError(const QString &message);
    void showSuccess(const QString &message);
    
    // Configuration and data
    const core::Config& config;
    core::KeyStore keyStore;
    
    // UI components
    QVBoxLayout *mainLayout;
    QHBoxLayout *toolbarLayout;
    
    // Toolbar components
    QLineEdit *searchEdit;
    QPushButton *addButton;
    QPushButton *editButton;
    QPushButton *deleteButton;
    QPushButton *toggleVisibilityButton;
    QPushButton *refreshButton;
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
        ColumnActions = 4,
        ColumnCount = 5
    };
    
    // State
    bool globalVisibilityState;
    QString currentFilter;
};

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI