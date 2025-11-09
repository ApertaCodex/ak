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
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QLabel>
#include <QUrl>
#include <QUrlQuery>
#include <QSet>
#include <algorithm>

namespace ak {
namespace gui {
namespace widgets {

namespace {

struct CurlHeader {
    QString name;
    QString value;
};

struct CurlParseResult {
    bool ok = false;
    QString error;
    QString method = "GET";
    QString url;
    QVector<CurlHeader> headers;
    QString data;
};

struct CurlImportAnalysis {
    bool ok = false;
    QString error;
    QString method;
    QUrl url;
    QVector<CurlHeader> otherHeaders;
    QString authParameter;
    QString authPrefix;
    QString authLocation;
    QString authPreset;
    QString keyName;
    QString apiKeyValue;
    QString serviceId;
    QString serviceDisplayName;
    QString body;
};

QStringList tokenizeCurl(const QString &command)
{
    QStringList tokens;
    QString current;
    bool inSingle = false;
    bool inDouble = false;
    bool escaping = false;

    const QString cleaned = command;

    for (int i = 0; i < cleaned.size(); ++i) {
        QChar ch = cleaned[i];

        if (ch == '\r') {
            continue;
        }

        if (!inSingle && !inDouble && ch == '\\' && i + 1 < cleaned.size() && cleaned[i + 1] == '\n') {
            ++i;
            continue;
        }

        if (escaping) {
            current.append(ch);
            escaping = false;
            continue;
        }

        if (!inSingle && ch == '\\') {
            escaping = true;
            continue;
        }

        if (ch == '"') {
            if (!inSingle) {
                inDouble = !inDouble;
                continue;
            }
        }

        if (ch == '\'') {
            if (!inDouble) {
                inSingle = !inSingle;
                continue;
            }
        }

        if (!inSingle && !inDouble && (ch == '\n' || ch.isSpace())) {
            if (!current.isEmpty()) {
                tokens.append(current);
                current.clear();
            }
            continue;
        }

        current.append(ch);
    }

    if (!current.isEmpty()) {
        tokens.append(current);
    }

    return tokens;
}

CurlHeader parseHeader(const QString &header)
{
    int idx = header.indexOf(':');
    if (idx == -1) {
        return {header.trimmed(), QString()};
    }
    QString name = header.left(idx).trimmed();
    QString value = header.mid(idx + 1).trimmed();
    return {name, value};
}

CurlParseResult parseCurlCommand(const QString &command)
{
    CurlParseResult result;
    QString trimmed = command.trimmed();
    if (trimmed.isEmpty()) {
        result.error = QObject::tr("The cURL command is empty.");
        return result;
    }

    QStringList tokens = tokenizeCurl(trimmed);
    if (tokens.isEmpty() || tokens.first().toLower() != "curl") {
        result.error = QObject::tr("Command must start with 'curl'.");
        return result;
    }

    QString method = "GET";
    QString url;
    QVector<CurlHeader> headers;
    QString data;

    for (int i = 1; i < tokens.size(); ++i) {
        const QString &token = tokens[i];

        auto nextValue = [&](QStringList &list, int &idx) -> QString {
            if (idx + 1 < list.size()) {
                return list[++idx];
            }
            return QString();
        };

        if (token == "-X" || token == "--request") {
            QString value = nextValue(tokens, i);
            if (!value.isEmpty()) {
                method = value.toUpper();
            }
            continue;
        }
        if (token.startsWith("--request=")) {
            method = token.section('=', 1).toUpper();
            continue;
        }
        if (token == "--url") {
            QString value = nextValue(tokens, i);
            if (!value.isEmpty()) {
                url = value;
            }
            continue;
        }
        if (token.startsWith("--url=")) {
            url = token.section('=', 1);
            continue;
        }
        if (token == "-H" || token == "--header") {
            QString value = nextValue(tokens, i);
            if (!value.isEmpty()) {
                headers.append(parseHeader(value));
            }
            continue;
        }
        if (token.startsWith("-H")) {
            QString value = token.section('=', 1);
            if (!value.isEmpty()) {
                headers.append(parseHeader(value));
            }
            continue;
        }
        if (token.startsWith("--header=")) {
            headers.append(parseHeader(token.section('=', 1)));
            continue;
        }
        if (token == "--data" || token == "--data-raw" || token == "--data-binary" || token == "--data-urlencode") {
            QString value = nextValue(tokens, i);
            if (!value.isEmpty()) {
                data = value;
            }
            continue;
        }
        if (token.startsWith("--data=") || token.startsWith("--data-raw=") || token.startsWith("--data-binary=") || token.startsWith("--data-urlencode=")) {
            data = token.section('=', 1);
            continue;
        }
        if (!token.startsWith('-') && url.isEmpty()) {
            url = token;
        }
    }

    if (method.isEmpty()) {
        method = "GET";
    }
    if (method == "GET" && !data.isEmpty()) {
        method = "POST";
    }

    if (url.isEmpty()) {
        result.error = QObject::tr("Unable to find request URL in the cURL command.");
        return result;
    }

    result.ok = true;
    result.method = method;
    result.url = url;
    result.headers = headers;
    result.data = data;
    return result;
}

QString extractEnvVariable(const QString &value)
{
    QRegularExpression envRe(QStringLiteral("\\$\\{?([A-Z0-9_]+)\\}?"));
    auto match = envRe.match(value);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return QString();
}

QString deriveServiceSlug(const QUrl &url)
{
    QString host = url.host();
    if (host.isEmpty()) {
        return QString();
    }

    QStringList parts = host.split('.', Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return host.toLower();
    }

    static const QSet<QString> genericPrefixes = {"api", "api2", "app", "apps", "service", "services", "v1", "v2", "www"};

    QString candidate;
    if (parts.size() >= 2) {
        candidate = parts.at(parts.size() - 2);
        if (genericPrefixes.contains(candidate) && parts.size() >= 3) {
            candidate = parts.at(parts.size() - 3);
        }
    } else {
        candidate = parts.first();
    }

    return candidate.toLower().replace('-', '_');
}

QString toEnvName(const QString &slug)
{
    if (slug.isEmpty()) {
        return QStringLiteral("SERVICE_API_KEY");
    }
    QString env = slug.toUpper();
    env.replace('-', '_');
    if (!env.endsWith("_API_KEY")) {
        env.append("_API_KEY");
    }
    return env;
}

QString toDisplayName(const QString &slug)
{
    QStringList parts = slug.split('_', Qt::SkipEmptyParts);
    for (QString &part : parts) {
        if (!part.isEmpty()) {
            part[0] = part[0].toUpper();
            for (int i = 1; i < part.size(); ++i) {
                part[i] = part[i].toLower();
            }
        }
    }
    return parts.join(' ');
}

QString buildHeaderString(const QVector<CurlHeader> &headers)
{
    QStringList parts;
    for (const auto &header : headers) {
        if (header.name.isEmpty() || header.value.isEmpty()) {
            continue;
        }
        parts.append(QString("-H '%1: %2'").arg(header.name, header.value));
    }
    return parts.join(' ');
}

CurlImportAnalysis analyzeCurl(const CurlParseResult &parse)
{
    CurlImportAnalysis analysis;

    if (!parse.ok) {
        analysis.error = parse.error;
        return analysis;
    }

    QUrl url(parse.url);
    if (!url.isValid()) {
        analysis.error = QObject::tr("The URL in the cURL command is invalid.");
        return analysis;
    }

    analysis.method = parse.method.isEmpty() ? QStringLiteral("GET") : parse.method.toUpper();
    analysis.url = url;
    analysis.body = parse.data;
    analysis.serviceId = deriveServiceSlug(url);
    if (analysis.serviceId.isEmpty()) {
        analysis.serviceId = QStringLiteral("service");
    }
    analysis.serviceDisplayName = toDisplayName(analysis.serviceId);
    analysis.keyName = toEnvName(analysis.serviceId);
    analysis.authParameter = QStringLiteral("Authorization");
    analysis.authPrefix = QStringLiteral("Bearer ");
    analysis.authLocation = QStringLiteral("header");
    analysis.authPreset = QStringLiteral("header_bearer");

    bool authAssigned = false;

    for (const auto &header : parse.headers) {
        QString name = header.name.trimmed();
        QString value = header.value.trimmed();
        bool consumed = false;
        QString lowerName = name.toLower();

        if (!authAssigned && (lowerName == "authorization" || lowerName.contains("api-key") || lowerName.contains("token"))) {
            authAssigned = true;
            analysis.authLocation = QStringLiteral("header");
            analysis.authParameter = name;

            QString scheme;
            QString tokenPart = value;
            int spaceIdx = value.indexOf(' ');
            if (lowerName == "authorization" && spaceIdx > 0) {
                scheme = value.left(spaceIdx).trimmed();
                tokenPart = value.mid(spaceIdx + 1).trimmed();
            }

            if (!scheme.isEmpty()) {
                analysis.authPrefix = scheme + QStringLiteral(" ");
                if (scheme.compare("Bearer", Qt::CaseInsensitive) == 0) {
                    analysis.authPreset = QStringLiteral("header_bearer");
                } else if (scheme.compare("Basic", Qt::CaseInsensitive) == 0) {
                    analysis.authPreset = QStringLiteral("header_basic");
                } else {
                    analysis.authPreset = QStringLiteral("header_custom");
                }
            } else {
                analysis.authPrefix.clear();
                analysis.authPreset = QStringLiteral("header_custom");
            }

            QString envVar = extractEnvVariable(tokenPart);
            if (!envVar.isEmpty()) {
                analysis.keyName = envVar.toUpper();
            } else if (!tokenPart.isEmpty()) {
                analysis.apiKeyValue = tokenPart.trimmed();
            }

            consumed = true;
        }

        if (!consumed) {
            analysis.otherHeaders.append({name, value});
        }
    }

    if (!authAssigned) {
        QUrlQuery query(url);
        const auto items = query.queryItems(QUrl::FullyDecoded);
        for (const auto &item : items) {
            const QString &paramName = item.first;
            QString value = item.second.trimmed();
            QString envVar = extractEnvVariable(value);
            if (!envVar.isEmpty() || !value.isEmpty()) {
                analysis.authLocation = QStringLiteral("query");
                analysis.authParameter = paramName;
                analysis.authPrefix.clear();
                analysis.authPreset = QStringLiteral("query_param");
                if (!envVar.isEmpty()) {
                    analysis.keyName = envVar.toUpper();
                } else {
                    analysis.apiKeyValue = value.trimmed();
                }
                authAssigned = true;
                break;
            }
        }
    }

    if (!authAssigned && !parse.data.trimmed().isEmpty()) {
        QJsonParseError jsonError;
        QJsonDocument doc = QJsonDocument::fromJson(parse.data.toUtf8(), &jsonError);
        if (jsonError.error == QJsonParseError::NoError && doc.isObject()) {
            auto obj = doc.object();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (it.value().isString()) {
                    QString value = it.value().toString().trimmed();
                    QString envVar = extractEnvVariable(value);
                    if (!envVar.isEmpty() || !value.isEmpty()) {
                        analysis.authLocation = QStringLiteral("body");
                        analysis.authParameter = it.key();
                        analysis.authPrefix.clear();
                        analysis.authPreset = QStringLiteral("body_param");
                        if (!envVar.isEmpty()) {
                            analysis.keyName = envVar.toUpper();
                        } else {
                            analysis.apiKeyValue = value.trimmed();
                        }
                        authAssigned = true;
                        break;
                    }
                }
            }
        }
    }

    analysis.ok = true;
    return analysis;
}

} // namespace

// ServiceEditorDialog Implementation
ServiceEditorDialog::ServiceEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Add Custom Service");
    setMinimumSize(450, 400);
    detectedApiKeyValue.clear();
    lastCurlCommand.clear();
    setupUi();
    updateUi();
}

ServiceEditorDialog::ServiceEditorDialog(const services::Service &service, QWidget *parent)
    : QDialog(parent), currentService(service)
{
    setWindowTitle(service.isBuiltIn ? "View/Edit Built-in Service" : "Edit User Service");
    setMinimumSize(450, 400);
    detectedApiKeyValue.clear();
    lastCurlCommand.clear();
    setupUi();
    setService(service);
    updateUi();
}

void ServiceEditorDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    importCurlButton = new QPushButton(tr("Import from cURL…"), this);
    importCurlButton->setToolTip(tr("Paste a cURL command to auto-fill these fields"));
    mainLayout->addWidget(importCurlButton, 0, Qt::AlignLeft);

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

    testBodyEdit = new QPlainTextEdit();
    testBodyEdit->setPlaceholderText("Optional JSON body to include when calling the test endpoint");
    testBodyEdit->setMaximumHeight(120);
    formLayout->addRow("Request Body (JSON):", testBodyEdit);

    authTypeCombo = new QComboBox();
    authTypeCombo->addItem("Authorization header (Bearer token)", "header_bearer");
    authTypeCombo->addItem("Authorization header (Basic)", "header_basic");
    authTypeCombo->addItem("Custom header", "header_custom");
    authTypeCombo->addItem("Query parameter", "query_param");
    authTypeCombo->addItem("Body parameter", "body_param");
    formLayout->addRow("Auth Method:", authTypeCombo);

    authParameterEdit = new QLineEdit();
    authParameterEdit->setPlaceholderText("Authorization");
    formLayout->addRow("Auth Parameter:", authParameterEdit);

    authPrefixEdit = new QLineEdit();
    authPrefixEdit->setPlaceholderText("Bearer ");
    formLayout->addRow("Auth Prefix:", authPrefixEdit);

    apiKeyValueEdit = new SecureInputWidget(this);
    apiKeyValueEdit->setPlaceholderText(tr("Optional API key value (for reference only)"));
    formLayout->addRow(tr("API Key Value (optional):"), apiKeyValueEdit);

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
    connect(authTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ServiceEditorDialog::onAuthTypeChanged);
    connect(testMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ServiceEditorDialog::onTestMethodChanged);
    connect(importCurlButton, &QPushButton::clicked, this, &ServiceEditorDialog::importFromCurl);

    validateInput();
    onAuthTypeChanged(authTypeCombo->currentIndex());
    onTestMethodChanged(testMethodCombo->currentIndex());
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
    service.testBody = testBodyEdit->toPlainText().toStdString();
    service.testable = testableCheckBox->isChecked();
    service.isBuiltIn = builtInCheckBox->isChecked();
    
    QString preset = authTypeCombo->currentData().toString();
    if (preset == "header_bearer") {
        service.authMethod = "Bearer";
        service.authLocation = "header";
        service.authParameter = "Authorization";
        service.authPrefix = "Bearer ";
    } else if (preset == "header_basic") {
        service.authMethod = "Basic Auth";
        service.authLocation = "header";
        service.authParameter = "Authorization";
        service.authPrefix = "Basic ";
    } else if (preset == "header_custom") {
        service.authMethod = "Custom Header";
        service.authLocation = "header";
        service.authParameter = authParameterEdit->text().trimmed().toStdString();
        service.authPrefix = authPrefixEdit->text().toStdString();
    } else if (preset == "query_param") {
        service.authMethod = "Query Parameter";
        service.authLocation = "query";
        service.authParameter = authParameterEdit->text().trimmed().toStdString();
        service.authPrefix = authPrefixEdit->text().toStdString();
    } else if (preset == "body_param") {
        service.authMethod = "Body Parameter";
        service.authLocation = "body";
        service.authParameter = authParameterEdit->text().trimmed().toStdString();
        service.authPrefix = authPrefixEdit->text().toStdString();
    } else {
        service.authLocation = "header";
        service.authParameter = authParameterEdit->text().trimmed().toStdString();
        service.authPrefix = authPrefixEdit->text().toStdString();
    }

    if (service.authParameter.empty()) {
        service.authParameter = (service.authLocation == "header") ? "Authorization" : "api_key";
    }

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
    testBodyEdit->setPlainText(QString::fromStdString(service.testBody));
    builtInCheckBox->setChecked(service.isBuiltIn);
    apiKeyValueEdit->setText(QString());
    detectedApiKeyValue.clear();
    lastCurlCommand.clear();

    QString preset = "header_custom";
    if (service.authLocation == "header") {
        if (service.authPrefix == "Bearer " || service.authMethod == "Bearer") {
            preset = "header_bearer";
        } else if (service.authPrefix == "Basic " || service.authMethod == "Basic Auth") {
            preset = "header_basic";
        } else if (service.authMethod == "Custom Header") {
            preset = "header_custom";
        }
    } else if (service.authLocation == "query") {
        preset = "query_param";
    } else if (service.authLocation == "body") {
        preset = "body_param";
    }

    int index = authTypeCombo->findData(preset);
    if (index < 0) index = 0;
    bool blocked = authTypeCombo->blockSignals(true);
    authTypeCombo->setCurrentIndex(index);
    authTypeCombo->blockSignals(blocked);
    onAuthTypeChanged(index);
    authParameterEdit->setText(QString::fromStdString(service.authParameter));
    authPrefixEdit->setText(QString::fromStdString(service.authPrefix));
    onTestMethodChanged(testMethodCombo->currentIndex());

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

QString ServiceEditorDialog::getDetectedApiKeyValue() const
{
    return apiKeyValueEdit ? apiKeyValueEdit->text().trimmed() : QString();
}

void ServiceEditorDialog::validateInput()
{
    bool isValid = !nameEdit->text().isEmpty() && !keyNameEdit->text().isEmpty();
    isValid = isValid && !authParameterEdit->text().trimmed().isEmpty();
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(isValid);
}

void ServiceEditorDialog::onTestableChanged(bool testable)
{
    testEndpointEdit->setEnabled(testable);
    testMethodCombo->setEnabled(testable);
    testHeadersEdit->setEnabled(testable);
    if (!testable) {
        testBodyEdit->setEnabled(false);
    } else {
        onTestMethodChanged(testMethodCombo->currentIndex());
    }
}

void ServiceEditorDialog::updateUi()
{
    onTestableChanged(testableCheckBox->isChecked());
}

void ServiceEditorDialog::onTestMethodChanged(int index)
{
    Q_UNUSED(index)
    QString method = testMethodCombo->currentText();
    bool allowBody = !method.isEmpty() && method != "GET" && method != "HEAD";
    testBodyEdit->setEnabled(allowBody);
    if (!allowBody) {
        testBodyEdit->setPlaceholderText("Bodies are typically ignored for " + method + " requests");
    } else {
        testBodyEdit->setPlaceholderText("Optional JSON body to include when calling the test endpoint");
    }
}

void ServiceEditorDialog::onAuthTypeChanged(int index)
{
    QString preset = authTypeCombo->itemData(index).toString();

    auto ensureDefault = [this](QLineEdit *edit, const QString &value) {
        if (edit->text().isEmpty()) {
            edit->setText(value);
        }
    };

    if (preset == "header_bearer") {
        authParameterEdit->setReadOnly(true);
        authPrefixEdit->setReadOnly(true);
        ensureDefault(authParameterEdit, "Authorization");
        ensureDefault(authPrefixEdit, "Bearer ");
    } else if (preset == "header_basic") {
        authParameterEdit->setReadOnly(true);
        authPrefixEdit->setReadOnly(true);
        ensureDefault(authParameterEdit, "Authorization");
        ensureDefault(authPrefixEdit, "Basic ");
    } else if (preset == "header_custom") {
        authParameterEdit->setReadOnly(false);
        authPrefixEdit->setReadOnly(false);
        ensureDefault(authParameterEdit, "X-API-Key");
    } else if (preset == "query_param") {
        authParameterEdit->setReadOnly(false);
        authPrefixEdit->setReadOnly(false);
        ensureDefault(authParameterEdit, "api_key");
        if (authPrefixEdit->text().isEmpty()) {
            authPrefixEdit->clear();
        }
    } else if (preset == "body_param") {
        authParameterEdit->setReadOnly(false);
        authPrefixEdit->setReadOnly(false);
        ensureDefault(authParameterEdit, "api_key");
        if (authPrefixEdit->text().isEmpty()) {
            authPrefixEdit->clear();
        }
    } else {
        authParameterEdit->setReadOnly(false);
        authPrefixEdit->setReadOnly(false);
    }

    validateInput();
}

void ServiceEditorDialog::importFromCurl()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Import from cURL"));
    dialog.setModal(true);

    QVBoxLayout layout(&dialog);
    QLabel prompt(tr("Paste a full cURL command below:"));
    layout.addWidget(&prompt);

    QPlainTextEdit editor;
    editor.setPlaceholderText(tr("curl --request ..."));
    editor.setPlainText(lastCurlCommand);
    editor.setMinimumHeight(220);
    layout.addWidget(&editor);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout.addWidget(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString command = editor.toPlainText().trimmed();
    if (command.isEmpty()) {
        return;
    }

    auto parsed = parseCurlCommand(command);
    if (!parsed.ok) {
        QMessageBox::warning(this, tr("Invalid cURL Command"), parsed.error);
        return;
    }

    auto analysis = analyzeCurl(parsed);
    if (!analysis.ok) {
        QString message = analysis.error.isEmpty() ? tr("Unable to analyse the provided cURL command.") : analysis.error;
        QMessageBox::warning(this, tr("Import Failed"), message);
        return;
    }

    lastCurlCommand = command;

    if (!analysis.serviceId.isEmpty() && !nameEdit->isReadOnly()) {
        nameEdit->setText(analysis.serviceId);
    }

    if (!analysis.serviceDisplayName.isEmpty() && descriptionEdit->toPlainText().trimmed().isEmpty()) {
        descriptionEdit->setPlainText(analysis.serviceDisplayName);
    }

    if (!analysis.keyName.isEmpty()) {
        keyNameEdit->setText(analysis.keyName);
    }

    testableCheckBox->setChecked(true);
    testEndpointEdit->setText(analysis.url.toString());

    int methodIndex = testMethodCombo->findText(analysis.method, Qt::MatchFixedString);
    if (methodIndex < 0) {
        testMethodCombo->addItem(analysis.method);
        methodIndex = testMethodCombo->count() - 1;
    }
    testMethodCombo->setCurrentIndex(methodIndex);
    onTestMethodChanged(methodIndex);

    testHeadersEdit->setText(buildHeaderString(analysis.otherHeaders));
    testBodyEdit->setPlainText(analysis.body);

    int presetIndex = authTypeCombo->findData(analysis.authPreset);
    if (presetIndex < 0) {
        presetIndex = 0;
    }

    bool blocked = authTypeCombo->blockSignals(true);
    authTypeCombo->setCurrentIndex(presetIndex);
    authTypeCombo->blockSignals(blocked);
    onAuthTypeChanged(presetIndex);

    authParameterEdit->setText(analysis.authParameter);
    authPrefixEdit->setText(analysis.authPrefix);

    detectedApiKeyValue = analysis.apiKeyValue;
    apiKeyValueEdit->setText(analysis.apiKeyValue);

    validateInput();
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
    addButton->setIcon(QIcon::fromTheme("list-add", QIcon(":/icons/add.svg")));
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
            auto results = services::run_tests_parallel(config, testableServices, false, std::string(), false);
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
