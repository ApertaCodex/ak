#ifdef BUILD_GUI

#include "gui/widgets/keymanager.hpp"
#include "gui/widgets/common/dialogs.hpp"
#include "gui/widgets/common/secureinput.hpp"
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
#include <QMap>
#include <algorithm>

namespace ak {
namespace gui {
namespace widgets {

// MaskedTableItem Implementation
MaskedTableItem::MaskedTableItem(const QString &actualValue)
    : QTableWidgetItem(), actualValue(actualValue), masked(true)
{
    updateDisplayValue();
}

void MaskedTableItem::setMasked(bool masked)
{
    this->masked = masked;
    updateDisplayValue();
}

bool MaskedTableItem::isMasked() const
{
    return masked;
}

QString MaskedTableItem::getActualValue() const
{
    return actualValue;
}

void MaskedTableItem::updateDisplayValue()
{
    if (masked) {
        QString maskedValue;
        if (actualValue.length() > 8) {
            maskedValue = actualValue.left(4) + QString("*").repeated(actualValue.length() - 8) + actualValue.right(4);
        } else {
            maskedValue = QString("*").repeated(actualValue.length());
        }
        setText(maskedValue);
        setToolTip("Click the eye button to reveal value");
    } else {
        setText(actualValue);
        setToolTip("");
    }
}

// KeyManagerWidget Implementation
KeyManagerWidget::KeyManagerWidget(const core::Config& config, QWidget *parent)
    : QWidget(parent), config(config), mainLayout(nullptr), toolbarLayout(nullptr),
      searchEdit(nullptr), addButton(nullptr), editButton(nullptr), deleteButton(nullptr),
      toggleVisibilityButton(nullptr), refreshButton(nullptr), statusLabel(nullptr),
      table(nullptr), contextMenu(nullptr), addAction(nullptr), editAction(nullptr),
      deleteAction(nullptr), copyNameAction(nullptr), copyValueAction(nullptr),
      toggleVisibilityAction(nullptr), globalVisibilityState(true)
{
    setupUi();
    loadKeys();
    updateTable();
}

void KeyManagerWidget::setupUi()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    setupToolbar();
    setupTable();
    setupContextMenu();
    
    // Status label
    statusLabel = new QLabel("Ready", this);
    statusLabel->setStyleSheet("color: #666; font-size: 12px;");
    mainLayout->addWidget(statusLabel);
}

void KeyManagerWidget::setupToolbar()
{
    toolbarLayout = new QHBoxLayout();
    
    // Search box
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Search keys...");
    searchEdit->setMaximumWidth(200);
    connect(searchEdit, &QLineEdit::textChanged, this, &KeyManagerWidget::searchKeys);
    
    // Buttons
    addButton = new QPushButton("Add Key", this);
    addButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
    connect(addButton, &QPushButton::clicked, this, &KeyManagerWidget::addKey);
    
    editButton = new QPushButton("Edit", this);
    editButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    editButton->setEnabled(false);
    connect(editButton, &QPushButton::clicked, this, &KeyManagerWidget::editKey);
    
    deleteButton = new QPushButton("Delete", this);
    deleteButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_TrashIcon));
    deleteButton->setEnabled(false);
    connect(deleteButton, &QPushButton::clicked, this, &KeyManagerWidget::deleteKey);
    
    toggleVisibilityButton = new QPushButton("Show All", this);
    toggleVisibilityButton->setCheckable(true);
    toggleVisibilityButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    connect(toggleVisibilityButton, &QPushButton::clicked, this, &KeyManagerWidget::toggleKeyVisibility);
    
    refreshButton = new QPushButton("Refresh", this);
    refreshButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserReload));
    connect(refreshButton, &QPushButton::clicked, this, &KeyManagerWidget::refreshKeys);
    
    // Test buttons
    testSelectedButton = new QPushButton("Test", this);
    testSelectedButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
    testSelectedButton->setEnabled(false);
    connect(testSelectedButton, &QPushButton::clicked, this, &KeyManagerWidget::testSelectedKey);
    
    testAllButton = new QPushButton("Test All", this);
    testAllButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    connect(testAllButton, &QPushButton::clicked, this, &KeyManagerWidget::testAllKeys);
    
    // Layout
    toolbarLayout->addWidget(searchEdit);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(addButton);
    toolbarLayout->addWidget(editButton);
    toolbarLayout->addWidget(deleteButton);
    toolbarLayout->addWidget(testSelectedButton);
    toolbarLayout->addWidget(testAllButton);
    toolbarLayout->addWidget(toggleVisibilityButton);
    toolbarLayout->addWidget(refreshButton);
    
    mainLayout->addLayout(toolbarLayout);
}

void KeyManagerWidget::setupTable()
{
    table = new QTableWidget(this);
    table->setColumnCount(ColumnCount);
    
    QStringList headers = {"Key Name", "Service", "API URL", "Value", "Actions"};
    table->setHorizontalHeaderLabels(headers);
    
    // Configure table appearance
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setSortingEnabled(true);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // Configure column widths
    QHeaderView *header = table->horizontalHeader();
    header->setSectionResizeMode(ColumnName, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ColumnService, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ColumnUrl, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ColumnValue, QHeaderView::Stretch);
    header->setSectionResizeMode(ColumnActions, QHeaderView::ResizeToContents);
    
    // Connect signals
    connect(table, &QTableWidget::itemSelectionChanged, this, &KeyManagerWidget::onSelectionChanged);
    connect(table, &QTableWidget::itemChanged, this, &KeyManagerWidget::onTableItemChanged);
    connect(table, &QTableWidget::customContextMenuRequested, this, &KeyManagerWidget::showContextMenu);
    connect(table, &QTableWidget::itemDoubleClicked, this, &KeyManagerWidget::editKey);
    
    mainLayout->addWidget(table);
}

void KeyManagerWidget::setupContextMenu()
{
    contextMenu = new QMenu(this);
    
    addAction = contextMenu->addAction("Add Key");
    addAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
    connect(addAction, &QAction::triggered, this, &KeyManagerWidget::addKey);
    
    editAction = contextMenu->addAction("Edit Key");
    editAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    connect(editAction, &QAction::triggered, this, &KeyManagerWidget::editKey);
    
    deleteAction = contextMenu->addAction("Delete Key");
    deleteAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_TrashIcon));
    connect(deleteAction, &QAction::triggered, this, &KeyManagerWidget::deleteKey);
    
    contextMenu->addSeparator();
    
    copyNameAction = contextMenu->addAction("Copy Name");
    copyNameAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogApplyButton));
    connect(copyNameAction, &QAction::triggered, [this]() {
        int row = table->currentRow();
        if (row >= 0) {
            QString name = table->item(row, ColumnName)->text();
            QApplication::clipboard()->setText(name);
            showSuccess("Key name copied to clipboard");
        }
    });
    
    copyValueAction = contextMenu->addAction("Copy Value");
    copyValueAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogApplyButton));
    connect(copyValueAction, &QAction::triggered, [this]() {
        int row = table->currentRow();
        if (row >= 0) {
            MaskedTableItem *item = dynamic_cast<MaskedTableItem*>(table->item(row, ColumnValue));
            if (item) {
                QApplication::clipboard()->setText(item->getActualValue());
                showSuccess("Key value copied to clipboard");
            }
        }
    });
    
    toggleVisibilityAction = contextMenu->addAction("Toggle Visibility");
    connect(toggleVisibilityAction, &QAction::triggered, [this]() {
        int row = table->currentRow();
        if (row >= 0) {
            MaskedTableItem *item = dynamic_cast<MaskedTableItem*>(table->item(row, ColumnValue));
            if (item) {
                item->setMasked(!item->isMasked());
            }
        }
    });
}

void KeyManagerWidget::loadKeys()
{
    try {
        keyStore = ak::storage::loadVault(config);
        emit statusMessage(QString("Loaded %1 keys").arg(keyStore.kv.size()));
    } catch (const std::exception& e) {
        showError(QString("Failed to load keys: %1").arg(e.what()));
        keyStore = core::KeyStore(); // Empty keystore on error
    }
}

void KeyManagerWidget::saveKeys()
{
    try {
        ak::storage::saveVault(config, keyStore);
        emit statusMessage("Keys saved successfully");
    } catch (const std::exception& e) {
        showError(QString("Failed to save keys: %1").arg(e.what()));
    }
}

void KeyManagerWidget::updateTable()
{
    // Clear table
    table->setRowCount(0);
    
    // Add keys to table
    int row = 0;
    for (const auto& [name, value] : keyStore.kv) {
        QString qname = QString::fromStdString(name);
        QString qvalue = QString::fromStdString(value);
        
        // Apply filter if active
        if (!currentFilter.isEmpty()) {
            if (!qname.contains(currentFilter, Qt::CaseInsensitive)) {
                continue;
            }
        }
        
        QString service = detectService(qname);
        QString apiUrl = getServiceApiUrl(service);
        addKeyToTable(qname, qvalue, service, apiUrl);
        row++;
    }
    
    // Update status
    statusLabel->setText(QString("Showing %1 keys").arg(table->rowCount()));
    
    // Update button states
    onSelectionChanged();
}

void KeyManagerWidget::addKeyToTable(const QString &name, const QString &value, const QString &service, const QString &apiUrl)
{
    int row = table->rowCount();
    table->insertRow(row);
    
    // Name column
    QTableWidgetItem *nameItem = new QTableWidgetItem(name);
    nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, ColumnName, nameItem);
    
    // Service column
    QTableWidgetItem *serviceItem = new QTableWidgetItem(service);
    serviceItem->setFlags(serviceItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, ColumnService, serviceItem);
    
    // API URL column
    QTableWidgetItem *urlItem = new QTableWidgetItem(apiUrl);
    urlItem->setFlags(urlItem->flags() & ~Qt::ItemIsEditable);
    urlItem->setToolTip("Click to open API documentation");
    table->setItem(row, ColumnUrl, urlItem);
    
    // Value column (masked)
    MaskedTableItem *valueItem = new MaskedTableItem(value);
    valueItem->setMasked(globalVisibilityState);
    valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, ColumnValue, valueItem);
    
    // Actions column (placeholder)
    QTableWidgetItem *actionsItem = new QTableWidgetItem("••• ");
    actionsItem->setFlags(actionsItem->flags() & ~Qt::ItemIsEditable);
    actionsItem->setTextAlignment(Qt::AlignCenter);
    actionsItem->setToolTip("Right-click for actions");
    table->setItem(row, ColumnActions, actionsItem);
}

QString KeyManagerWidget::detectService(const QString &keyName)
{
    QString lowerName = keyName.toLower();
    
    // Map key patterns to services
    static const QMap<QString, QString> servicePatterns = {
        {"openai", "OpenAI"},
        {"anthropic", "Anthropic"},
        {"google", "Google"},
        {"azure", "Azure"},
        {"aws", "AWS"},
        {"github", "GitHub"},
        {"slack", "Slack"},
        {"discord", "Discord"},
        {"stripe", "Stripe"},
        {"twilio", "Twilio"},
        {"sendgrid", "SendGrid"},
        {"mailgun", "Mailgun"},
        {"cloudflare", "Cloudflare"},
        {"vercel", "Vercel"},
        {"netlify", "Netlify"},
        {"heroku", "Heroku"}
    };
    
    for (auto it = servicePatterns.begin(); it != servicePatterns.end(); ++it) {
        if (lowerName.contains(it.key())) {
            return it.value();
        }
    }
    
    return "Custom";
}

QString KeyManagerWidget::getServiceApiUrl(const QString &service)
{
    static const QMap<QString, QString> serviceUrls = {
        {"OpenAI", "https://api.openai.com"},
        {"Anthropic", "https://api.anthropic.com"},
        {"Google", "https://api.google.com"},
        {"Gemini", "https://generativelanguage.googleapis.com"},
        {"Azure", "https://azure.microsoft.com"},
        {"AWS", "https://aws.amazon.com"},
        {"GitHub", "https://api.github.com"},
        {"Slack", "https://api.slack.com"},
        {"Discord", "https://discord.com/api"},
        {"Stripe", "https://api.stripe.com"},
        {"Twilio", "https://api.twilio.com"},
        {"SendGrid", "https://api.sendgrid.com"},
        {"Mailgun", "https://api.mailgun.net"},
        {"Cloudflare", "https://api.cloudflare.com"},
        {"Vercel", "https://api.vercel.com"},
        {"Netlify", "https://api.netlify.com"},
        {"Heroku", "https://api.heroku.com"},
        {"Groq", "https://api.groq.com"},
        {"Mistral", "https://api.mistral.ai"},
        {"Cohere", "https://api.cohere.ai"},
        {"Brave", "https://api.search.brave.com"},
        {"DeepSeek", "https://api.deepseek.com"},
        {"Exa", "https://api.exa.ai"},
        {"Fireworks", "https://api.fireworks.ai"},
        {"Hugging Face", "https://huggingface.co/api"},
        {"OpenRouter", "https://openrouter.ai/api"},
        {"Perplexity", "https://api.perplexity.ai"},
        {"SambaNova", "https://api.sambanova.ai"},
        {"Tavily", "https://api.tavily.com"},
        {"Together AI", "https://api.together.xyz"},
        {"xAI", "https://api.x.ai"}
    };
    
    auto it = serviceUrls.find(service);
    return it != serviceUrls.end() ? it.value() : "";
}

void KeyManagerWidget::refreshKeys()
{
    loadKeys();
    updateTable();
    showSuccess("Keys refreshed");
}

void KeyManagerWidget::selectKey(const QString &keyName)
{
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->item(row, ColumnName)->text() == keyName) {
            table->selectRow(row);
            table->scrollToItem(table->item(row, ColumnName));
            break;
        }
    }
}

void KeyManagerWidget::addKey()
{
    KeyEditDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.getKeyName();
        QString value = dialog.getKeyValue();
        
        // Check if key already exists
        if (keyStore.kv.find(name.toStdString()) != keyStore.kv.end()) {
            showError("Key with this name already exists!");
            return;
        }
        
        // Add to keystore
        keyStore.kv[name.toStdString()] = value.toStdString();
        saveKeys();
        updateTable();
        selectKey(name);
        
        showSuccess(QString("Added key: %1").arg(name));
    }
}

void KeyManagerWidget::editKey()
{
    int row = table->currentRow();
    if (row < 0) return;
    
    QString name = table->item(row, ColumnName)->text();
    MaskedTableItem *valueItem = dynamic_cast<MaskedTableItem*>(table->item(row, ColumnValue));
    QString service = table->item(row, ColumnService)->text();
    
    if (!valueItem) return;
    
    KeyEditDialog dialog(name, valueItem->getActualValue(), service, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString newValue = dialog.getKeyValue();
        
        // Update keystore
        keyStore.kv[name.toStdString()] = newValue.toStdString();
        saveKeys();
        
        // Update table item
        valueItem = new MaskedTableItem(newValue);
        valueItem->setMasked(globalVisibilityState);
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, ColumnValue, valueItem);
        
        showSuccess(QString("Updated key: %1").arg(name));
    }
}

void KeyManagerWidget::deleteKey()
{
    int row = table->currentRow();
    if (row < 0) return;
    
    QString name = table->item(row, ColumnName)->text();
    
    if (ConfirmationDialog::confirm(this, "Delete Key", 
        QString("Are you sure you want to delete the key '%1'?").arg(name),
        "This action cannot be undone.")) {
        
        // Remove from keystore
        keyStore.kv.erase(name.toStdString());
        saveKeys();
        updateTable();
        
        showSuccess(QString("Deleted key: %1").arg(name));
    }
}

void KeyManagerWidget::searchKeys(const QString &text)
{
    currentFilter = text.trimmed();
    updateTable();
}

void KeyManagerWidget::toggleKeyVisibility()
{
    globalVisibilityState = !globalVisibilityState;
    
    // Update all masked items
    for (int row = 0; row < table->rowCount(); ++row) {
        MaskedTableItem *item = dynamic_cast<MaskedTableItem*>(table->item(row, ColumnValue));
        if (item) {
            item->setMasked(globalVisibilityState);
        }
    }
    
    // Update button text
    toggleVisibilityButton->setText(globalVisibilityState ? "Show All" : "Hide All");
    toggleVisibilityButton->setChecked(!globalVisibilityState);
}

void KeyManagerWidget::showContextMenu(const QPoint &pos)
{
    QTableWidgetItem *item = table->itemAt(pos);
    if (!item) return;
    
    int row = item->row();
    bool hasSelection = row >= 0;
    
    editAction->setEnabled(hasSelection);
    deleteAction->setEnabled(hasSelection);
    copyNameAction->setEnabled(hasSelection);
    copyValueAction->setEnabled(hasSelection);
    toggleVisibilityAction->setEnabled(hasSelection);
    
    contextMenu->exec(table->mapToGlobal(pos));
}

void KeyManagerWidget::onTableItemChanged(QTableWidgetItem *item)
{
    // Currently all items are read-only, so this shouldn't be called
    Q_UNUSED(item)
}

void KeyManagerWidget::onSelectionChanged()
{
    bool hasSelection = table->currentRow() >= 0;
    editButton->setEnabled(hasSelection);
    deleteButton->setEnabled(hasSelection);
}

bool KeyManagerWidget::validateKeyName(const QString &name)
{
    if (name.isEmpty()) return false;
    
    // Check for valid environment variable name format
    QRegularExpression regex("^[A-Z][A-Z0-9_]*$");
    return regex.match(name).hasMatch();
}

void KeyManagerWidget::showError(const QString &message)
{
    statusLabel->setText(QString("Error: %1").arg(message));
    statusLabel->setStyleSheet("color: #ff4444; font-size: 12px;");
    
    // Reset to normal after 5 seconds
    QTimer::singleShot(5000, [this]() {
        statusLabel->setText("Ready");
        statusLabel->setStyleSheet("color: #666; font-size: 12px;");
    });
    
    emit statusMessage(QString("Error: %1").arg(message));
}

void KeyManagerWidget::showSuccess(const QString &message)
{
    statusLabel->setText(message);
    statusLabel->setStyleSheet("color: #00aa00; font-size: 12px;");
    
    // Reset to normal after 3 seconds
    QTimer::singleShot(3000, [this]() {
        statusLabel->setText("Ready");
        statusLabel->setStyleSheet("color: #666; font-size: 12px;");
    });
    
    emit statusMessage(message);
}

void KeyManagerWidget::testSelectedKey()
{
    int row = table->currentRow();
    if (row < 0) return;
    
    QString keyName = table->item(row, ColumnName)->text();
    QString serviceName = table->item(row, ColumnService)->text();
    
    // Map service display name to service code
    QString serviceCode = getServiceCode(serviceName);
    if (serviceCode.isEmpty()) {
        showError("Cannot test: Service not recognized");
        return;
    }
    
    // Disable button during test
    testSelectedButton->setEnabled(false);
    statusLabel->setText("Testing " + serviceName + "...");
    statusLabel->setStyleSheet("color: #0066cc; font-size: 12px;");
    
    // Run test in separate thread to avoid blocking UI
    QTimer::singleShot(100, [this, serviceCode, serviceName]() {
        try {
            auto result = ak::services::test_one(config, serviceCode.toStdString());
            
            if (result.ok) {
                showSuccess(QString("%1 test passed! (%2ms)").arg(serviceName).arg(result.duration.count()));
            } else {
                QString error = result.error_message.empty() ? "Test failed" : QString::fromStdString(result.error_message);
                showError(QString("%1 test failed: %2").arg(serviceName).arg(error));
            }
        } catch (const std::exception& e) {
            showError(QString("%1 test failed: %2").arg(serviceName).arg(e.what()));
        }
        
        testSelectedButton->setEnabled(table->currentRow() >= 0);
    });
}

void KeyManagerWidget::testAllKeys()
{
    // Get all configured services
    auto configuredServices = ak::services::detectConfiguredServices(config);
    if (configuredServices.empty()) {
        showError("No API keys configured for testing");
        return;
    }
    
    // Disable buttons during test
    testAllButton->setEnabled(false);
    testSelectedButton->setEnabled(false);
    
    statusLabel->setText(QString("Testing %1 services...").arg(configuredServices.size()));
    statusLabel->setStyleSheet("color: #0066cc; font-size: 12px;");
    
    // Run tests
    QTimer::singleShot(100, [this, configuredServices]() {
        try {
            auto results = ak::services::run_tests_parallel(config, configuredServices, false);
            
            int passed = 0;
            int failed = 0;
            for (const auto& result : results) {
                if (result.ok) passed++;
                else failed++;
            }
            
            if (failed == 0) {
                showSuccess(QString("All %1 API tests passed!").arg(passed));
            } else {
                showError(QString("Tests completed: %1 passed, %2 failed").arg(passed).arg(failed));
            }
        } catch (const std::exception& e) {
            showError(QString("Test failed: %1").arg(e.what()));
        }
        
        testAllButton->setEnabled(true);
        testSelectedButton->setEnabled(table->currentRow() >= 0);
    });
}

QString KeyManagerWidget::getServiceCode(const QString &displayName)
{
    static const QMap<QString, QString> serviceCodes = {
        {"OpenAI", "openai"},
        {"Anthropic", "anthropic"},
        {"Google", "gemini"},
        {"Gemini", "gemini"},
        {"Azure", "azure_openai"},
        {"AWS", "aws"},
        {"GitHub", "github"},
        {"Slack", "slack"},
        {"Discord", "discord"},
        {"Stripe", "stripe"},
        {"Twilio", "twilio"},
        {"SendGrid", "sendgrid"},
        {"Mailgun", "mailgun"},
        {"Cloudflare", "cloudflare"},
        {"Vercel", "vercel"},
        {"Netlify", "netlify"},
        {"Heroku", "heroku"},
        {"Groq", "groq"},
        {"Mistral", "mistral"},
        {"Cohere", "cohere"},
        {"Brave", "brave"},
        {"DeepSeek", "deepseek"},
        {"Exa", "exa"},
        {"Fireworks", "fireworks"},
        {"Hugging Face", "huggingface"},
        {"OpenRouter", "openrouter"},
        {"Perplexity", "perplexity"},
        {"SambaNova", "sambanova"},
        {"Tavily", "tavily"},
        {"Together AI", "together"},
        {"xAI", "xai"}
    };
    
    auto it = serviceCodes.find(displayName);
    return it != serviceCodes.end() ? it.value() : "";
}

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI