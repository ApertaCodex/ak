#pragma once

#ifdef BUILD_GUI

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QString>

namespace ak {
namespace gui {
namespace widgets {

class SecureInputWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SecureInputWidget(QWidget *parent = nullptr);
    
    void setText(const QString &text);
    QString text() const;
    void setPlaceholderText(const QString &text);
    void setReadOnly(bool readOnly);
    bool isReadOnly() const;
    
    // Show/hide functionality
    void setMasked(bool masked);
    bool isMasked() const;
    
    // Validation
    void setValid(bool valid);
    bool isValid() const;

signals:
    void textChanged(const QString &text);
    void editingFinished();

private slots:
    void toggleVisibility();
    void onTextChanged(const QString &text);

private:
    void setupUi();
    void updateVisibility();
    void updateStyleSheet();
    
    QLineEdit *lineEdit;
    QPushButton *toggleButton;
    QHBoxLayout *layout;
    
    bool masked;
    bool valid;
    QString originalText;
};

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI