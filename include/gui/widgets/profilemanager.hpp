#pragma once

#ifdef BUILD_GUI

#include "core/config.hpp"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QProgressBar>
#include <QGroupBox>
#include <QString>
#include <QStringList>

namespace ak {
namespace gui {
namespace widgets {

// Forward declarations
class ProfileImportExportDialog;

// Profile List Item
class ProfileListItem : public QListWidgetItem
{
public:
    explicit ProfileListItem(const QString &profileName, int keyCount, QListWidget *parent = nullptr);

    QString getProfileName() const;
    int getKeyCount() const;
    void setKeyCount(int count);

private:
    void updateDisplay();

    QString profileName;
    int keyCount;
};

// Profile Manager Widget
class ProfileManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProfileManagerWidget(const core::Config& config, QWidget *parent = nullptr);

    void refreshProfiles();

signals:
    void statusMessage(const QString &message);

private slots:
    void createProfile();
    void deleteProfile();
    void renameProfile();
    void importProfile();
    void exportProfile();
    void onProfileSelectionChanged();
    void onProfileDoubleClicked(QListWidgetItem *item);

private:
    void setupUi();
    void setupProfileList();
    void setupControls();
    void setupProfileDetails();
    void loadProfiles();
    void updateProfileDetails(const QString &profileName);
    void showProfileKeys(const QString &profileName);
    bool validateProfileName(const QString &name);
    void showError(const QString &message);
    void showSuccess(const QString &message);

    // Configuration and data
    const core::Config& config;
    QStringList availableProfiles;
    QString currentProfile;

    // UI components
    QVBoxLayout *mainLayout;
    QHBoxLayout *controlsLayout;

    // Profile list
    QListWidget *profileList;
    QLabel *profileListLabel;

    // Controls
    QPushButton *createButton;
    QPushButton *deleteButton;
    QPushButton *renameButton;
    QPushButton *importButton;
    QPushButton *exportButton;
    QPushButton *refreshButton;

    // Profile details
    QGroupBox *detailsGroup;
    QLabel *profileNameLabel;
    QLabel *keyCountLabel;
    QTextEdit *keysText;

    // Status
    QLabel *statusLabel;
};

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI