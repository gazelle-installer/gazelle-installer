#include "qlineedit.h"

namespace ui {

QLineEdit::QLineEdit(QWidget* parent)
    : QObject(parent)
    , m_guiLineEdit(nullptr)
    , m_tuiLineEdit(nullptr)
{
    if (Context::isGUI()) {
        m_guiLineEdit = new ::QLineEdit(parent);
        connect(m_guiLineEdit, &::QLineEdit::textChanged, this, &QLineEdit::onGuiTextChanged);
        connect(m_guiLineEdit, &::QLineEdit::textEdited, this, &QLineEdit::onGuiTextEdited);
        connect(m_guiLineEdit, &::QLineEdit::returnPressed, this, &QLineEdit::onGuiReturnPressed);
    } else {
        m_tuiLineEdit = new qtui::TLineEdit();
        connect(m_tuiLineEdit, &qtui::TLineEdit::textChanged, this, &QLineEdit::onTuiTextChanged);
        connect(m_tuiLineEdit, &qtui::TLineEdit::textEdited, this, &QLineEdit::onTuiTextEdited);
        connect(m_tuiLineEdit, &qtui::TLineEdit::returnPressed, this, &QLineEdit::onTuiReturnPressed);
    }
}

QLineEdit::~QLineEdit()
{
    delete m_guiLineEdit;
    delete m_tuiLineEdit;
}

void QLineEdit::setText(const QString& text)
{
    if (m_guiLineEdit) {
        m_guiLineEdit->setText(text);
    } else if (m_tuiLineEdit) {
        m_tuiLineEdit->setText(text);
    }
}

QString QLineEdit::text() const
{
    if (m_guiLineEdit) {
        return m_guiLineEdit->text();
    } else if (m_tuiLineEdit) {
        return m_tuiLineEdit->text();
    }
    return QString();
}

void QLineEdit::setPlaceholderText(const QString& text)
{
    if (m_guiLineEdit) {
        m_guiLineEdit->setPlaceholderText(text);
    } else if (m_tuiLineEdit) {
        m_tuiLineEdit->setPlaceholderText(text);
    }
}

QString QLineEdit::placeholderText() const
{
    if (m_guiLineEdit) {
        return m_guiLineEdit->placeholderText();
    } else if (m_tuiLineEdit) {
        return m_tuiLineEdit->placeholderText();
    }
    return QString();
}

void QLineEdit::setEchoMode(::QLineEdit::EchoMode mode)
{
    if (m_guiLineEdit) {
        m_guiLineEdit->setEchoMode(mode);
    } else if (m_tuiLineEdit) {
        qtui::TLineEdit::EchoMode tuiMode = qtui::TLineEdit::Normal;
        if (mode == ::QLineEdit::Password) {
            tuiMode = qtui::TLineEdit::Password;
        }
        m_tuiLineEdit->setEchoMode(tuiMode);
    }
}

::QLineEdit::EchoMode QLineEdit::echoMode() const
{
    if (m_guiLineEdit) {
        return m_guiLineEdit->echoMode();
    } else if (m_tuiLineEdit) {
        // TLineEdit doesn't have echoMode() getter, check the member directly
        return ::QLineEdit::Normal; // Default to Normal for now
    }
    return ::QLineEdit::Normal;
}

void QLineEdit::setMaxLength(int maxLength)
{
    if (m_guiLineEdit) {
        m_guiLineEdit->setMaxLength(maxLength);
    } else if (m_tuiLineEdit) {
        m_tuiLineEdit->setMaxLength(maxLength);
    }
}

int QLineEdit::maxLength() const
{
    if (m_guiLineEdit) {
        return m_guiLineEdit->maxLength();
    } else if (m_tuiLineEdit) {
        return m_tuiLineEdit->maxLength();
    }
    return 0;
}

void QLineEdit::setEnabled(bool enabled)
{
    if (m_guiLineEdit) {
        m_guiLineEdit->setEnabled(enabled);
    } else if (m_tuiLineEdit) {
        m_tuiLineEdit->setEnabled(enabled);
    }
}

bool QLineEdit::isEnabled() const
{
    if (m_guiLineEdit) {
        return m_guiLineEdit->isEnabled();
    } else if (m_tuiLineEdit) {
        return m_tuiLineEdit->isEnabled();
    }
    return false;
}

void QLineEdit::setPosition(int row, int col)
{
    if (m_tuiLineEdit) {
        m_tuiLineEdit->setPosition(row, col);
    }
}

void QLineEdit::setWidth(int width)
{
    if (m_tuiLineEdit) {
        m_tuiLineEdit->setWidth(width);
    }
}

QWidget* QLineEdit::widget() const
{
    return m_guiLineEdit;
}

qtui::Widget* QLineEdit::tuiWidget() const
{
    return m_tuiLineEdit;
}

void QLineEdit::show()
{
    if (m_guiLineEdit) {
        m_guiLineEdit->show();
    } else if (m_tuiLineEdit) {
        m_tuiLineEdit->show();
    }
}

void QLineEdit::hide()
{
    if (m_guiLineEdit) {
        m_guiLineEdit->hide();
    } else if (m_tuiLineEdit) {
        m_tuiLineEdit->hide();
    }
}

void QLineEdit::setFocus()
{
    if (m_guiLineEdit) {
        m_guiLineEdit->setFocus();
    } else if (m_tuiLineEdit) {
        m_tuiLineEdit->setFocus(true);
    }
}

void QLineEdit::onGuiTextChanged(const QString& text)
{
    emit textChanged(text);
}

void QLineEdit::onGuiTextEdited(const QString& text)
{
    emit textEdited(text);
}

void QLineEdit::onGuiReturnPressed()
{
    emit returnPressed();
}

void QLineEdit::onTuiTextChanged(const QString& text)
{
    emit textChanged(text);
}

void QLineEdit::onTuiTextEdited(const QString& text)
{
    emit textEdited(text);
}

void QLineEdit::onTuiReturnPressed()
{
    emit returnPressed();
}

} // namespace ui
