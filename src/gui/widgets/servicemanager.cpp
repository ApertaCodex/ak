#ifdef BUILD_GUI

#include "gui/widgets/servicemanager.hpp"
#include "gui/widgets/common/dialogs.hpp"
#include "storage/vault.hpp"
#include "services/services.hpp"
#include "core/config.hpp"
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTimer>
#include <QRegularExpression>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

namespace ak {
namespace gui {
namespace widgets {

// ServiceEditorDialog Implementation
ServiceEditorDialog::ServiceEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Add Custom Service");
    setMinimumSize(450, 400);
    setupUi();
    updateUi();
}

ServiceEditorDialog::ServiceEditorDialog(const services::Service &service, QWidget *parent)
    : QDialog(parent), currentService(service)
{
    setWindowTitle(service.isBuiltIn ? "View/Edit Built-in Service" : "Edit User Service");
    setMinimumSize(450, 400);
    setupUi();
    setService(service);
    updateUi();
}

void ServiceEditorDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Form layout
    formLayout = new QFormLayout();
    
    nameEdit = new QLineEdit();
    nameEdit->setPlaceholderText("e.g., myapi");
    formLayout->addRow("Service Name:", nameEdit);
    
    keyNameEdit = new QLineEdit();
    keyNameEdit->setPlaceholderText("e.g., MYAPI_API_KEY");
    formLayout->addRow("Key Name:", keyNameEdit);
    
    descriptionEdit = new QTextEdit();
    descriptionEdit->setPlaceholderText("Optional description of the service");
    descriptionEdit->setMaximumHeight(80);
    formLayout->addRow("Description:", descriptionEdit);
    
    testableCheckBox = new QCheckBox("Enable testing for this service");
    formLayout->addRow("", testableCheckBox);
    
    builtInCheckBox = new QCheckBox("Built-in service (read-only core properties)");
    builtInCheckBox->setEnabled(false); // This should not be user-editable
    formLayout->addRow("", builtInCheckBox);
    
    testEndpointEdit = new QLineEdit();
    testEndpointEdit->setPlaceholderText("https://api.example.com/v1/status");
    formLayout->addRow("Test Endpoint:", testEndpointEdit);
    
    testMethodCombo = new QComboBox();
    testMethodCombo->addItems({"GET", "POST", "PUT", "DELETE", "HEAD"});
    formLayout->addRow("Test Method:", testMethodCombo);
    
    testHeadersEdit = new QLineEdit();
    testHeadersEdit->setPlaceholderText("-H 'Content-Type: application/json'");
    formLayout->addRow("Additional Headers:", testHeadersEdit);
    
    mainLayout->addLayout(formLayout);
    
    // Button box
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
    
    // Connect signals
    connect(nameEdit, &QLineEdit::textChanged, this, &ServiceEditorDialog::validateInput);
    connect(keyNameEdit, &QLineEdit::textChanged, this, &ServiceEditorDialog::validateInput);
    connect(testableCheckBox, &QCheckBox::toggled, this, &ServiceEditorDialog::onTestableChanged);
    
    validateInput();
}

services::Service ServiceEditorDialog::getService() const
{
    services::Service service;
    service.name = nameEdit->text().toStdString();
    service.keyName = keyNameEdit->text().toStdString();
    service.description = descriptionEdit->toPlainText().toStdString();
    service.testEndpoint = testEndpointEdit->text().toStdString();
    service.testMethod = testMethodCombo->currentText().toStdString();
    service.testHeaders = testHeadersEdit->text().toStdString();
    service.authMethod = "Bearer"; // Default auth method
    service.testable = testableCheckBox->isChecked();
    service.isBuiltIn = builtInCheckBox->isChecked();
    
    return service;
}

void ServiceEditorDialog::setService(const services::Service &service)
{
    currentService = service;
    nameEdit->setText(QString::fromStdString(service.name));
    keyNameEdit->setText(QString::fromStdString(service.keyName));
    descriptionEdit->setPlainText(QString::fromStdString(service.description));
    testableCheckBox->setChecked(service.testable);
    testEndpointEdit->setText(QString::fromStdString(service.testEndpoint));
    testMethodCombo->setCurrentText(QString::fromStdString(service.testMethod));
    testHeadersEdit->setText(QString::fromStdString(service.testHeaders));
    builtInCheckBox->setChecked(service.isBuiltIn);
    
    // Disable editing of built-in services' core properties
    nameEdit->setEnabled(!service.isBuiltIn);
    keyNameEdit->setEnabled(!service.isBuiltIn);
    builtInCheckBox->setEnabled(false); // This should not be user-editable
    
    updateUi();
}

bool ServiceEditorDialog::isBuiltIn() const
{
    return builtInCheckBox->isChecked();
}

void ServiceEditorDialog::validateInput()
{
    bool isValid = !nameEdit->text().isEmpty() && !keyNameEdit->text().isEmpty();
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(isValid);
}

void ServiceEditorDialog::onTestableChanged(bool testable)
{
    testEndpointEdit->setEnabled(testable);
    testMethodCombo->setEnabled(testable);
    testHeadersEdit->setEnabled(testable);
}

void ServiceEditorDialog::updateUi()
{
    onTestableChanged(testableCheckBox->isChecked());
}

// ServiceManagerWidget Implementation
ServiceManagerWidget::ServiceManagerWidget(const core::Config& config, QWidget *parent)
    : QWidget(parent), config(config)
{
    setupUi();
    setupTable();
    setupToolbar();
    setupContextMenu();
    loadServices();
}

void ServiceManagerWidget::setupUi()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    // Create toolbar layout
    toolbarLayout = new QHBoxLayout();
    mainLayout->addLayout(toolbarLayout);
    
    // Create table
    table = new QTableWidget(this);
    mainLayout->addWidget(table);
    
    // Status label
    statusLabel = new QLabel("Ready");
    statusLabel->setStyleSheet("QLabel { color: #666666; font-size: 11px; }");
    mainLayout->addWidget(statusLabel);
}

void ServiceManagerWidget::setupTable()
{
    table->setColumnCount(ColumnCount);
    QStringList headers = {"Name", "Key Name", "Description", "Type", "Testable", "Status"};
    table->setHorizontalHeaderLabels(headers);
    
    // Configure table appearance
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSortingEnabled(true);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // Configure column widths
    QHeaderView *header = table->horizontalHeader();
    header->setStretchLastSection(true);
    header->resizeSection(ColumnName, 120);
    header->resizeSection(ColumnKeyName, 150);
    header->resizeSection(ColumnDescription, 200);
    header->resizeSection(ColumnTestable, 80);
    
    // Connect signals
    connect(table, &QTableWidget::customContextMenuRequested,
            this, &ServiceManagerWidget::showContextMenu);
    connect(table, &QTableWidget::itemChanged,
            this, &ServiceManagerWidget::onTableItemChanged);
    connect(table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ServiceManagerWidget::onSelectionChanged);
    connect(table, &QTableWidget::itemDoubleClicked, [this]() {
        editService();
    });
}

void ServiceManagerWidget::setupToolbar()
{
    // Search
    searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText("Search services...");
    searchEdit->setMaximumWidth(200);
    connect(searchEdit, &QLineEdit::textChanged, this, &ServiceManagerWidget::searchServices);
    toolbarLayout->addWidget(searchEdit);
    
    toolbarLayout->addStretch();
    
    // Action buttons
    addButton = new QPushButton("Add Service");
    addButton->setIcon(QIcon(":/icons/add.png"));
    connect(addButton, &QPushButton::clicked, this, &ServiceManagerWidget::addService);
    toolbarLayout->addWidget(addButton);
    
    editButton = new QPushButton("Edit");
    editButton->setEnabled(false);
    connect(editButton, &QPushButton::clicked, this, &ServiceManagerWidget::editService);
    toolbarLayout->addWidget(editButton);
    
    deleteButton = new QPushButton("Delete");
    deleteButton->setEnabled(false);
    deleteButton->setStyleSheet("QPushButton { color: #d32f2f; }");
    connect(deleteButton, &QPushButton::clicked, this, &ServiceManagerWidget::deleteService);
    toolbarLayout->addWidget(deleteButton);
    
    toolbarLayout->addSpacing(15);
    
    testSelectedButton = new QPushButton("Test Selected");
    testSelectedButton->setEnabled(false);
    connect(testSelectedButton, &QPushButton::clicked, this, &ServiceManagerWidget::testSelectedService);
    toolbarLayout->addWidget(testSelectedButton);
    
    testAllButton = new QPushButton("Test All");
    connect(testAllButton, &QPushButton::clicked, this, &ServiceManagerWidget::testAllServices);
    toolbarLayout->addWidget(testAllButton);
    
    toolbarLayout->addSpacing(15);
    
    exportButton = new QPushButton("Export");
    connect(exportButton, &QPushButton::clicked, this, &ServiceManagerWidget::exportServices);
    toolbarLayout->addWidget(exportButton);
    
    importButton = new QPushButton("Import");
    connect(importButton, &QPushButton::clicked, this, &ServiceManagerWidget::importServices);
    toolbarLayout->addWidget(importButton);
    
    refreshButton = new QPushButton("Refresh");
    connect(refreshButton, &QPushButton::clicked, this, &ServiceManagerWidget::refreshServices);
    toolbarLayout->addWidget(refreshButton);
}

void ServiceManagerWidget::setupContextMenu()
{
    contextMenu = new QMenu(this);
    
    addAction = contextMenu->addAction("Add Service");
    connect(addAction, &QAction::triggered, this, &ServiceManagerWidget::addService);
    
    contextMenu->addSeparator();
    
    editAction = contextMenu->addAction("Edit");
    connect(editAction, &QAction::triggered, this, &ServiceManagerWidget::editService);
    
    deleteAction = contextMenu->addAction("Delete");
    connect(deleteAction, &QAction::triggered, this, &ServiceManagerWidget::deleteService);
    
    contextMenu->addSeparator();
    
    testAction = contextMenu->addAction("Test Service");
    connect(testAction, &QAction::triggered, this, &ServiceManagerWidget::testSelectedService);
    
    contextMenu->addSeparator();
    
    copyNameAction = contextMenu->addAction("Copy Name");
    connect(copyNameAction, &QAction::triggered, [this]() {
        int row = table->currentRow();
        if (row >= 0) {
            QApplication::clipboard()->setText(table->item(row, ColumnName)->text());
            emit statusMessage("Service name copied to clipboard");
        }
    });
    
    duplicateAction = contextMenu->addAction("Duplicate");
    connect(duplicateAction, &QAction::triggered, [this]() {
        int row = table->currentRow();
        if (row >= 0 && row < table->rowCount()) {
            QString serviceName = table->item(row, ColumnName)->text();
            auto it = allServices.find(serviceName.toStdString());
            if (it != allServices.end()) {
                auto service = it->second;
                service.name += "_copy";
                service.isBuiltIn = false; // Duplicated services are always user-defined
                ServiceEditorDialog dialog(service, this);
                if (dialog.exec() == QDialog::Accepted) {
                    auto newService = dialog.getService();
                    try {
                        services::addService(config, newService);
                        refreshServices();
                        emit statusMessage("Service duplicated successfully");
                    } catch (const std::exception& e) {
                        showError("Failed to duplicate service: " + QString::fromStdString(e.what()));
                    }
                }
            }
        }
    });
}

void ServiceManagerWidget::loadServices()
{
    try {
        allServices = services::loadAllServices(config);
        updateTable();
        emit statusMessage(QString("Loaded %1 services (%2 built-in, %3 user-defined)")
                          .arg(allServices.size())
                          .arg(std::count_if(allServices.begin(), allServices.end(),
                               [](const auto& pair) { return pair.second.isBuiltIn; }))
                          .arg(std::count_if(allServices.begin(), allServices.end(),
                               [](const auto& pair) { return !pair.second.isBuiltIn; })));
    } catch (const std::exception& e) {
        showError("Failed to load services: " + QString::fromStdString(e.what()));
    }
}

void ServiceManagerWidget::saveServices()
{
    try {
        services::saveUserServices(config, allServices);
        emit statusMessage("User services saved successfully");
    } catch (const std::exception& e) {
        showError("Failed to save services: " + QString::fromStdString(e.what()));
    }
}

void ServiceManagerWidget::updateTable()
{
    table->setRowCount(0);
    
    for (const auto& [name, service] : allServices) {
        if (!currentFilter.isEmpty() &&
            !QString::fromStdString(service.name).contains(currentFilter, Qt::CaseInsensitive) &&
            !QString::fromStdString(service.keyName).contains(currentFilter, Qt::CaseInsensitive) &&
            !QString::fromStdString(service.description).contains(currentFilter, Qt::CaseInsensitive)) {
            continue;
        }
        
        addServiceToTable(service);
    }
    
    table->resizeRowsToContents();
}

void ServiceManagerWidget::addServiceToTable(const services::Service &service)
{
    int row = table->rowCount();
    table->insertRow(row);
    
    // Name
    QTableWidgetItem *nameItem = new QTableWidgetItem(QString::fromStdString(service.name));
    nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
    if (service.isBuiltIn) {
        nameItem->setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    } else {
        nameItem->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
    }
    table->setItem(row, ColumnName, nameItem);
    
    // Key Name
    QTableWidgetItem *keyItem = new QTableWidgetItem(QString::fromStdString(service.keyName));
    keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, ColumnKeyName, keyItem);
    
    // Description
    QString desc = QString::fromStdString(service.description);
    if (desc.length() > 50) {
        desc = desc.left(47) + "...";
    }
    QTableWidgetItem *descItem = new QTableWidgetItem(desc);
    descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
    descItem->setToolTip(QString::fromStdString(service.description));
    table->setItem(row, ColumnDescription, descItem);
    
    // Type
    QTableWidgetItem *typeItem = new QTableWidgetItem(service.isBuiltIn ? "Built-in" : "User");
    typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
    if (service.isBuiltIn) {
        typeItem->setForeground(QColor(0, 100, 0)); // Dark green for built-in
    } else {
        typeItem->setForeground(QColor(0, 0, 150)); // Dark blue for user-defined
    }
    table->setItem(row, ColumnType, typeItem);
    
    // Testable
    QTableWidgetItem *testableItem = new QTableWidgetItem(service.testable ? "Yes" : "No");
    testableItem->setFlags(testableItem->flags() & ~Qt::ItemIsEditable);
    if (service.testable) {
        testableItem->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogApplyButton));
    }
    table->setItem(row, ColumnTestable, testableItem);
    
    // Status
    QTableWidgetItem *statusItem = new QTableWidgetItem("Not tested");
    statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, ColumnTestStatus, statusItem);
}

bool ServiceManagerWidget::canEditService(const services::Service &service)
{
    (void)service; // Currently unused, all services are editable
    
    // Built-in services can have their testing configuration edited, but not core properties
    return true;
}

bool ServiceManagerWidget::canDeleteService(const services::Service &service)
{
    // Only user-defined services can be deleted
    return !service.isBuiltIn;
}

void ServiceManagerWidget::refreshServices()
{
    loadServices();
}

void ServiceManagerWidget::selectService(const QString &serviceName)
{
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->item(row, ColumnName)->text() == serviceName) {
            table->selectRow(row);
            break;
        }
    }
}

void ServiceManagerWidget::addService()
{
    ServiceEditorDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        auto service = dialog.getService();
        
        // Check if service name already exists
        if (allServices.find(service.name) != allServices.end()) {
            showError("A service with this name already exists");
            return;
        }
        
        try {
            services::addService(config, service);
            refreshServices();
            selectService(QString::fromStdString(service.name));
            emit statusMessage("Service added successfully");
        } catch (const std::exception& e) {
            showError("Failed to add service: " + QString::fromStdString(e.what()));
        }
    }
}

void ServiceManagerWidget::editService()
{
    int row = table->currentRow();
    if (row < 0 || row >= table->rowCount()) {
        return;
    }
    
    QString serviceName = table->item(row, ColumnName)->text();
    auto it = allServices.find(serviceName.toStdString());
    if (it == allServices.end()) {
        return;
    }
    
    auto service = it->second;
    if (!canEditService(service)) {
        showError("This service cannot be edited");
        return;
    }
    
    ServiceEditorDialog dialog(service, this);
    if (dialog.exec() == QDialog::Accepted) {
        auto updatedService = dialog.getService();
        
        // Check if name conflicts with other services (if name changed)
        if (updatedService.name != service.name &&
            allServices.find(updatedService.name) != allServices.end()) {
            showError("A service with this name already exists");
            return;
        }
        
        try {
            services::updateService(config, updatedService);
            refreshServices();
            selectService(QString::fromStdString(updatedService.name));
            emit statusMessage("Service updated successfully");
        } catch (const std::exception& e) {
            showError("Failed to update service: " + QString::fromStdString(e.what()));
        }
    }
}

void ServiceManagerWidget::deleteService()
{
    int row = table->currentRow();
    if (row < 0 || row >= table->rowCount()) {
        return;
    }
    
    QString serviceName = table->item(row, ColumnName)->text();
    auto it = allServices.find(serviceName.toStdString());
    if (it == allServices.end()) {
        return;
    }
    
    auto service = it->second;
    if (!canDeleteService(service)) {
        showError("Built-in services cannot be deleted");
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Delete Service",
        QString("Are you sure you want to delete the service '%1'?\n\n"
                "This action cannot be undone.").arg(serviceName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        try {
            services::removeService(config, service.name);
            refreshServices();
            emit statusMessage("Service deleted successfully");
        } catch (const std::exception& e) {
            showError("Failed to delete service: " + QString::fromStdString(e.what()));
        }
    }
}

void ServiceManagerWidget::searchServices(const QString &text)
{
    currentFilter = text;
    updateTable();
}

void ServiceManagerWidget::testSelectedService()
{
    int row = table->currentRow();
    if (row < 0 || row >= table->rowCount()) {
        return;
    }
    
    QString serviceName = table->item(row, 0)->text();
    auto it = allServices.find(serviceName.toStdString());
    if (it == allServices.end()) {
        return;
    }
    
    auto service = it->second;
    if (!service.testable) {
        showError("This service is not configured for testing");
        return;
    }
    
    // Use the existing serviceName variable
    table->item(row, ColumnTestStatus)->setText("Testing...");
    
    QTimer::singleShot(100, [this, serviceName, row]() {
        try {
            auto result = services::test_one(config, serviceName.toStdString());
            updateTestStatus(serviceName, result.ok, QString::fromStdString(result.error_message));
        } catch (const std::exception& e) {
            updateTestStatus(serviceName, false, QString::fromStdString(e.what()));
        }
    });
}

void ServiceManagerWidget::testAllServices()
{
    std::vector<std::string> testableServices;
    for (const auto& [name, service] : allServices) {
        if (service.testable) {
            testableServices.push_back(service.name);
        }
    }
    
    if (testableServices.empty()) {
        showError("No testable services found");
        return;
    }
    
    // Update status for all testable services
    for (int row = 0; row < table->rowCount(); ++row) {
        QString serviceName = table->item(row, ColumnName)->text();
        for (const auto& testable : testableServices) {
            if (serviceName.toStdString() == testable) {
                table->item(row, ColumnTestStatus)->setText("Testing...");
                break;
            }
        }
    }
    
    QTimer::singleShot(100, [this, testableServices]() {
        try {
            auto results = services::run_tests_parallel(config, testableServices, false);
            for (const auto& result : results) {
                updateTestStatus(QString::fromStdString(result.service), result.ok, 
                               QString::fromStdString(result.error_message));
            }
        } catch (const std::exception& e) {
            showError("Test failed: " + QString::fromStdString(e.what()));
        }
    });
}

void ServiceManagerWidget::exportServices()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Export Services", "custom_services.json", "JSON Files (*.json)");
    if (fileName.isEmpty()) return;
    
    try {
        QJsonArray servicesArray;
        for (const auto& [name, service] : allServices) {
            if (!service.isBuiltIn) {  // Only export user-defined services
                QJsonObject serviceObj;
                serviceObj["name"] = QString::fromStdString(service.name);
                serviceObj["keyName"] = QString::fromStdString(service.keyName);
                serviceObj["description"] = QString::fromStdString(service.description);
                serviceObj["testEndpoint"] = QString::fromStdString(service.testEndpoint);
                serviceObj["testMethod"] = QString::fromStdString(service.testMethod);
                serviceObj["testHeaders"] = QString::fromStdString(service.testHeaders);
                serviceObj["testable"] = service.testable;
                servicesArray.append(serviceObj);
            }
        }
        
        QJsonDocument doc(servicesArray);
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
            emit statusMessage("Services exported successfully");
        } else {
            showError("Failed to write export file");
        }
    } catch (const std::exception& e) {
        showError("Export failed: " + QString::fromStdString(e.what()));
    }
}

void ServiceManagerWidget::importServices()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Import Services", "", "JSON Files (*.json)");
    if (fileName.isEmpty()) return;
    
    try {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            showError("Failed to open import file");
            return;
        }
        
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isArray()) {
            showError("Invalid import file format");
            return;
        }
        
        QJsonArray servicesArray = doc.array();
        int imported = 0;
        int skipped = 0;
        
        for (const auto& value : servicesArray) {
            QJsonObject obj = value.toObject();
            
            services::Service service;
            service.name = obj["name"].toString().toStdString();
            service.isBuiltIn = false;  // Imported services are user-defined
            service.keyName = obj["keyName"].toString().toStdString();
            service.description = obj["description"].toString().toStdString();
            service.testEndpoint = obj["testEndpoint"].toString().toStdString();
            service.testMethod = obj["testMethod"].toString().toStdString();
            service.testHeaders = obj["testHeaders"].toString().toStdString();
            service.testable = obj["testable"].toBool();
            
            // Check if service already exists
            bool exists = false;
            for (const auto& [existingName, existing] : allServices) {
                if (existing.name == service.name) {
                    exists = true;
                    break;
                }
            }
            
            if (!exists) {
                services::addService(config, service);
                imported++;
            } else {
                skipped++;
            }
        }
        
        refreshServices();
        emit statusMessage(QString("Import complete: %1 imported, %2 skipped").arg(imported).arg(skipped));
    } catch (const std::exception& e) {
        showError("Import failed: " + QString::fromStdString(e.what()));
    }
}

void ServiceManagerWidget::showContextMenu(const QPoint &pos)
{
    QTableWidgetItem *item = table->itemAt(pos);
    bool hasSelection = item != nullptr;
    
    editAction->setEnabled(hasSelection);
    deleteAction->setEnabled(hasSelection);
    testAction->setEnabled(hasSelection);
    copyNameAction->setEnabled(hasSelection);
    duplicateAction->setEnabled(hasSelection);
    
    contextMenu->exec(table->mapToGlobal(pos));
}

void ServiceManagerWidget::onTableItemChanged(QTableWidgetItem *item)
{
    Q_UNUSED(item)
    // Table items are read-only, so this shouldn't happen
}

void ServiceManagerWidget::onSelectionChanged()
{
    bool hasSelection = table->currentRow() >= 0;
    editButton->setEnabled(hasSelection);
    deleteButton->setEnabled(hasSelection);
    testSelectedButton->setEnabled(hasSelection);
}

void ServiceManagerWidget::updateTestStatus(const QString &serviceName, bool success, const QString &message)
{
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->item(row, ColumnName)->text() == serviceName) {
            QTableWidgetItem *statusItem = table->item(row, ColumnTestStatus);
            if (success) {
                statusItem->setText("✅ Success");
                statusItem->setToolTip("Test passed");
            } else {
                statusItem->setText("❌ Failed");
                statusItem->setToolTip(message.isEmpty() ? "Test failed" : message);
            }
            break;
        }
    }
}

bool ServiceManagerWidget::validateServiceName(const QString &name)
{
    if (name.isEmpty()) return false;
    
    // Check for valid characters (alphanumeric and underscore)
    QRegularExpression re("^[a-zA-Z0-9_]+$");
    return re.match(name).hasMatch();
}

void ServiceManagerWidget::showError(const QString &message)
{
    QMessageBox::critical(this, "Error", message);
    emit statusMessage("Error: " + message);
}

void ServiceManagerWidget::showSuccess(const QString &message)
{
    emit statusMessage(message);
}

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI