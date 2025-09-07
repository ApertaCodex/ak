#pragma once

#ifdef BUILD_GUI

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QProgressBar>
#include <QString>
#include <QStringList>

namespace ak {
namespace gui {
namespace widgets {

// Forward declaration
class SecureInputWidget;

// Base dialog class with common functionality
class BaseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BaseDialog(const QString &title, QWidget *parent = nullptr);

protected:
    void setupButtons(QDialogButtonBox::StandardButtons buttons);
    void addFormRow(const QString &label, QWidget *widget);
    void addWidget(QWidget *widget);
    void setValid(bool valid);

private:
    QVBoxLayout *mainLayout;
    QFormLayout *formLayout;
    QDialogButtonBox *buttonBox;
};

// Key Add/Edit Dialog
class KeyEditDialog : public BaseDialog
{
    Q_OBJECT

public:
    explicit KeyEditDialog(QWidget *parent = nullptr);
    KeyEditDialog(const QString &keyName, const QString &keyValue, 
                  const QString &service, QWidget *parent = nullptr);

    QString getKeyName() const;
    QString getKeyValue() const;
    QString getService() const;

private slots:
    void validateInput();

private:
    void setupUi();
    void populateServices();

    QLineEdit *nameEdit;
    SecureInputWidget *valueEdit;
    QComboBox *serviceCombo;
    bool editMode;
};

// Profile Create Dialog
class ProfileCreateDialog : public BaseDialog
{
    Q_OBJECT

public:
    explicit ProfileCreateDialog(QWidget *parent = nullptr);

    QString getProfileName() const;

private slots:
    void validateInput();

private:
    void setupUi();

    QLineEdit *nameEdit;
};

// Confirmation Dialog
class ConfirmationDialog : public QDialog
{
    Q_OBJECT

public:
    static bool confirm(QWidget *parent, const QString &title, 
                       const QString &message, const QString &details = QString());

    static void showInfo(QWidget *parent, const QString &title, const QString &message);
    static void showError(QWidget *parent, const QString &title, const QString &message);
    static void showWarning(QWidget *parent, const QString &title, const QString &message);

private:
    explicit ConfirmationDialog(const QString &title, const QString &message,
                               const QString &details, QWidget *parent = nullptr);
};

// Progress Dialog
class ProgressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProgressDialog(const QString &title, QWidget *parent = nullptr);

    void setProgress(int value);
    void setMaximum(int maximum);
    void setLabelText(const QString &text);
    void setDetailText(const QString &text);

private:
    void setupUi();

    QLabel *statusLabel;
    QProgressBar *progressBar;
    QTextEdit *detailsText;
};

// Profile Import/Export Dialog
class ProfileImportExportDialog : public BaseDialog
{
    Q_OBJECT

public:
    enum Mode { Import, Export };
    
    explicit ProfileImportExportDialog(Mode mode, QWidget *parent = nullptr);
    explicit ProfileImportExportDialog(Mode mode, const QString &defaultProfileName, QWidget *parent = nullptr);

    QString getFilePath() const;
    QString getFormat() const;
    QString getProfileName() const;
    
    void setDefaultProfileName(const QString &profileName);

private slots:
    void browseFile();
    void validateInput();

private:
    void setupUi();

    Mode mode;
    QLineEdit *filePathEdit;
    QPushButton *browseButton;
    QComboBox *formatCombo;
    QLineEdit *profileNameEdit;
};

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI