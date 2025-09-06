#ifdef BUILD_GUI

#include "gui/widgets/profilemanager.hpp"
#include "gui/widgets/common/dialogs.hpp"
#include "storage/vault.hpp"
#include "core/config.hpp"
#include <QApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QFileDialog>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QRegularExpression>
#include <QTimer>
#include <fstream>
#include <algorithm>

namespace ak {
namespace gui {
namespace widgets {

// ProfileListItem Implementation
ProfileListItem::ProfileListItem(const QString &profileName, int keyCount, QListWidget *parent)
    : QListWidgetItem(parent), profileName(profileName), keyCount(keyCount)
{
    updateDisplay();
}

QString ProfileListItem::getProfileName() const
{
    return profileName;
}

int ProfileListItem::getKeyCount() const
{
    return keyCount;
}

void ProfileListItem::setKeyCount(int count)
{
    keyCount = count;
    updateDisplay();
}

void ProfileListItem::updateDisplay()
{
    QString displayText = QString("%1 (%2 keys)").arg(profileName).arg(keyCount);
    setText(displayText);

    setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));

    QString tooltip = QString("Profile: %1\nKeys: %2").arg(profileName).arg(keyCount);
    setToolTip(tooltip);
}

// ProfileManagerWidget Implementation
ProfileManagerWidget::ProfileManagerWidget(const core::Config& config, QWidget *parent)
    : QWidget(parent), config(config), mainLayout(nullptr), controlsLayout(nullptr),
      profileList(nullptr), profileListLabel(nullptr), createButton(nullptr),
      deleteButton(nullptr), renameButton(nullptr), importButton(nullptr),
      exportButton(nullptr), refreshButton(nullptr), detailsGroup(nullptr),
      profileNameLabel(nullptr), keyCountLabel(nullptr), keysText(nullptr),
      statusLabel(nullptr)
{
    setupUi();
    loadProfiles();
}

void ProfileManagerWidget::setupUi()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Create splitter for main layout
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // Left panel - profile list and controls
    QWidget *leftPanel = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);

    setupProfileList();
    setupControls();

    leftLayout->addWidget(profileListLabel);
    leftLayout->addWidget(profileList);
    leftLayout->addLayout(controlsLayout);

    // Right panel - profile details
    QWidget *rightPanel = new QWidget();
    setupProfileDetails();

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setSizes({300, 500});

    mainLayout->addWidget(splitter);

    // Status bar
    statusLabel = new QLabel("Ready", this);
    statusLabel->setStyleSheet("color: #666; font-size: 12px;");
    mainLayout->addWidget(statusLabel);
}

void ProfileManagerWidget::setupProfileList()
{
    profileListLabel = new QLabel("Available Profiles:", this);
    profileListLabel->setStyleSheet("font-weight: bold; font-size: 14px;");

    profileList = new QListWidget(this);
    profileList->setSelectionMode(QAbstractItemView::SingleSelection);
    profileList->setSortingEnabled(true);

    connect(profileList, &QListWidget::itemSelectionChanged,
            this, &ProfileManagerWidget::onProfileSelectionChanged);
    connect(profileList, &QListWidget::itemDoubleClicked,
            this, &ProfileManagerWidget::onProfileDoubleClicked);
}

void ProfileManagerWidget::setupControls()
{
    controlsLayout = new QHBoxLayout();

    createButton = new QPushButton("Create", this);
    createButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
    connect(createButton, &QPushButton::clicked, this, &ProfileManagerWidget::createProfile);

    deleteButton = new QPushButton("Delete", this);
    deleteButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_TrashIcon));
    deleteButton->setEnabled(false);
    connect(deleteButton, &QPushButton::clicked, this, &ProfileManagerWidget::deleteProfile);

    renameButton = new QPushButton("Rename", this);
    renameButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    renameButton->setEnabled(false);
    connect(renameButton, &QPushButton::clicked, this, &ProfileManagerWidget::renameProfile);

    importButton = new QPushButton("Import", this);
    importButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowDown));
    connect(importButton, &QPushButton::clicked, this, &ProfileManagerWidget::importProfile);

    exportButton = new QPushButton("Export", this);
    exportButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
    exportButton->setEnabled(false);
    connect(exportButton, &QPushButton::clicked, this, &ProfileManagerWidget::exportProfile);

    refreshButton = new QPushButton("Refresh", this);
    refreshButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserReload));
    connect(refreshButton, &QPushButton::clicked, this, &ProfileManagerWidget::refreshProfiles);

    // Layout buttons
    QVBoxLayout *buttonsLayout = new QVBoxLayout();

    QHBoxLayout *topButtons = new QHBoxLayout();
    topButtons->addWidget(createButton);
    topButtons->addWidget(deleteButton);
    topButtons->addWidget(renameButton);

    QHBoxLayout *bottomButtons = new QHBoxLayout();
    bottomButtons->addWidget(importButton);
    bottomButtons->addWidget(exportButton);
    bottomButtons->addWidget(refreshButton);

    buttonsLayout->addLayout(topButtons);
    buttonsLayout->addLayout(bottomButtons);

    controlsLayout->addLayout(buttonsLayout);
}

void ProfileManagerWidget::setupProfileDetails()
{
    QVBoxLayout *detailsLayout = new QVBoxLayout();

    detailsGroup = new QGroupBox("Profile Details", this);
    QVBoxLayout *groupLayout = new QVBoxLayout(detailsGroup);

    profileNameLabel = new QLabel("No profile selected", this);
    profileNameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");

    keyCountLabel = new QLabel("", this);
    keyCountLabel->setStyleSheet("color: #666;");

    keysText = new QTextEdit(this);
    keysText->setReadOnly(true);
    keysText->setFont(QFont("Consolas", 10));
    keysText->setPlainText("Select a profile to view its keys...");

    groupLayout->addWidget(profileNameLabel);
    groupLayout->addWidget(keyCountLabel);
    groupLayout->addWidget(keysText);

    detailsLayout->addWidget(detailsGroup);
}

void ProfileManagerWidget::loadProfiles()
{
    try {
        availableProfiles.clear();
        auto profiles = ak::storage::listProfiles(config);
        for (const auto& profile : profiles) {
            availableProfiles << QString::fromStdString(profile);
        }

        // Clear and populate list
        profileList->clear();

        for (const QString& profileName : availableProfiles) {
            // Get key count for this profile
            auto keys = ak::storage::readProfile(config, profileName.toStdString());
            int keyCount = keys.size();

            new ProfileListItem(profileName, keyCount, profileList);
        }

        profileListLabel->setText(QString("Available Profiles (%1):").arg(availableProfiles.size()));

        // Clear details if no profiles
        if (availableProfiles.isEmpty()) {
            updateProfileDetails("");
        }

    } catch (const std::exception& e) {
        showError(QString("Failed to load profiles: %1").arg(e.what()));
    }
}

void ProfileManagerWidget::refreshProfiles()
{
    loadProfiles();
    showSuccess("Profiles refreshed");
}

void ProfileManagerWidget::createProfile()
{
    ProfileCreateDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString profileName = dialog.getProfileName();

        try {
            // Create empty profile
            std::vector<std::string> emptyKeys;
            ak::storage::writeProfile(config, profileName.toStdString(), emptyKeys);

            loadProfiles();
            showSuccess(QString("Profile '%1' created").arg(profileName));

        } catch (const std::exception& e) {
            showError(QString("Failed to create profile: %1").arg(e.what()));
        }
    }
}

void ProfileManagerWidget::deleteProfile()
{
    QListWidgetItem *selectedItem = profileList->currentItem();
    if (!selectedItem) return;

    ProfileListItem *profileItem = static_cast<ProfileListItem*>(selectedItem);
    QString profileName = profileItem->getProfileName();

    if (ConfirmationDialog::confirm(this, "Delete Profile",
        QString("Are you sure you want to delete the profile '%1'?").arg(profileName),
        "This action cannot be undone.")) {

        try {
            // Delete profile file
            std::filesystem::path profilePath = ak::storage::profilePath(config, profileName.toStdString());
            if (std::filesystem::exists(profilePath)) {
                std::filesystem::remove(profilePath);
            }

            loadProfiles();
            showSuccess(QString("Profile '%1' deleted").arg(profileName));

        } catch (const std::exception& e) {
            showError(QString("Failed to delete profile: %1").arg(e.what()));
        }
    }
}

void ProfileManagerWidget::renameProfile()
{
    QListWidgetItem *selectedItem = profileList->currentItem();
    if (!selectedItem) return;

    ProfileListItem *profileItem = static_cast<ProfileListItem*>(selectedItem);
    QString oldName = profileItem->getProfileName();

    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Profile",
        "Enter new profile name:", QLineEdit::Normal, oldName, &ok);

    if (ok && !newName.isEmpty() && newName != oldName) {
        if (!validateProfileName(newName)) {
            showError("Invalid profile name. Use only letters, numbers, hyphens, and underscores.");
            return;
        }

        if (availableProfiles.contains(newName)) {
            showError("A profile with this name already exists.");
            return;
        }

        try {
            // Read old profile
            auto keys = ak::storage::readProfile(config, oldName.toStdString());

            // Write to new profile
            ak::storage::writeProfile(config, newName.toStdString(), keys);

            // Delete old profile
            std::filesystem::path oldPath = ak::storage::profilePath(config, oldName.toStdString());
            if (std::filesystem::exists(oldPath)) {
                std::filesystem::remove(oldPath);
            }

            loadProfiles();
            showSuccess(QString("Profile renamed from '%1' to '%2'").arg(oldName).arg(newName));

        } catch (const std::exception& e) {
            showError(QString("Failed to rename profile: %1").arg(e.what()));
        }
    }
}

void ProfileManagerWidget::importProfile()
{
    ProfileImportExportDialog dialog(ProfileImportExportDialog::Import, this);
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

            loadProfiles();
            showSuccess(QString("Profile '%1' imported with %2 keys").arg(profileName).arg(keys.size()));

        } catch (const std::exception& e) {
            showError(QString("Failed to import profile: %1").arg(e.what()));
        }
    }
}

void ProfileManagerWidget::exportProfile()
{
    QListWidgetItem *selectedItem = profileList->currentItem();
    if (!selectedItem) return;

    ProfileListItem *profileItem = static_cast<ProfileListItem*>(selectedItem);
    QString profileName = profileItem->getProfileName();

    ProfileImportExportDialog dialog(ProfileImportExportDialog::Export, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString filePath = dialog.getFilePath();
        QString format = dialog.getFormat();

        try {
            // Read profile keys
            auto keys = ak::storage::readProfile(config, profileName.toStdString());

            if (format == "env") {
                // Export as .env file
                std::ofstream output(filePath.toStdString());
                for (const auto& key : keys) {
                    output << key << "=" << std::endl;
                }
            } else if (format == "json") {
                // Export as JSON
                QJsonObject root;
                QJsonArray keysArray;
                for (const auto& key : keys) {
                    keysArray.append(QString::fromStdString(key));
                }
                root["keys"] = keysArray;

                QJsonDocument doc(root);
                QFile file(filePath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(doc.toJson());
                    file.close();
                }
            }

            showSuccess(QString("Profile '%1' exported to %2").arg(profileName).arg(filePath));

        } catch (const std::exception& e) {
            showError(QString("Failed to export profile: %1").arg(e.what()));
        }
    }
}

void ProfileManagerWidget::onProfileSelectionChanged()
{
    QListWidgetItem *selectedItem = profileList->currentItem();
    bool hasSelection = selectedItem != nullptr;

    deleteButton->setEnabled(hasSelection);
    renameButton->setEnabled(hasSelection);
    exportButton->setEnabled(hasSelection);

    if (hasSelection) {
        ProfileListItem *profileItem = static_cast<ProfileListItem*>(selectedItem);
        updateProfileDetails(profileItem->getProfileName());
    } else {
        updateProfileDetails("");
    }
}

void ProfileManagerWidget::onProfileDoubleClicked(QListWidgetItem *item)
{
    ProfileListItem *profileItem = static_cast<ProfileListItem*>(item);
    updateProfileDetails(profileItem->getProfileName());
}

void ProfileManagerWidget::updateProfileDetails(const QString &profileName)
{
    currentProfile = profileName;

    if (profileName.isEmpty()) {
        profileNameLabel->setText("No profile selected");
        keyCountLabel->setText("");
        keysText->setPlainText("Select a profile to view its keys...");
        return;
    }

    profileNameLabel->setText(QString("Profile: %1").arg(profileName));

    try {
        auto keys = ak::storage::readProfile(config, profileName.toStdString());
        keyCountLabel->setText(QString("%1 keys").arg(keys.size()));

        showProfileKeys(profileName);

    } catch (const std::exception& e) {
        keyCountLabel->setText("Error loading keys");
        keysText->setPlainText(QString("Error: %1").arg(e.what()));
    }
}

void ProfileManagerWidget::showProfileKeys(const QString &profileName)
{
    try {
        auto keys = ak::storage::readProfile(config, profileName.toStdString());

        QString keysTextContent;
        if (keys.empty()) {
            keysTextContent = "This profile contains no keys.";
        } else {
            keysTextContent = "Keys in this profile:\n\n";
            for (const auto& key : keys) {
                keysTextContent += QString::fromStdString(key) + "\n";
            }
        }

        keysText->setPlainText(keysTextContent);

    } catch (const std::exception& e) {
        keysText->setPlainText(QString("Error loading keys: %1").arg(e.what()));
    }
}

bool ProfileManagerWidget::validateProfileName(const QString &name)
{
    if (name.isEmpty()) return false;

    QRegularExpression regex("^[a-zA-Z0-9_-]+$");
    return regex.match(name).hasMatch();
}

void ProfileManagerWidget::showError(const QString &message)
{
    statusLabel->setText(QString("Error: %1").arg(message));
    statusLabel->setStyleSheet("color: #ff4444; font-size: 12px;");

    // Reset after 5 seconds
    QTimer::singleShot(5000, [this]() {
        statusLabel->setText("Ready");
        statusLabel->setStyleSheet("color: #666; font-size: 12px;");
    });

    emit statusMessage(QString("Error: %1").arg(message));
}

void ProfileManagerWidget::showSuccess(const QString &message)
{
    statusLabel->setText(message);
    statusLabel->setStyleSheet("color: #00aa00; font-size: 12px;");

    // Reset after 3 seconds
    QTimer::singleShot(3000, [this]() {
        statusLabel->setText("Ready");
        statusLabel->setStyleSheet("color: #666; font-size: 12px;");
    });

    emit statusMessage(message);
}

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI