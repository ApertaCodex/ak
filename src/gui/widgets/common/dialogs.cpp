#ifdef BUILD_GUI

#include "gui/widgets/common/dialogs.hpp"
#include "gui/widgets/common/secureinput.hpp"
#include "services/services.hpp"
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
KeyEditDialog::KeyEditDialog(QWidget *parent)
    : BaseDialog("Add New Key", parent), nameEdit(nullptr), valueEdit(nullptr), 
      serviceCombo(nullptr), editMode(false)
{
    setupUi();
}

KeyEditDialog::KeyEditDialog(const QString &keyName, const QString &keyValue, 
                           const QString &service, QWidget *parent)
    : BaseDialog("Edit Key", parent), nameEdit(nullptr), valueEdit(nullptr),
      serviceCombo(nullptr), editMode(true)
{
    setupUi();
    
    nameEdit->setText(keyName);
    valueEdit->setText(keyValue);
    
    int serviceIndex = serviceCombo->findText(service);
    if (serviceIndex >= 0) {
        serviceCombo->setCurrentIndex(serviceIndex);
    }
    
    // In edit mode, don't allow changing the name
    nameEdit->setReadOnly(true);
}

void KeyEditDialog::setupUi()
{
    nameEdit = new QLineEdit(this);
    nameEdit->setPlaceholderText("Enter key name (e.g., OPENAI_API_KEY)");
    
    valueEdit = new SecureInputWidget(this);
    valueEdit->setPlaceholderText("Enter key value");
    
    serviceCombo = new QComboBox(this);
    populateServices();
    
    addFormRow("Key Name:", nameEdit);
    addFormRow("Key Value:", valueEdit);
    addFormRow("Service:", serviceCombo);
    
    setupButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    
    // Connect validation
    connect(nameEdit, &QLineEdit::textChanged, this, &KeyEditDialog::validateInput);
    connect(valueEdit, &SecureInputWidget::textChanged, this, &KeyEditDialog::validateInput);
    
    // Set name field validator
    QRegularExpression nameRegex("^[A-Z][A-Z0-9_]*$");
    nameEdit->setValidator(new QRegularExpressionValidator(nameRegex, this));
    
    validateInput();
}

void KeyEditDialog::populateServices()
{
    serviceCombo->addItem("Custom", "custom");
    
    // Get known services from the services module
    auto knownServices = ak::services::getKnownServiceKeys();
    QStringList services;
    for (const auto& service : knownServices) {
        services << QString::fromStdString(service);
    }
    services.sort();
    
    for (const QString& service : services) {
        QString displayName = service;
        // Convert snake_case to Title Case
        displayName = displayName.replace('_', ' ');
        QStringList parts = displayName.split(' ');
        for (QString& part : parts) {
            if (!part.isEmpty()) {
                part[0] = part[0].toUpper();
                for (int i = 1; i < part.length(); ++i) {
                    part[i] = part[i].toLower();
                }
            }
        }
        displayName = parts.join(' ');
        
        serviceCombo->addItem(displayName, service.toLower());
    }
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
    return serviceCombo->currentData().toString();
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