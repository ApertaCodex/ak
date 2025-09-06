#ifdef BUILD_GUI

#include "gui/widgets/common/secureinput.hpp"
#include <QApplication>
#include <QStyle>

namespace ak {
namespace gui {
namespace widgets {

SecureInputWidget::SecureInputWidget(QWidget *parent)
    : QWidget(parent), lineEdit(nullptr), toggleButton(nullptr), layout(nullptr),
      masked(true), valid(true)
{
    setupUi();
    updateVisibility();
    updateStyleSheet();
}

void SecureInputWidget::setupUi()
{
    layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    
    lineEdit = new QLineEdit(this);
    lineEdit->setEchoMode(QLineEdit::Password);
    
    toggleButton = new QPushButton(this);
    toggleButton->setFixedSize(24, 24);
    toggleButton->setFlat(true);
    toggleButton->setCursor(Qt::PointingHandCursor);
    toggleButton->setToolTip("Toggle visibility");
    
    layout->addWidget(lineEdit);
    layout->addWidget(toggleButton);
    
    // Connect signals
    connect(lineEdit, &QLineEdit::textChanged, this, &SecureInputWidget::onTextChanged);
    connect(lineEdit, &QLineEdit::editingFinished, this, &SecureInputWidget::editingFinished);
    connect(toggleButton, &QPushButton::clicked, this, &SecureInputWidget::toggleVisibility);
}

void SecureInputWidget::setText(const QString &text)
{
    originalText = text;
    lineEdit->setText(text);
}

QString SecureInputWidget::text() const
{
    return lineEdit->text();
}

void SecureInputWidget::setPlaceholderText(const QString &text)
{
    lineEdit->setPlaceholderText(text);
}

void SecureInputWidget::setReadOnly(bool readOnly)
{
    lineEdit->setReadOnly(readOnly);
    toggleButton->setEnabled(!readOnly);
}

bool SecureInputWidget::isReadOnly() const
{
    return lineEdit->isReadOnly();
}

void SecureInputWidget::setMasked(bool masked)
{
    this->masked = masked;
    updateVisibility();
}

bool SecureInputWidget::isMasked() const
{
    return masked;
}

void SecureInputWidget::setValid(bool valid)
{
    this->valid = valid;
    updateStyleSheet();
}

bool SecureInputWidget::isValid() const
{
    return valid;
}

void SecureInputWidget::toggleVisibility()
{
    setMasked(!masked);
}

void SecureInputWidget::onTextChanged(const QString &text)
{
    originalText = text;
    emit textChanged(text);
}

void SecureInputWidget::updateVisibility()
{
    if (masked) {
        lineEdit->setEchoMode(QLineEdit::Password);
        toggleButton->setText("👁");
        toggleButton->setToolTip("Show value");
    } else {
        lineEdit->setEchoMode(QLineEdit::Normal);
        toggleButton->setText("🙈");
        toggleButton->setToolTip("Hide value");
    }
}

void SecureInputWidget::updateStyleSheet()
{
    QString style = "QLineEdit { padding: 4px; border: 1px solid %1; border-radius: 3px; }";
    if (!valid) {
        style = style.arg("#ff6b6b");
        lineEdit->setToolTip("Invalid value");
    } else {
        style = style.arg("#ccc");
        lineEdit->setToolTip("");
    }
    lineEdit->setStyleSheet(style);
}

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI