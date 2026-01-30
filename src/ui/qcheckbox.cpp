#include "qcheckbox.h"

namespace ui {

QCheckBox::QCheckBox(QWidget* parent)
    : QObject(parent)
    , m_guiCheckBox(nullptr)
    , m_tuiCheckBox(nullptr)
{
    if (Context::isGUI()) {
        m_guiCheckBox = new ::QCheckBox(parent);
        connect(m_guiCheckBox, &::QCheckBox::toggled, this, &QCheckBox::onGuiToggled);
        connect(m_guiCheckBox, &::QCheckBox::clicked, this, &QCheckBox::onGuiClicked);
    } else {
        m_tuiCheckBox = new qtui::TCheckBox();
        connect(m_tuiCheckBox, &qtui::TCheckBox::toggled, this, &QCheckBox::onTuiToggled);
        connect(m_tuiCheckBox, &qtui::TCheckBox::clicked, this, &QCheckBox::onTuiClicked);
    }
}

QCheckBox::~QCheckBox()
{
    delete m_guiCheckBox;
    delete m_tuiCheckBox;
}

void QCheckBox::setText(const QString& text)
{
    if (m_guiCheckBox) {
        m_guiCheckBox->setText(text);
    } else if (m_tuiCheckBox) {
        m_tuiCheckBox->setText(text);
    }
}

QString QCheckBox::text() const
{
    if (m_guiCheckBox) {
        return m_guiCheckBox->text();
    } else if (m_tuiCheckBox) {
        return m_tuiCheckBox->text();
    }
    return QString();
}

void QCheckBox::setChecked(bool checked)
{
    if (m_guiCheckBox) {
        m_guiCheckBox->setChecked(checked);
    } else if (m_tuiCheckBox) {
        m_tuiCheckBox->setChecked(checked);
    }
}

bool QCheckBox::isChecked() const
{
    if (m_guiCheckBox) {
        return m_guiCheckBox->isChecked();
    } else if (m_tuiCheckBox) {
        return m_tuiCheckBox->isChecked();
    }
    return false;
}

void QCheckBox::setEnabled(bool enabled)
{
    if (m_guiCheckBox) {
        m_guiCheckBox->setEnabled(enabled);
    } else if (m_tuiCheckBox) {
        m_tuiCheckBox->setEnabled(enabled);
    }
}

bool QCheckBox::isEnabled() const
{
    if (m_guiCheckBox) {
        return m_guiCheckBox->isEnabled();
    } else if (m_tuiCheckBox) {
        return m_tuiCheckBox->isEnabled();
    }
    return false;
}

void QCheckBox::setPosition(int row, int col)
{
    if (m_tuiCheckBox) {
        m_tuiCheckBox->setPosition(row, col);
    }
}

void QCheckBox::setFocus(bool focus)
{
    if (m_guiCheckBox) {
        if (focus) {
            m_guiCheckBox->setFocus();
        } else {
            m_guiCheckBox->clearFocus();
        }
    } else if (m_tuiCheckBox) {
        m_tuiCheckBox->setFocus(focus);
    }
}

bool QCheckBox::hasFocus() const
{
    if (m_guiCheckBox) {
        return m_guiCheckBox->hasFocus();
    } else if (m_tuiCheckBox) {
        return m_tuiCheckBox->hasFocus();
    }
    return false;
}

QWidget* QCheckBox::widget() const
{
    return m_guiCheckBox;
}

qtui::Widget* QCheckBox::tuiWidget() const
{
    return m_tuiCheckBox;
}

void QCheckBox::show()
{
    if (m_guiCheckBox) {
        m_guiCheckBox->show();
    } else if (m_tuiCheckBox) {
        m_tuiCheckBox->show();
    }
}

void QCheckBox::hide()
{
    if (m_guiCheckBox) {
        m_guiCheckBox->hide();
    } else if (m_tuiCheckBox) {
        m_tuiCheckBox->hide();
    }
}

void QCheckBox::onGuiToggled(bool checked)
{
    emit toggled(checked);
}

void QCheckBox::onGuiClicked()
{
    emit clicked();
}

void QCheckBox::onTuiToggled(bool checked)
{
    emit toggled(checked);
}

void QCheckBox::onTuiClicked()
{
    emit clicked();
}

} // namespace ui