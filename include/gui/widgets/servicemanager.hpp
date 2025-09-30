#pragma once

#ifdef BUILD_GUI

#include "core/config.hpp"
#include "services/services.hpp"
#include "gui/widgets/common/secureinput.hpp"
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
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>

namespace ak {
namespace gui {
namespace widgets {

// Service Editor Dialog
class ServiceEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ServiceEditorDialog(QWidget *parent = nullptr);
    explicit ServiceEditorDialog(const services::Service &service, QWidget *parent = nullptr);
    
    services::Service getService() const;
    void setService(const services::Service &service);
    bool isBuiltIn() const;
    QString getDetectedApiKeyValue() const;

private slots:
    void validateInput();
    void onTestableChanged(bool testable);
    void onAuthTypeChanged(int index);
    void onTestMethodChanged(int index);
    void importFromCurl();

private:
    void setupUi();
    void updateUi();
    QFormLayout *formLayout;
    QLineEdit *nameEdit;
    QLineEdit *keyNameEdit;
    QTextEdit *descriptionEdit;
    QLineEdit *testEndpointEdit;
    QComboBox *testMethodCombo;
    QLineEdit *testHeadersEdit;
    QPlainTextEdit *testBodyEdit;
    QCheckBox *testableCheckBox;
    QDialogButtonBox *buttonBox;
    QCheckBox *builtInCheckBox;
    QComboBox *authTypeCombo;
    QLineEdit *authParameterEdit;
    QLineEdit *authPrefixEdit;
    SecureInputWidget *apiKeyValueEdit;
    QPushButton *importCurlButton;
    QString detectedApiKeyValue;
    QString lastCurlCommand;

    services::Service currentService;
};

// Custom Service Manager Widget
class ServiceManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ServiceManagerWidget(const core::Config& config, QWidget *parent = nullptr);
    
    // Public interface
    void refreshServices();
    void selectService(const QString &serviceName);

signals:
    void statusMessage(const QString &message);

private slots:
    void addService();
    void editService();
    void deleteService();
    void searchServices(const QString &text);
    void testSelectedService();
    void testAllServices();
    void showContextMenu(const QPoint &pos);
    void onTableItemChanged(QTableWidgetItem *item);
    void onSelectionChanged();
    void exportServices();
    void importServices();

private:
    void setupUi();
    void setupTable();
    void setupToolbar();
    void setupContextMenu();
    void loadServices();
    void saveServices();
    void updateTable();
    void filterTable(const QString &filter);
    
    // Utility methods
    void addServiceToTable(const services::Service &service);
    void updateTestStatus(const QString &serviceName, bool success, const QString &message = "");
    bool validateServiceName(const QString &name);
    bool canEditService(const services::Service &service);
    bool canDeleteService(const services::Service &service);
    void showError(const QString &message);
    void showSuccess(const QString &message);
    
    // Configuration and data
    const core::Config& config;
    std::map<std::string, services::Service> allServices;
    
    // UI components
    QVBoxLayout *mainLayout;
    QHBoxLayout *toolbarLayout;
    
    // Toolbar components
    QLineEdit *searchEdit;
    QPushButton *addButton;
    QPushButton *editButton;
    QPushButton *deleteButton;
    QPushButton *refreshButton;
    QPushButton *testSelectedButton;
    QPushButton *testAllButton;
    QPushButton *exportButton;
    QPushButton *importButton;
    QLabel *statusLabel;
    
    // Table
    QTableWidget *table;
    
    // Context menu
    QMenu *contextMenu;
    QAction *addAction;
    QAction *editAction;
    QAction *deleteAction;
    QAction *testAction;
    QAction *copyNameAction;
    QAction *duplicateAction;
    
    // Table columns
    enum TableColumn {
        ColumnName = 0,
        ColumnKeyName = 1,
        ColumnDescription = 2,
        ColumnType = 3,
        ColumnTestable = 4,
        ColumnTestStatus = 5,
        ColumnCount = 6
    };
    
    // State
    QString currentFilter;
};

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI
