#include "qradiobutton.h"

namespace ui {

QRadioButton::QRadioButton(QWidget* parent)
    : QObject(parent)
    , m_guiRadioButton(nullptr)
    , m_tuiRadioButton(nullptr)
{
    if (Context::isGUI()) {
        m_guiRadioButton = new ::QRadioButton(parent);
        connect(m_guiRadioButton, &::QRadioButton::toggled, this, &QRadioButton::onGuiToggled);
    } else {
        m_tuiRadioButton = new qtui::TRadioButton();
        connect(m_tuiRadioButton, &qtui::TRadioButton::toggled, this, &QRadioButton::onTuiToggled);
    }
}

QRadioButton::~QRadioButton()
{
    delete m_guiRadioButton;
    delete m_tuiRadioButton;
}

void QRadioButton::setText(const QString& text)
{
    if (m_guiRadioButton) {
        m_guiRadioButton->setText(text);
    } else if (m_tuiRadioButton) {
        m_tuiRadioButton->setText(text);
    }
}

QString QRadioButton::text() const
{
    if (m_guiRadioButton) {
        return m_guiRadioButton->text();
    } else if (m_tuiRadioButton) {
        return m_tuiRadioButton->text();
    }
    return QString();
}

void QRadioButton::setChecked(bool checked)
{
    if (m_guiRadioButton) {
        m_guiRadioButton->setChecked(checked);
    } else if (m_tuiRadioButton) {
        m_tuiRadioButton->setChecked(checked);
    }
}

bool QRadioButton::isChecked() const
{
    if (m_guiRadioButton) {
        return m_guiRadioButton->isChecked();
    } else if (m_tuiRadioButton) {
        return m_tuiRadioButton->isChecked();
    }
    return false;
}

void QRadioButton::setEnabled(bool enabled)
{
    if (m_guiRadioButton) {
        m_guiRadioButton->setEnabled(enabled);
    } else if (m_tuiRadioButton) {
        m_tuiRadioButton->setEnabled(enabled);
    }
}

bool QRadioButton::isEnabled() const
{
    if (m_guiRadioButton) {
        return m_guiRadioButton->isEnabled();
    } else if (m_tuiRadioButton) {
        return m_tuiRadioButton->isEnabled();
    }
    return false;
}

void QRadioButton::setPosition(int row, int col)
{
    if (m_tuiRadioButton) {
        m_tuiRadioButton->setPosition(row, col);
    }
}

void QRadioButton::setFocus(bool focused)
{
    if (m_tuiRadioButton) {
        m_tuiRadioButton->setFocus(focused);
    }
}

void QRadioButton::setButtonGroup(::QButtonGroup* guiGroup, qtui::TButtonGroup* tuiGroup)
{
    if (m_guiRadioButton && guiGroup) {
        guiGroup->addButton(m_guiRadioButton);
    } else if (m_tuiRadioButton && tuiGroup) {
        m_tuiRadioButton->setButtonGroup(tuiGroup);
    }
}

QWidget* QRadioButton::widget() const
{
    return m_guiRadioButton;
}

qtui::Widget* QRadioButton::tuiWidget() const
{
    return m_tuiRadioButton;
}

void QRadioButton::show()
{
    if (m_guiRadioButton) {
        m_guiRadioButton->show();
    } else if (m_tuiRadioButton) {
        m_tuiRadioButton->show();
    }
}

void QRadioButton::hide()
{
    if (m_guiRadioButton) {
        m_guiRadioButton->hide();
    } else if (m_tuiRadioButton) {
        m_tuiRadioButton->hide();
    }
}

void QRadioButton::onGuiToggled(bool checked)
{
    emit toggled(checked);
}

void QRadioButton::onTuiToggled(bool checked)
{
    emit toggled(checked);
}

} // namespace ui
