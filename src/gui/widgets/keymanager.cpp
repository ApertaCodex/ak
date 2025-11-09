#ifdef BUILD_GUI

#include "gui/widgets/keymanager.hpp"
#include "gui/widgets/common/dialogs.hpp"
#include "gui/widgets/common/secureinput.hpp"
#include "gui/widgets/servicehelpers.hpp"
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
#include <QPushButton>
#include <QUrl>
#include <QThread>
#include <QMetaType>
#include <QFile>
#include <vector>
#include <algorithm>
#include <map>
#include <filesystem>

namespace ak {
namespace gui {
namespace widgets {

// ProfileKeysLoaderWorker Implementation
ProfileKeysLoaderWorker::ProfileKeysLoaderWorker(const core::Config& cfg, const QString& profileName, const QString& passphrase, QObject *parent)
    : QObject(parent), config(cfg), profileName(profileName), passphrase(passphrase.toStdString())
{
}

void ProfileKeysLoaderWorker::loadKeys()
{
    try {
        core::Config cfg = config;
        if (!passphrase.empty()) {
            cfg.presetPassphrase = passphrase;
        }
        std::map<std::string, std::string> keys = ak::storage::loadProfileKeys(cfg, profileName.toStdString());
        emit keysLoaded(profileName, keys);
    } catch (const std::exception& e) {
        emit loadFailed(profileName, QString::fromStdString(e.what()));
    }
}

// ProfileKeysSaverWorker Implementation
ProfileKeysSaverWorker::ProfileKeysSaverWorker(const core::Config& cfg,
                                               const QString& profileName,
                                               const std::map<std::string, std::string>& keys,
                                               const QString& passphrase,
                                               QObject *parent)
    : QObject(parent), config(cfg), profileName(profileName), keysToSave(keys), passphrase(passphrase.toStdString())
{
}

void ProfileKeysSaverWorker::saveKeys()
{
    try {
        core::Config cfg = config;
        if (!passphrase.empty()) {
            cfg.presetPassphrase = passphrase;
        }
        ak::storage::saveProfileKeys(cfg, profileName.toStdString(), keysToSave);
        emit saveCompleted(profileName);
    } catch (const std::exception& e) {
        emit saveFailed(profileName, QString::fromStdString(e.what()));
    }
}

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
    : QWidget(parent), config(config), currentProfile("default"),
      profileKeys(), cachedProfileKeys(), keysModified(false),
      loadingInProgress(false), savingInProgress(false),
      sessionPassphrase(), rememberedPassphrase(), rememberPassphrase(false),
      mainLayout(nullptr), toolbarLayout(nullptr),
      profileCombo(nullptr), profileActionsButton(nullptr), profileActionsMenu(nullptr),
      searchEdit(nullptr), addButton(nullptr), editButton(nullptr), deleteButton(nullptr),
      toggleVisibilityButton(nullptr), refreshButton(nullptr), statusLabel(nullptr),
      table(nullptr), contextMenu(nullptr), addAction(nullptr), editAction(nullptr),
      deleteAction(nullptr), copyNameAction(nullptr), copyValueAction(nullptr),
      toggleVisibilityAction(nullptr), globalVisibilityState(true), currentFilter(),
      loadKeysThread(nullptr), loadKeysWorker(nullptr),
      saveKeysThread(nullptr), saveKeysWorker(nullptr)
{
    qRegisterMetaType<std::map<std::string, std::string>>("std::map<std::string, std::string>");

    // Ensure default profile exists
    ak::storage::ensureDefaultProfile(config);
    
    setupUi();
    refreshProfileList();
    loadKeys();
    updateTable();
    updateButtonStates();
}

KeyManagerWidget::~KeyManagerWidget()
{
    cleanupLoadThread(true);
    cleanupSaveThread(true);
    clearPassphrase();
    loadingInProgress = false;
    savingInProgress = false;
}

void KeyManagerWidget::cleanupLoadThread(bool waitForFinish)
{
    if (!loadKeysThread) {
        return;
    }

    if (loadKeysThread->isRunning()) {
        loadKeysThread->quit();
        if (waitForFinish) {
            loadKeysThread->wait();
        } else {
            return;
        }
    }

    if (loadKeysWorker) {
        delete loadKeysWorker;
        loadKeysWorker = nullptr;
    }

    delete loadKeysThread;
    loadKeysThread = nullptr;
}

void KeyManagerWidget::cleanupSaveThread(bool waitForFinish)
{
    if (!saveKeysThread) {
        return;
    }

    if (saveKeysThread->isRunning()) {
        saveKeysThread->quit();
        if (waitForFinish) {
            saveKeysThread->wait();
        } else {
            return;
        }
    }

    if (saveKeysWorker) {
        delete saveKeysWorker;
        saveKeysWorker = nullptr;
    }

    delete saveKeysThread;
    saveKeysThread = nullptr;
}

QString KeyManagerWidget::currentPassphrase() const
{
    return rememberPassphrase ? rememberedPassphrase : sessionPassphrase;
}

bool KeyManagerWidget::ensurePassphrase(bool requireConfirmation, QString &outPassphrase)
{
    QString current = currentPassphrase();
    if (!current.isEmpty()) {
        outPassphrase = current;
        return true;
    }

    PassphrasePromptDialog dialog(requireConfirmation, config.gpgAvailable && !config.forcePlain, this);
    if (dialog.exec() != QDialog::Accepted) {
        outPassphrase.clear();
        return false;
    }

    QString passphrase = dialog.passphrase();
    bool remember = dialog.rememberForSession();
    setCurrentPassphrase(passphrase, remember);
    outPassphrase = passphrase;
    return true;
}

void KeyManagerWidget::setCurrentPassphrase(const QString &passphrase, bool remember)
{
    sessionPassphrase = passphrase;
    if (remember) {
        rememberedPassphrase = passphrase;
        rememberPassphrase = true;
    } else {
        if (!rememberPassphrase) {
            rememberedPassphrase.clear();
        }
        rememberPassphrase = false;
    }
}

void KeyManagerWidget::clearPassphrase()
{
    sessionPassphrase.clear();
    rememberedPassphrase.clear();
    rememberPassphrase = false;
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
    
    // Profile selector
    profileCombo = new QComboBox(this);
    profileCombo->setMaximumWidth(150);
    profileCombo->setToolTip("Select profile to manage keys");
    connect(profileCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            this, &KeyManagerWidget::onProfileChanged);
    
    // Profile actions menu button
    profileActionsButton = new QPushButton("⋯", this);
    profileActionsButton->setToolTip("Profile actions");
    profileActionsButton->setMaximumWidth(30);
    profileActionsButton->setFlat(true);
    profileActionsMenu = new QMenu(this);
    
    QAction *createAction = profileActionsMenu->addAction("Create Profile");
    createAction->setIcon(QIcon::fromTheme("document-new", QIcon(":/icons/add.svg")));
    connect(createAction, &QAction::triggered, this, &KeyManagerWidget::createProfile);
    
    QAction *renameAction = profileActionsMenu->addAction("Rename Profile");
    renameAction->setIcon(QIcon::fromTheme("document-edit", QIcon(":/icons/edit.svg")));
    connect(renameAction, &QAction::triggered, this, &KeyManagerWidget::renameProfile);
    
    QAction *duplicateAction = profileActionsMenu->addAction("Duplicate Profile");
    duplicateAction->setIcon(QIcon::fromTheme("edit-copy", QIcon(":/icons/add.svg")));
    connect(duplicateAction, &QAction::triggered, this, &KeyManagerWidget::duplicateProfile);
    
    profileActionsMenu->addSeparator();
    
    QAction *deleteAction = profileActionsMenu->addAction("Delete Profile");
    deleteAction->setIcon(QIcon::fromTheme("edit-delete", QIcon(":/icons/delete.svg")));
    connect(deleteAction, &QAction::triggered, this, &KeyManagerWidget::deleteProfile);
    
    profileActionsMenu->addSeparator();
    
    QAction *importAction = profileActionsMenu->addAction("Import Profile");
    importAction->setIcon(QIcon::fromTheme("document-import", QIcon(":/icons/import.svg")));
    connect(importAction, &QAction::triggered, this, &KeyManagerWidget::importProfile);
    
    QAction *exportAction = profileActionsMenu->addAction("Export Profile");
    exportAction->setIcon(QIcon::fromTheme("document-export", QIcon(":/icons/export.svg")));
    connect(exportAction, &QAction::triggered, this, &KeyManagerWidget::exportProfile);
    
    profileActionsButton->setMenu(profileActionsMenu);
    
    // Search box
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Search keys...");
    searchEdit->setMaximumWidth(200);
    connect(searchEdit, &QLineEdit::textChanged, this, &KeyManagerWidget::searchKeys);
    
    // Buttons
    addButton = new QPushButton("Add Key", this);
    addButton->setIcon(QIcon::fromTheme("list-add", QIcon(":/icons/add.svg")));
    connect(addButton, &QPushButton::clicked, this, &KeyManagerWidget::addKey);
    
    editButton = new QPushButton("Edit", this);
    editButton->setIcon(QIcon::fromTheme("document-edit", QIcon(":/icons/edit.svg")));
    editButton->setEnabled(false);
    connect(editButton, &QPushButton::clicked, this, &KeyManagerWidget::editKey);
    
    deleteButton = new QPushButton("Delete", this);
    deleteButton->setIcon(QIcon::fromTheme("edit-delete", QIcon(":/icons/delete.svg")));
    deleteButton->setEnabled(false);
    connect(deleteButton, &QPushButton::clicked, this, &KeyManagerWidget::deleteKey);
    
    toggleVisibilityButton = new QPushButton("Show All", this);
    toggleVisibilityButton->setCheckable(true);
    toggleVisibilityButton->setIcon(QIcon::fromTheme("view-visible", QIcon(":/icons/eye.svg")));
    toggleVisibilityButton->setText("Show All");
    toggleVisibilityButton->setToolTip("Toggle visibility of all key values");
    connect(toggleVisibilityButton, &QPushButton::clicked, this, &KeyManagerWidget::toggleKeyVisibility);
    
    refreshButton = new QPushButton("Refresh", this);
    refreshButton->setIcon(QIcon::fromTheme("view-refresh", QIcon(":/icons/refresh.svg")));
    connect(refreshButton, &QPushButton::clicked, this, &KeyManagerWidget::refreshKeys);
    
    // Test buttons
    testSelectedButton = new QPushButton("Test", this);
    testSelectedButton->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/icons/play.svg")));
    testSelectedButton->setEnabled(false);
    connect(testSelectedButton, &QPushButton::clicked, this, &KeyManagerWidget::testSelectedKey);
    
    testAllButton = new QPushButton("Test All", this);
    testAllButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    connect(testAllButton, &QPushButton::clicked, this, &KeyManagerWidget::testAllKeys);
    
    // Layout
    toolbarLayout->addWidget(new QLabel("Profile:", this));
    toolbarLayout->addWidget(profileCombo);
    toolbarLayout->addWidget(profileActionsButton);
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
    
    QStringList headers = {"Key Name", "Service", "API URL", "Value", "Test Status", "Actions"};
    table->setHorizontalHeaderLabels(headers);
    
    // Configure table appearance
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setSortingEnabled(true);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // Disable sorting for actions column specifically
    QHeaderView *headerForSorting = table->horizontalHeader();
    connect(headerForSorting, &QHeaderView::sortIndicatorChanged, [this](int logicalIndex, Qt::SortOrder) {
        if (logicalIndex == ColumnActions) {
            // Prevent sorting on Actions column
            table->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
        }
    });
    
    // Configure column widths - responsive design
    QHeaderView *header = table->horizontalHeader();
    header->setSectionResizeMode(ColumnName, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ColumnService, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ColumnUrl, QHeaderView::Interactive);
    header->setSectionResizeMode(ColumnValue, QHeaderView::Stretch);
    header->setSectionResizeMode(ColumnTestStatus, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ColumnActions, QHeaderView::Fixed);
    
    // Set fixed width for actions column
    header->resizeSection(ColumnActions, 60);
    
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
    loadProfileKeys(currentProfile);
}

void KeyManagerWidget::saveKeys()
{
    saveProfileKeys(currentProfile);
}

void KeyManagerWidget::loadProfileKeys(const QString &profileName)
{
    if (loadingInProgress && loadKeysThread && loadKeysThread->isRunning()) {
        statusLabel->setText(QString("Waiting for profile '%1' to finish loading...").arg(profileName));
        statusLabel->setStyleSheet("color: #cc6600; font-size: 12px;");
        return;
    }

    cleanupLoadThread(false);

    // Check cache first
    auto cachedIt = cachedProfileKeys.find(profileName);
    if (cachedIt != cachedProfileKeys.end()) {
        profileKeys = cachedIt->second;
        loadingInProgress = false;
        updateTable();
        statusLabel->setText(QString("Loaded %1 keys from profile '%2'").arg(profileKeys.size()).arg(profileName));
        statusLabel->setStyleSheet("color: #666; font-size: 12px;");
        updateButtonStates();
        return;
    }

    bool passRequired = config.gpgAvailable && !config.forcePlain;
    std::filesystem::path keysPath = ak::storage::profileKeysPath(config, profileName.toStdString());
    bool encryptedFileExists = passRequired && std::filesystem::exists(keysPath) && keysPath.extension() == ".gpg";

    QString passphrase;
    if (encryptedFileExists) {
        if (!ensurePassphrase(false, passphrase)) {
            statusLabel->setText("Load cancelled");
            statusLabel->setStyleSheet("color: #cc6600; font-size: 12px;");
            loadingInProgress = false;
            updateButtonStates();
            return;
        }
    }

    // Show loading status
    statusLabel->setText(QString("Loading keys from profile '%1'...").arg(profileName));
    statusLabel->setStyleSheet("color: #0066cc; font-size: 12px;");

    loadingInProgress = true;
    updateButtonStates();

    // Create new worker and thread
    loadKeysThread = new QThread();
    loadKeysWorker = new ProfileKeysLoaderWorker(config, profileName, passphrase);
    loadKeysWorker->moveToThread(loadKeysThread);

    // Connect signals
    connect(loadKeysThread, &QThread::started, loadKeysWorker, &ProfileKeysLoaderWorker::loadKeys);
    connect(loadKeysWorker, &ProfileKeysLoaderWorker::keysLoaded, this, &KeyManagerWidget::onKeysLoaded);
    connect(loadKeysWorker, &ProfileKeysLoaderWorker::loadFailed, this, &KeyManagerWidget::onKeysLoadFailed);
    connect(loadKeysWorker, &ProfileKeysLoaderWorker::keysLoaded, loadKeysThread, &QThread::quit);
    connect(loadKeysWorker, &ProfileKeysLoaderWorker::loadFailed, loadKeysThread, &QThread::quit);
    connect(loadKeysThread, &QThread::finished, this, [this]() {
        if (loadKeysWorker) {
            loadKeysWorker->deleteLater();
            loadKeysWorker = nullptr;
        }
        if (loadKeysThread) {
            loadKeysThread->deleteLater();
            loadKeysThread = nullptr;
        }
    });

    // Start the thread
    loadKeysThread->start();
}

void KeyManagerWidget::onKeysLoaded(const QString& profileName, const std::map<std::string, std::string>& keys)
{
    profileKeys = keys;
    cachedProfileKeys[profileName] = profileKeys;
    keysModified = false;
    loadingInProgress = false;

    updateTable();

    emit statusMessage(QString("Loaded %1 keys from profile '%2'").arg(profileKeys.size()).arg(profileName));
    statusLabel->setStyleSheet("color: #666; font-size: 12px;");
    updateButtonStates();
}

void KeyManagerWidget::onKeysLoadFailed(const QString& profileName, const QString& error)
{
    profileKeys.clear();
    cachedProfileKeys[profileName] = profileKeys;
    keysModified = false;
    loadingInProgress = false;

    updateTable();

    showError(QString("Failed to load keys from profile '%1': %2").arg(profileName).arg(error));
    updateButtonStates();
}

void KeyManagerWidget::saveProfileKeys(const QString &profileName)
{
    if (savingInProgress && saveKeysThread && saveKeysThread->isRunning()) {
        statusLabel->setText(QString("Saving keys for profile '%1'... Please wait.").arg(profileName));
        statusLabel->setStyleSheet("color: #cc6600; font-size: 12px;");
        return;
    }

    cleanupSaveThread(false);

    bool passRequired = config.gpgAvailable && !config.forcePlain;
    QString passphrase;
    if (passRequired) {
        bool needConfirmation = currentPassphrase().isEmpty();
        if (!ensurePassphrase(needConfirmation, passphrase)) {
            statusLabel->setText("Save cancelled");
            statusLabel->setStyleSheet("color: #cc6600; font-size: 12px;");
            savingInProgress = false;
            updateButtonStates();
            return;
        }
    }

    statusLabel->setText(QString("Saving keys to profile '%1'...").arg(profileName));
    statusLabel->setStyleSheet("color: #0066cc; font-size: 12px;");

    savingInProgress = true;
    updateButtonStates();

    saveKeysThread = new QThread();
    saveKeysWorker = new ProfileKeysSaverWorker(config, profileName, profileKeys, passphrase);
    saveKeysWorker->moveToThread(saveKeysThread);

    connect(saveKeysThread, &QThread::started, saveKeysWorker, &ProfileKeysSaverWorker::saveKeys);
    connect(saveKeysWorker, &ProfileKeysSaverWorker::saveCompleted, this, &KeyManagerWidget::onKeysSaved);
    connect(saveKeysWorker, &ProfileKeysSaverWorker::saveFailed, this, &KeyManagerWidget::onKeysSaveFailed);
    connect(saveKeysWorker, &ProfileKeysSaverWorker::saveCompleted, saveKeysThread, &QThread::quit);
    connect(saveKeysWorker, &ProfileKeysSaverWorker::saveFailed, saveKeysThread, &QThread::quit);
    connect(saveKeysThread, &QThread::finished, this, [this]() {
        if (saveKeysWorker) {
            saveKeysWorker->deleteLater();
            saveKeysWorker = nullptr;
        }
        if (saveKeysThread) {
            saveKeysThread->deleteLater();
            saveKeysThread = nullptr;
        }
    });

    saveKeysThread->start();
}

void KeyManagerWidget::onKeysSaved(const QString& profileName)
{
    savingInProgress = false;
    keysModified = false;
    cachedProfileKeys[profileName] = profileKeys;

    try {
        std::vector<std::string> keyNames;
        keyNames.reserve(profileKeys.size());
        for (const auto& [key, value] : profileKeys) {
            keyNames.push_back(key);
        }
        ak::storage::writeProfile(config, profileName.toStdString(), keyNames);
        showSuccess(QString("Keys saved to profile '%1'").arg(profileName));
    } catch (const std::exception& e) {
        showError(QString("Keys saved but failed to update profile index: %1").arg(e.what()));
    }

    emit statusMessage(QString("Keys saved to profile '%1'").arg(profileName));
    updateButtonStates();
}

void KeyManagerWidget::onKeysSaveFailed(const QString& profileName, const QString& error)
{
    savingInProgress = false;
    showError(QString("Failed to save keys to profile '%1': %2").arg(profileName).arg(error));
    updateButtonStates();
}

void KeyManagerWidget::updateButtonStates()
{
    bool busy = loadingInProgress || savingInProgress;
    bool hasSelection = table && table->currentRow() >= 0;

    if (addButton) addButton->setEnabled(!busy);
    if (refreshButton) refreshButton->setEnabled(!busy && !loadingInProgress);
    if (editButton) editButton->setEnabled(!busy && hasSelection);
    if (deleteButton) deleteButton->setEnabled(!busy && hasSelection);
    if (testSelectedButton) testSelectedButton->setEnabled(!busy && hasSelection);
    if (testAllButton) testAllButton->setEnabled(!busy);
}

void KeyManagerWidget::refreshProfileList()
{
    try {
        auto profiles = ak::storage::listProfiles(config);
        
        profileCombo->clear();
        for (const auto& profile : profiles) {
            profileCombo->addItem(QString::fromStdString(profile));
        }
        
        // Set current profile or default
        int index = profileCombo->findText(currentProfile);
        if (index >= 0) {
            profileCombo->setCurrentIndex(index);
        } else if (profileCombo->count() > 0) {
            currentProfile = profileCombo->itemText(0);
            profileCombo->setCurrentIndex(0);
        }
        
    } catch (const std::exception& e) {
        showError(QString("Failed to load profiles: %1").arg(e.what()));
    }
}

void KeyManagerWidget::onProfileChanged(const QString &profileName)
{
    if (profileName != currentProfile) {
        // Only save current profile keys if they've been modified
        if (!currentProfile.isEmpty() && keysModified) {
            saveProfileKeys(currentProfile);
            keysModified = false;
        }
        
        currentProfile = profileName;
        // Load keys (will use cache if available, otherwise load in background)
        loadProfileKeys(currentProfile);
        
        emit profileChanged(profileName);
    }
}

void KeyManagerWidget::setCurrentProfile(const QString &profileName)
{
    // Skip if it's the same profile
    if (profileName == currentProfile) {
        return;
    }
    
    // Block signals to prevent triggering onProfileChanged multiple times
    profileCombo->blockSignals(true);
    
    int index = profileCombo->findText(profileName);
    if (index >= 0) {
        profileCombo->setCurrentIndex(index);
    } else if (!profileName.isEmpty()) {
        // Profile not found in combo, refresh the list and try again
        refreshProfileList();
        index = profileCombo->findText(profileName);
        if (index >= 0) {
            profileCombo->setCurrentIndex(index);
        }
    }
    
    profileCombo->blockSignals(false);
    
    // Manually trigger profile change (we already checked it's different above)
    if (!profileName.isEmpty()) {
        onProfileChanged(profileName);
    }
}

QString KeyManagerWidget::getCurrentProfile() const
{
    return currentProfile;
}

void KeyManagerWidget::updateTable()
{
    // Clear table
    table->setRowCount(0);
    
    // Add keys to table
    int row = 0;
    for (const auto& [name, value] : profileKeys) {
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
    statusLabel->setText(QString("Showing %1 keys from profile '%2'").arg(table->rowCount()).arg(currentProfile));
    
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
    QTableWidgetItem *urlItem = new QTableWidgetItem(apiUrl.isEmpty() ? "N/A" : apiUrl);
    urlItem->setFlags(urlItem->flags() & ~Qt::ItemIsEditable);
    if (!apiUrl.isEmpty()) {
        urlItem->setToolTip("API Endpoint: " + apiUrl);
    }
    table->setItem(row, ColumnUrl, urlItem);
    
    // Value column (masked)
    MaskedTableItem *valueItem = new MaskedTableItem(value);
    valueItem->setMasked(globalVisibilityState);
    valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, ColumnValue, valueItem);
    
    // Test Status column
    QTableWidgetItem *testStatusItem = new QTableWidgetItem("Not tested");
    testStatusItem->setFlags(testStatusItem->flags() & ~Qt::ItemIsEditable);
    testStatusItem->setTextAlignment(Qt::AlignCenter);
    table->setItem(row, ColumnTestStatus, testStatusItem);
    
    // Actions column - make it clickable for context menu
    QPushButton *actionsButton = new QPushButton();
    QIcon menuIcon = QIcon::fromTheme("application-menu", QIcon(":/icons/menu.svg"));
    actionsButton->setIcon(menuIcon);
    actionsButton->setToolTip("Click for actions");
    actionsButton->setMaximumSize(30, 25);
    actionsButton->setFlat(true);
    connect(actionsButton, &QPushButton::clicked, [this, row, actionsButton]() {
        table->selectRow(row);
        // Get the global position of the button to show menu nearby
        QPoint globalPos = actionsButton->mapToGlobal(QPoint(actionsButton->width()/2, actionsButton->height()));
        contextMenu->exec(globalPos);
    });
    table->setCellWidget(row, ColumnActions, actionsButton);
}

QString KeyManagerWidget::detectService(const QString &keyName)
{
    QString lowerName = keyName.toLower();
    
    // First, check actual loaded services from config (handles custom services)
    try {
        auto allServices = ak::services::loadAllServices(config);
        for (const auto& [serviceName, service] : allServices) {
            if (QString::fromStdString(service.keyName).compare(keyName, Qt::CaseInsensitive) == 0) {
                // Use description if available, otherwise use service name
                QString displayName = QString::fromStdString(service.description);
                if (displayName.isEmpty()) {
                    displayName = QString::fromStdString(service.name);
                    // Capitalize first letter
                    if (!displayName.isEmpty()) {
                        displayName[0] = displayName[0].toUpper();
                    }
                }
                return displayName;
            }
        }
    } catch (const std::exception&) {
        // Fall back to pattern matching if loading services fails
    }
    
    // Map key patterns to services - more comprehensive mapping (fallback)
    static const QMap<QString, QString> servicePatterns = {
        {"openai", "OpenAI"},
        {"anthropic", "Anthropic"},
        {"google", "Google"},
        {"gemini", "Gemini"},
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
        {"heroku", "Heroku"},
        {"groq", "Groq"},
        {"mistral", "Mistral"},
        {"cohere", "Cohere"},
        {"brave", "Brave"},
        {"deepseek", "DeepSeek"},
        {"exa", "Exa"},
        {"fireworks", "Fireworks"},
        {"huggingface", "Hugging Face"},
        {"hugging_face", "Hugging Face"},
        {"openrouter", "OpenRouter"},
        {"perplexity", "Perplexity"},
        {"sambanova", "SambaNova"},
        {"tavily", "Tavily"},
        {"together", "Together AI"},
        {"xai", "xAI"},
        {"ollama", "Ollama"}
    };
    
    // Check each pattern for better matching
    for (auto it = servicePatterns.begin(); it != servicePatterns.end(); ++it) {
        if (lowerName.contains(it.key())) {
            return it.value();
        }
    }
    
    return "Custom";
}

QString KeyManagerWidget::getServiceApiUrl(const QString &service)
{
    // First, check actual loaded services from config (handles custom services)
    try {
        auto allServices = ak::services::loadAllServices(config);
        for (const auto& [serviceName, serviceObj] : allServices) {
            QString serviceDisplayName = QString::fromStdString(serviceObj.description);
            if (serviceDisplayName.isEmpty()) {
                QString name = QString::fromStdString(serviceObj.name);
                if (!name.isEmpty()) {
                    name[0] = name[0].toUpper();
                }
                serviceDisplayName = name;
            }
            
            // Check if display name matches
            if (service.compare(serviceDisplayName, Qt::CaseInsensitive) == 0 ||
                service.compare(QString::fromStdString(serviceObj.name), Qt::CaseInsensitive) == 0) {
                // Extract base URL from test endpoint if available
                if (!serviceObj.testEndpoint.empty()) {
                    QString endpoint = QString::fromStdString(serviceObj.testEndpoint);
                    // Extract base URL (remove path)
                    QUrl url(endpoint);
                    if (url.isValid()) {
                        return url.scheme() + "://" + url.host() + (url.port() != -1 ? ":" + QString::number(url.port()) : "");
                    }
                }
            }
        }
    } catch (const std::exception&) {
        // Fall back to static mapping if loading services fails
    }
    
    // Static mapping for built-in services (fallback)
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
        {"xAI", "https://api.x.ai"},
        {"Ollama", "http://localhost:11434"}
    };
    
    auto it = serviceUrls.find(service);
    return it != serviceUrls.end() ? it.value() : "";
}

void KeyManagerWidget::refreshKeys()
{
    // Clear cache for current profile to force reload
    cachedProfileKeys.erase(currentProfile);
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
    KeyEditDialog dialog(config, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.getKeyName();
        QString value = dialog.getKeyValue();
        
        // Check if key already exists
        if (profileKeys.find(name.toStdString()) != profileKeys.end()) {
            showError("Key with this name already exists!");
            return;
        }
        
        // Add to profile keys
        profileKeys[name.toStdString()] = value.toStdString();
        keysModified = true;
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
    
    KeyEditDialog dialog(config, name, valueItem->getActualValue(), service, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString newValue = dialog.getKeyValue();
        
        // Update keystore
        profileKeys[name.toStdString()] = newValue.toStdString();
        keysModified = true;
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
        profileKeys.erase(name.toStdString());
        keysModified = true;
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
    
    // Update button text and icon
    toggleVisibilityButton->setIcon(globalVisibilityState
        ? QIcon::fromTheme("view-visible", QIcon(":/icons/eye.svg"))
        : QIcon::fromTheme("view-hidden", QIcon(":/icons/eye-off.svg")));
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
    updateButtonStates();
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
        updateTestStatus(keyName, false, "Service not recognized");
        showError("Cannot test: Service not recognized");
        return;
    }
    
    // Show testing status
    updateTestStatus(keyName, false, "Testing...");
    
    // Disable button during test
    testSelectedButton->setEnabled(false);
    statusLabel->setText("Testing " + serviceName + "...");
    statusLabel->setStyleSheet("color: #0066cc; font-size: 12px;");
    
    // Run test in separate thread to avoid blocking UI
    QTimer::singleShot(100, [this, serviceCode, serviceName, keyName]() {
        try {
            auto result = ak::services::test_one(config, serviceCode.toStdString());
            
            if (result.ok) {
                updateTestStatus(keyName, true, QString("? %1ms").arg(result.duration.count()));
                showSuccess(QString("%1 test passed! (%2ms)").arg(serviceName).arg(result.duration.count()));
            } else {
                QString error = result.error_message.empty() ? "Test failed" : QString::fromStdString(result.error_message);
                updateTestStatus(keyName, false, "? " + error);
                showError(QString("%1 test failed: %2").arg(serviceName).arg(error));
            }
        } catch (const std::exception& e) {
            updateTestStatus(keyName, false, QString("? %1").arg(e.what()));
            showError(QString("%1 test failed: %2").arg(serviceName).arg(e.what()));
        }
        
        testSelectedButton->setEnabled(table->currentRow() >= 0);
    });
}

void KeyManagerWidget::updateTestStatus(const QString &keyName, bool success, const QString &message)
{
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->item(row, ColumnName)->text() == keyName) {
            QTableWidgetItem *statusItem = table->item(row, ColumnTestStatus);
            if (statusItem) {
                statusItem->setText(message);
                if (success) {
                    statusItem->setForeground(QBrush(QColor(0, 170, 0))); // Green
                } else if (message.startsWith("?")) {
                    statusItem->setForeground(QBrush(QColor(204, 68, 68))); // Red
                } else {
                    statusItem->setForeground(QBrush(QColor(102, 102, 102))); // Gray for testing
                }
            }
            break;
        }
    }
}

void KeyManagerWidget::testAllKeys()
{
    // Get all configured services
    auto configuredServices = ak::services::detectConfiguredServices(config, currentProfile.toStdString());
    if (configuredServices.empty()) {
        showError("No API keys configured for testing");
        return;
    }
    
    // Reset all test statuses to "Testing..."
    for (int row = 0; row < table->rowCount(); ++row) {
        QString keyName = table->item(row, ColumnName)->text();
        QString serviceName = table->item(row, ColumnService)->text();
        QString serviceCode = getServiceCode(serviceName);
        
        if (!serviceCode.isEmpty() && std::find(configuredServices.begin(), configuredServices.end(), serviceCode.toStdString()) != configuredServices.end()) {
            updateTestStatus(keyName, false, "Testing...");
        }
    }
    
    // Disable buttons during test
    testAllButton->setEnabled(false);
    testSelectedButton->setEnabled(false);
    
    statusLabel->setText(QString("Testing %1 services...").arg(configuredServices.size()));
    statusLabel->setStyleSheet("color: #0066cc; font-size: 12px;");
    
    // Run tests
    std::string profileName = currentProfile.toStdString();

    QTimer::singleShot(100, [this, configuredServices, profileName]() {
        try {
            auto results = ak::services::run_tests_parallel(config, configuredServices, false, profileName, false);
            
            // Update status for each service
            for (const auto& result : results) {
                // Find the key name for this service
                for (int row = 0; row < table->rowCount(); ++row) {
                    QString keyName = table->item(row, ColumnName)->text();
                    QString serviceName = table->item(row, ColumnService)->text();
                    QString serviceCode = getServiceCode(serviceName);
                    
                    if (serviceCode.toStdString() == result.service) {
                        if (result.ok) {
                            updateTestStatus(keyName, true, QString("? %1ms").arg(result.duration.count()));
                        } else {
                            QString error = result.error_message.empty() ? "Failed" : QString::fromStdString(result.error_message);
                            updateTestStatus(keyName, false, "? " + error);
                        }
                    }
                }
            }
            
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
    // First try built-in service code mapping
    QString code = serviceCodeForDisplay(displayName);
    if (!code.isEmpty()) {
        return code;
    }
    
    // If not found in built-in services, check custom services from config
    try {
        auto allServices = ak::services::loadAllServices(config);
        for (const auto& [serviceName, service] : allServices) {
            // Check if display name matches service description or name
            QString serviceDisplayName = QString::fromStdString(service.description);
            if (serviceDisplayName.isEmpty()) {
                QString name = QString::fromStdString(service.name);
                if (!name.isEmpty()) {
                    name[0] = name[0].toUpper();
                }
                serviceDisplayName = name;
            }
            
            // Also check the service name directly (lowercase)
            if (displayName.compare(serviceDisplayName, Qt::CaseInsensitive) == 0 ||
                displayName.compare(QString::fromStdString(service.name), Qt::CaseInsensitive) == 0) {
                return QString::fromStdString(service.name);
            }
        }
    } catch (const std::exception&) {
        // If loading services fails, return empty (will be handled by caller)
    }
    
    // If still not found, return empty string (service not recognized)
    return QString();
}

// Profile Management Methods
void KeyManagerWidget::createProfile()
{
    ProfileCreateDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString profileName = dialog.getProfileName();

        try {
            // Create empty profile
            std::vector<std::string> emptyKeys;
            ak::storage::writeProfile(config, profileName.toStdString(), emptyKeys);

            refreshProfileList();
            
            // Switch to the new profile
            setCurrentProfile(profileName);
            
            showSuccess(QString("Profile '%1' created").arg(profileName));

        } catch (const std::exception& e) {
            showError(QString("Failed to create profile: %1").arg(e.what()));
        }
    }
}

void KeyManagerWidget::deleteProfile()
{
    if (currentProfile.isEmpty()) {
        showError("No profile selected");
        return;
    }

    if (ConfirmationDialog::confirm(this, "Delete Profile",
        QString("Are you sure you want to delete the profile '%1'?").arg(currentProfile),
        "This action cannot be undone.")) {

        try {
            // Delete profile file
            std::filesystem::path profilePath = ak::storage::profilePath(config, currentProfile.toStdString());
            if (std::filesystem::exists(profilePath)) {
                std::filesystem::remove(profilePath);
            }

            // Clear cache for deleted profile
            cachedProfileKeys.erase(currentProfile);
            profileKeys.clear();
            
            refreshProfileList();
            
            // Switch to default profile if available
            if (profileCombo->count() > 0) {
                setCurrentProfile(profileCombo->itemText(0));
            }
            
            showSuccess(QString("Profile '%1' deleted").arg(currentProfile));

        } catch (const std::exception& e) {
            showError(QString("Failed to delete profile: %1").arg(e.what()));
        }
    }
}

void KeyManagerWidget::renameProfile()
{
    if (currentProfile.isEmpty()) {
        showError("No profile selected");
        return;
    }

    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Profile",
        "Enter new profile name:", QLineEdit::Normal, currentProfile, &ok);

    if (ok && !newName.isEmpty() && newName != currentProfile) {
        if (!validateProfileName(newName)) {
            showError("Invalid profile name. Use only letters, numbers, hyphens, and underscores.");
            return;
        }

        // Check if profile name already exists
        try {
            auto profiles = ak::storage::listProfiles(config);
            for (const auto& profile : profiles) {
                if (QString::fromStdString(profile) == newName) {
                    showError("A profile with this name already exists.");
                    return;
                }
            }
        } catch (const std::exception& e) {
            showError(QString("Failed to check profiles: %1").arg(e.what()));
            return;
        }

        try {
            // Read current profile
            auto keys = ak::storage::readProfile(config, currentProfile.toStdString());

            // Write to new profile
            ak::storage::writeProfile(config, newName.toStdString(), keys);

            // Copy keys if they exist in cache
            if (cachedProfileKeys.find(currentProfile) != cachedProfileKeys.end()) {
                cachedProfileKeys[newName] = cachedProfileKeys[currentProfile];
            }

            // Delete old profile
            std::filesystem::path oldPath = ak::storage::profilePath(config, currentProfile.toStdString());
            if (std::filesystem::exists(oldPath)) {
                std::filesystem::remove(oldPath);
            }

            refreshProfileList();
            
            // Switch to renamed profile
            setCurrentProfile(newName);

            showSuccess(QString("Profile renamed from '%1' to '%2'").arg(currentProfile).arg(newName));

        } catch (const std::exception& e) {
            showError(QString("Failed to rename profile: %1").arg(e.what()));
        }
    }
}

void KeyManagerWidget::duplicateProfile()
{
    if (currentProfile.isEmpty()) {
        showError("No profile selected");
        return;
    }

    QString newProfileName = QInputDialog::getText(this, "Duplicate Profile",
        QString("Enter name for duplicate of '%1':").arg(currentProfile),
        QLineEdit::Normal, currentProfile + "_copy");

    if (!newProfileName.isEmpty() && validateProfileName(newProfileName)) {
        // Check if profile name already exists
        try {
            auto profiles = ak::storage::listProfiles(config);
            for (const auto& profile : profiles) {
                if (QString::fromStdString(profile) == newProfileName) {
                    showError("A profile with this name already exists!");
                    return;
                }
            }
        } catch (const std::exception& e) {
            showError(QString("Failed to check profiles: %1").arg(e.what()));
            return;
        }

        try {
            // Read original profile keys
            auto originalKeys = ak::storage::readProfile(config, currentProfile.toStdString());
            
            // Create new profile with same keys
            ak::storage::writeProfile(config, newProfileName.toStdString(), originalKeys);

            // Copy keys from cache if available
            if (cachedProfileKeys.find(currentProfile) != cachedProfileKeys.end()) {
                cachedProfileKeys[newProfileName] = cachedProfileKeys[currentProfile];
            }

            refreshProfileList();
            
            // Switch to the duplicated profile
            setCurrentProfile(newProfileName);
            
            showSuccess(QString("Profile '%1' duplicated as '%2'").arg(currentProfile).arg(newProfileName));

        } catch (const std::exception& e) {
            showError(QString("Failed to duplicate profile: %1").arg(e.what()));
        }
    }
}

void KeyManagerWidget::importProfile()
{
    QString suggestedName = currentProfile.isEmpty() ? "imported_profile" : currentProfile + "_imported";
    ProfileImportExportDialog dialog(ProfileImportExportDialog::Import, suggestedName, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString filePath = dialog.getFilePath();
        QString format = dialog.getFormat();
        QString profileName = dialog.getProfileName();

        try {
            std::vector<std::string> keys;

            if (format == "env") {
                // Parse .env file
                std::ifstream input(filePath.toStdString());
                auto parsedKeys = ak::storage::parse_env_file(input);
                for (const auto& [key, value] : parsedKeys) {
                    keys.push_back(key);
                }
            } else if (format == "json") {
                // Parse JSON file
                std::ifstream input(filePath.toStdString());
                std::string content((std::istreambuf_iterator<char>(input)),
                                  std::istreambuf_iterator<char>());
                auto parsedKeys = ak::storage::parse_json_min(content);
                for (const auto& [key, value] : parsedKeys) {
                    keys.push_back(key);
                }
            } else if (format == "yaml") {
                // For now, treat YAML as JSON (simplified)
                std::ifstream input(filePath.toStdString());
                std::string content((std::istreambuf_iterator<char>(input)),
                                  std::istreambuf_iterator<char>());
                auto parsedKeys = ak::storage::parse_json_min(content);
                for (const auto& [key, value] : parsedKeys) {
                    keys.push_back(key);
                }
            }

            // Write profile
            ak::storage::writeProfile(config, profileName.toStdString(), keys);

            refreshProfileList();
            
            // Switch to imported profile
            setCurrentProfile(profileName);
            
            showSuccess(QString("Profile '%1' imported with %2 keys").arg(profileName).arg(keys.size()));

        } catch (const std::exception& e) {
            showError(QString("Failed to import profile: %1").arg(e.what()));
        }
    }
}

void KeyManagerWidget::exportProfile()
{
    if (currentProfile.isEmpty()) {
        showError("No profile selected");
        return;
    }

    ProfileImportExportDialog dialog(ProfileImportExportDialog::Export, currentProfile, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString filePath = dialog.getFilePath();
        QString format = dialog.getFormat();
        QString selectedProfileForExport = dialog.getProfileName();

        try {
            // Read profile keys
            auto keys = ak::storage::readProfile(config, selectedProfileForExport.toStdString());
            
            // Load vault to get actual key values
            auto vault = ak::storage::loadVault(config);

            if (format == "env") {
                // Export as .env file with values
                std::ofstream output(filePath.toStdString());
                for (const auto& key : keys) {
                    auto it = vault.kv.find(key);
                    if (it != vault.kv.end()) {
                        // Escape quotes and backslashes in values
                        std::string value = it->second;
                        std::string escaped;
                        for (char c : value) {
                            if (c == '"' || c == '\\') {
                                escaped += '\\';
                            }
                            escaped += c;
                        }
                        output << key << "=\"" << escaped << "\"" << std::endl;
                    } else {
                        output << key << "=" << std::endl;
                    }
                }
            } else if (format == "json") {
                // Export as JSON with values
                QJsonObject root;
                QJsonObject keysObject;
                for (const auto& key : keys) {
                    auto it = vault.kv.find(key);
                    if (it != vault.kv.end()) {
                        keysObject[QString::fromStdString(key)] = QString::fromStdString(it->second);
                    } else {
                        keysObject[QString::fromStdString(key)] = "";
                    }
                }
                root["profile"] = QString::fromStdString(selectedProfileForExport.toStdString());
                root["keys"] = keysObject;

                QJsonDocument doc(root);
                QFile file(filePath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(doc.toJson());
                    file.close();
                }
            }

            showSuccess(QString("Profile '%1' exported to %2").arg(selectedProfileForExport).arg(filePath));

        } catch (const std::exception& e) {
            showError(QString("Failed to export profile: %1").arg(e.what()));
        }
    }
}

bool KeyManagerWidget::validateProfileName(const QString &name)
{
    if (name.isEmpty()) return false;

    QRegularExpression regex("^[a-zA-Z0-9_-]+$");
    return regex.match(name).hasMatch();
}

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI
