#ifdef BUILD_GUI

#include "gui/widgets/common/dialogs.hpp"
#include "gui/widgets/common/secureinput.hpp"
#include "gui/widgets/servicemanager.hpp"
#include "services/services.hpp"
#include <algorithm>
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

namespace ak {
namespace gui {
namespace widgets {

// BaseDialog Implementation
BaseDialog::BaseDialog(const QString &title, QWidget *parent)
    : QDialog(parent), mainLayout(nullptr), formLayout(nullptr), buttonBox(nullptr)
{
    setWindowTitle(title);
    setModal(true);
    resize(400, 300);
    
    mainLayout = new QVBoxLayout(this);
    formLayout = new QFormLayout();
    mainLayout->addLayout(formLayout);
    
    // Add stretch before buttons
    mainLayout->addStretch();
}

void BaseDialog::setupButtons(QDialogButtonBox::StandardButtons buttons)
{
    buttonBox = new QDialogButtonBox(buttons, this);
    mainLayout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void BaseDialog::addFormRow(const QString &label, QWidget *widget)
{
    formLayout->addRow(label, widget);
}

void BaseDialog::addWidget(QWidget *widget)
{
    formLayout->addRow(widget);
}

void BaseDialog::setValid(bool valid)
{
    if (buttonBox) {
        QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
        if (okButton) {
            okButton->setEnabled(valid);
        }
    }
}

// KeyEditDialog Implementation
KeyEditDialog::KeyEditDialog(const core::Config& cfg, QWidget *parent)
    : BaseDialog("Add New Key", parent), nameEdit(nullptr), valueEdit(nullptr),
      serviceCombo(nullptr), pendingServiceId(), config(cfg), editMode(false)
{
    setupUi();
}

KeyEditDialog::KeyEditDialog(const core::Config& cfg, const QString &keyName, const QString &keyValue,
                           const QString &service, QWidget *parent)
    : BaseDialog("Edit Key", parent), nameEdit(nullptr), valueEdit(nullptr),
      serviceCombo(nullptr), pendingServiceId(), config(cfg), editMode(true)
{
    setupUi();

    nameEdit->setText(keyName);
    valueEdit->setText(keyValue);

    int serviceIndex = serviceCombo->findData(service);
    if (serviceIndex < 0) {
        serviceIndex = serviceCombo->findText(service, Qt::MatchFixedString);
        if (serviceIndex < 0) {
            for (int i = 0; i < serviceCombo->count(); ++i) {
                if (serviceCombo->itemText(i).compare(service, Qt::CaseInsensitive) == 0) {
                    serviceIndex = i;
                    break;
                }
            }
            if (serviceIndex < 0) {
                serviceIndex = 0;
            }
        }
    }
    serviceCombo->setCurrentIndex(serviceIndex);

    // In edit mode, don't allow changing the name
    nameEdit->setReadOnly(true);
}

PassphrasePromptDialog::PassphrasePromptDialog(bool requireConfirmation, bool allowRemember, QWidget *parent)
    : BaseDialog(requireConfirmation ? "Set Passphrase" : "Enter Passphrase", parent),
      passphraseEdit(nullptr), confirmEdit(nullptr), rememberCheck(nullptr), warningLabel(nullptr),
      requireConfirmation(requireConfirmation)
{
    setupUi(requireConfirmation, allowRemember);
    validateInput();
}

void PassphrasePromptDialog::setupUi(bool requireConfirmation, bool allowRemember)
{
    QLabel *infoLabel = new QLabel("Enter the passphrase that protects your encrypted keys.", this);
    infoLabel->setWordWrap(true);
    addWidget(infoLabel);

    passphraseEdit = new SecureInputWidget(this);
    passphraseEdit->setPlaceholderText("Passphrase");
    addFormRow("Passphrase:", passphraseEdit);

    confirmEdit = nullptr;
    if (requireConfirmation) {
        confirmEdit = new SecureInputWidget(this);
        confirmEdit->setPlaceholderText("Confirm passphrase");
        addFormRow("Confirm:", confirmEdit);
        connect(confirmEdit, &SecureInputWidget::textChanged, this, &PassphrasePromptDialog::validateInput);
    }

    connect(passphraseEdit, &SecureInputWidget::textChanged, this, &PassphrasePromptDialog::validateInput);

    rememberCheck = nullptr;
    if (allowRemember) {
        rememberCheck = new QCheckBox("Remember passphrase until I quit", this);
        rememberCheck->setChecked(true);
        addWidget(rememberCheck);
    }

    warningLabel = new QLabel(this);
    warningLabel->setStyleSheet("color: #cc6600; font-size: 12px;");
    warningLabel->setVisible(false);
    addWidget(warningLabel);

    setupButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
}

QString PassphrasePromptDialog::passphrase() const
{
    return passphraseEdit ? passphraseEdit->text() : QString();
}

bool PassphrasePromptDialog::rememberForSession() const
{
    return rememberCheck ? rememberCheck->isChecked() : false;
}

void PassphrasePromptDialog::validateInput()
{
    QString pass = passphraseEdit ? passphraseEdit->text() : QString();
    QString confirm = confirmEdit ? confirmEdit->text() : QString();

    bool ok = !pass.isEmpty();
    if (ok && requireConfirmation) {
        ok = (pass == confirm);
    }

    bool weak = pass.length() < 12;
    if (weak && !pass.isEmpty()) {
        warningLabel->setText("Passphrase looks weak. Consider using at least 12 characters.");
        warningLabel->setVisible(true);
    } else {
        warningLabel->setVisible(false);
    }

    if (confirmEdit) {
        bool match = (pass == confirm);
        confirmEdit->setValid(match || confirm.isEmpty());
    }

    setValid(ok);
}

void KeyEditDialog::setupUi()
{
    nameEdit = new QLineEdit(this);
    nameEdit->setPlaceholderText("Enter key name (e.g., OPENAI_API_KEY)");
    
    valueEdit = new SecureInputWidget(this);
    valueEdit->setPlaceholderText("Enter key value");
    
    serviceCombo = new QComboBox(this);
    populateServices();
    connect(serviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &KeyEditDialog::onServiceChanged);
    
    addFormRow("Key Name:", nameEdit);
    addFormRow("Key Value:", valueEdit);
    addFormRow("Service:", serviceCombo);
    
    setupButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    
    // Connect validation
    connect(nameEdit, &QLineEdit::textChanged, this, &KeyEditDialog::validateInput);
    connect(valueEdit, &SecureInputWidget::textChanged, this, &KeyEditDialog::validateInput);
    
    // Set name field validator - allow lowercase/uppercase/numbers/underscores
    // The actual validation happens in validateInput() which converts to uppercase
    QRegularExpression nameRegex("^[A-Za-z][A-Za-z0-9_]*$");
    nameEdit->setValidator(new QRegularExpressionValidator(nameRegex, this));
    
    // Convert to uppercase as user types (but allow lowercase input)
    connect(nameEdit, &QLineEdit::textChanged, [this](const QString &text) {
        if (!text.isEmpty() && !editMode) {
            // Convert to uppercase but preserve cursor position
            int cursorPos = nameEdit->cursorPosition();
            QString upper = text.toUpper();
            if (upper != text) {
                nameEdit->blockSignals(true);
                nameEdit->setText(upper);
                nameEdit->setCursorPosition(qMin(cursorPos, upper.length()));
                nameEdit->blockSignals(false);
            }
        }
    });
    
    validateInput();
}

void KeyEditDialog::populateServices()
{
    refreshServiceList();
}

QString KeyEditDialog::getKeyName() const
{
    return nameEdit->text().trimmed().toUpper();
}

QString KeyEditDialog::getKeyValue() const
{
    return valueEdit->text();
}

QString KeyEditDialog::getService() const
{
    QVariant data = serviceCombo->currentData();
    if (data.isValid()) {
        QString value = data.toString();
        if (value == "__add_new__") {
            return QString();
        }
        return value;
    }
    return QString();
}

void KeyEditDialog::validateInput()
{
    bool valid = !nameEdit->text().trimmed().isEmpty() && 
                 !valueEdit->text().isEmpty();
    
    // Additional name validation
    if (valid && !editMode) {
        QString name = nameEdit->text().trimmed();
        valid = name.length() > 0 && name.contains(QRegularExpression("^[A-Z][A-Z0-9_]*$"));
    }
    
    setValid(valid);
}

void KeyEditDialog::refreshServiceList()
{
    bool blocked = serviceCombo->blockSignals(true);
    serviceCombo->clear();
    availableServices.clear();

    serviceCombo->addItem(QStringLiteral("Auto-detect from key name"), QString());

    try {
        auto services = ak::services::loadAllServices(config);
        for (const auto& [name, service] : services) {
            availableServices.push_back(service);
        }
    } catch (const std::exception&) {
        // Ignore loading errors and leave list minimal
    }

    std::sort(availableServices.begin(), availableServices.end(), [this](const services::Service& a, const services::Service& b) {
        return displayNameForService(a).localeAwareCompare(displayNameForService(b)) < 0;
    });

    for (const auto& service : availableServices) {
        QString id = QString::fromStdString(service.name);
        QString label = displayNameForService(service);
        serviceCombo->addItem(label, id);
    }

    serviceCombo->insertSeparator(serviceCombo->count());
    serviceCombo->addItem(QStringLiteral("Add new service…"), QStringLiteral("__add_new__"));

    serviceCombo->blockSignals(blocked);
}

QString KeyEditDialog::displayNameForService(const services::Service &service) const
{
    if (!service.description.empty()) {
        return QString::fromStdString(service.description);
    }

    QString base = QString::fromStdString(service.name);
    base.replace('_', ' ');
    base.replace('-', ' ');
    QStringList parts = base.split(' ', Qt::SkipEmptyParts);
    for (QString& part : parts) {
        if (!part.isEmpty()) {
            part[0] = part[0].toUpper();
            for (int i = 1; i < part.length(); ++i) {
                part[i] = part[i].toLower();
            }
        }
    }
    return parts.join(' ');
}

void KeyEditDialog::onServiceChanged(int index)
{
    QString value = serviceCombo->itemData(index).toString();
    if (value == QStringLiteral("__add_new__")) {
        if (!handleNewServiceFlow()) {
            serviceCombo->blockSignals(true);
            serviceCombo->setCurrentIndex(0);
            serviceCombo->blockSignals(false);
        }
    }
}

bool KeyEditDialog::handleNewServiceFlow()
{
    ServiceEditorDialog dialog;

    services::Service seed;
    seed.isBuiltIn = false;
    QString keyName = nameEdit->text().trimmed();
    if (!keyName.isEmpty()) {
        seed.keyName = keyName.toUpper().toStdString();
        QString simplified = keyName.toLower();
        simplified.replace("_api_key", "");
        simplified.replace("api_key", "");
        simplified.replace('_', ' ');
        simplified = simplified.trimmed().replace(' ', '_');
        if (!simplified.isEmpty()) {
            seed.name = simplified.toStdString();
        }
    }
    dialog.setService(seed);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    services::Service newService = dialog.getService();
    newService.isBuiltIn = false;

    try {
        services::addService(config, newService);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Service Creation Failed", QString::fromStdString(e.what()));
        return false;
    }

    pendingServiceId = QString::fromStdString(newService.name);
    refreshServiceList();

    int index = serviceCombo->findData(pendingServiceId);
    if (index >= 0) {
        serviceCombo->blockSignals(true);
        serviceCombo->setCurrentIndex(index);
        serviceCombo->blockSignals(false);
    }

    QString detectedValue = dialog.getDetectedApiKeyValue();
    if (!detectedValue.isEmpty() && valueEdit->text().isEmpty()) {
        valueEdit->setText(detectedValue);
    }

    return true;
}

// ProfileCreateDialog Implementation
ProfileCreateDialog::ProfileCreateDialog(QWidget *parent)
    : BaseDialog("Create New Profile", parent), nameEdit(nullptr)
{
    setupUi();
}

void ProfileCreateDialog::setupUi()
{
    nameEdit = new QLineEdit(this);
    nameEdit->setPlaceholderText("Enter profile name");
    
    addFormRow("Profile Name:", nameEdit);
    
    setupButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    
    connect(nameEdit, &QLineEdit::textChanged, this, &ProfileCreateDialog::validateInput);
    
    // Set validator for profile names
    QRegularExpression profileRegex("^[a-zA-Z0-9_-]+$");
    nameEdit->setValidator(new QRegularExpressionValidator(profileRegex, this));
    
    validateInput();
}

QString ProfileCreateDialog::getProfileName() const
{
    return nameEdit->text().trimmed();
}

void ProfileCreateDialog::validateInput()
{
    QString name = nameEdit->text().trimmed();
    bool valid = !name.isEmpty() && name.length() >= 2;
    setValid(valid);
}

// ConfirmationDialog Implementation
ConfirmationDialog::ConfirmationDialog(const QString &title, const QString &message,
                                     const QString &details, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setModal(true);
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    QLabel *messageLabel = new QLabel(message, this);
    messageLabel->setWordWrap(true);
    layout->addWidget(messageLabel);
    
    if (!details.isEmpty()) {
        QTextEdit *detailsEdit = new QTextEdit(this);
        detailsEdit->setPlainText(details);
        detailsEdit->setReadOnly(true);
        detailsEdit->setMaximumHeight(100);
        layout->addWidget(detailsEdit);
    }
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Yes | QDialogButtonBox::No, this);
    layout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    resize(350, details.isEmpty() ? 120 : 220);
}

bool ConfirmationDialog::confirm(QWidget *parent, const QString &title, 
                                const QString &message, const QString &details)
{
    ConfirmationDialog dialog(title, message, details, parent);
    return dialog.exec() == QDialog::Accepted;
}

void ConfirmationDialog::showInfo(QWidget *parent, const QString &title, const QString &message)
{
    QMessageBox::information(parent, title, message);
}

void ConfirmationDialog::showError(QWidget *parent, const QString &title, const QString &message)
{
    QMessageBox::critical(parent, title, message);
}

void ConfirmationDialog::showWarning(QWidget *parent, const QString &title, const QString &message)
{
    QMessageBox::warning(parent, title, message);
}

// ProgressDialog Implementation
ProgressDialog::ProgressDialog(const QString &title, QWidget *parent)
    : QDialog(parent), statusLabel(nullptr), progressBar(nullptr), detailsText(nullptr)
{
    setWindowTitle(title);
    setModal(true);
    setupUi();
}

void ProgressDialog::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    statusLabel = new QLabel("Processing...", this);
    layout->addWidget(statusLabel);
    
    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    layout->addWidget(progressBar);
    
    detailsText = new QTextEdit(this);
    detailsText->setMaximumHeight(150);
    detailsText->setReadOnly(true);
    layout->addWidget(detailsText);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    resize(400, 300);
}

void ProgressDialog::setProgress(int value)
{
    progressBar->setValue(value);
}

void ProgressDialog::setMaximum(int maximum)
{
    progressBar->setMaximum(maximum);
}

void ProgressDialog::setLabelText(const QString &text)
{
    statusLabel->setText(text);
}

void ProgressDialog::setDetailText(const QString &text)
{
    detailsText->append(text);
    
    // Auto-scroll to bottom
    QTextCursor cursor = detailsText->textCursor();
    cursor.movePosition(QTextCursor::End);
    detailsText->setTextCursor(cursor);
}

// ProfileImportExportDialog Implementation
ProfileImportExportDialog::ProfileImportExportDialog(Mode mode, QWidget *parent)
    : BaseDialog(mode == Import ? "Import Profile" : "Export Profile", parent),
      mode(mode), filePathEdit(nullptr), browseButton(nullptr),
      formatCombo(nullptr), profileNameEdit(nullptr)
{
    setupUi();
}

ProfileImportExportDialog::ProfileImportExportDialog(Mode mode, const QString &defaultProfileName, QWidget *parent)
    : BaseDialog(mode == Import ? "Import Profile" : "Export Profile", parent),
      mode(mode), filePathEdit(nullptr), browseButton(nullptr),
      formatCombo(nullptr), profileNameEdit(nullptr)
{
    setupUi();
    setDefaultProfileName(defaultProfileName);
}

void ProfileImportExportDialog::setupUi()
{
    // File selection
    QHBoxLayout *fileLayout = new QHBoxLayout();
    filePathEdit = new QLineEdit(this);
    filePathEdit->setPlaceholderText(mode == Import ? "Select file to import" : "Select export location");
    browseButton = new QPushButton("Browse...", this);
    fileLayout->addWidget(filePathEdit);
    fileLayout->addWidget(browseButton);

    // Wrap layout in a QWidget container for QFormLayout row
    QWidget *fileRow = new QWidget(this);
    fileRow->setLayout(fileLayout);
    addFormRow("File:", fileRow);
    
    // Format selection
    formatCombo = new QComboBox(this);
    formatCombo->addItem("Environment (.env)", "env");
    formatCombo->addItem("JSON (.json)", "json");
    formatCombo->addItem("YAML (.yaml)", "yaml");
    addFormRow("Format:", formatCombo);
    
    // Profile name (for import or export name)
    profileNameEdit = new QLineEdit(this);
    if (mode == Import) {
        profileNameEdit->setPlaceholderText("Name for imported profile");
        addFormRow("Profile Name:", profileNameEdit);
    } else {
        profileNameEdit->setPlaceholderText("Profile to export");
        addFormRow("Export Profile:", profileNameEdit);
    }
    
    setupButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    
    // Connect signals
    connect(browseButton, &QPushButton::clicked, this, &ProfileImportExportDialog::browseFile);
    connect(filePathEdit, &QLineEdit::textChanged, this, &ProfileImportExportDialog::validateInput);
    connect(profileNameEdit, &QLineEdit::textChanged, this, &ProfileImportExportDialog::validateInput);
    
    validateInput();
}

void ProfileImportExportDialog::browseFile()
{
    QString filter;
    QString format = formatCombo->currentData().toString();
    
    if (format == "env") {
        filter = "Environment Files (*.env);;All Files (*)";
    } else if (format == "json") {
        filter = "JSON Files (*.json);;All Files (*)";
    } else if (format == "yaml") {
        filter = "YAML Files (*.yaml *.yml);;All Files (*)";
    } else {
        filter = "All Files (*)";
    }
    
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString filePath;
    
    if (mode == Import) {
        filePath = QFileDialog::getOpenFileName(this, "Select file to import", dir, filter);
    } else {
        filePath = QFileDialog::getSaveFileName(this, "Select export location", dir, filter);
    }
    
    if (!filePath.isEmpty()) {
        filePathEdit->setText(filePath);
    }
}

QString ProfileImportExportDialog::getFilePath() const
{
    return filePathEdit->text().trimmed();
}

QString ProfileImportExportDialog::getFormat() const
{
    return formatCombo->currentData().toString();
}

QString ProfileImportExportDialog::getProfileName() const
{
    return profileNameEdit->text().trimmed();
}

void ProfileImportExportDialog::setDefaultProfileName(const QString &profileName)
{
    if (profileNameEdit) {
        profileNameEdit->setText(profileName);
    }
}

void ProfileImportExportDialog::validateInput()
{
    bool valid = !filePathEdit->text().trimmed().isEmpty() &&
                 !profileNameEdit->text().trimmed().isEmpty();
    setValid(valid);
}

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI
