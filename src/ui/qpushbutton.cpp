#include "qpushbutton.h"

namespace ui {

QPushButton::QPushButton(const QString& text, QWidget* parent)
    : QObject(parent)
    , m_guiButton(nullptr)
    , m_tuiButton(nullptr)
{
    if (Context::isGUI()) {
        m_guiButton = new ::QPushButton(text, parent);
        connect(m_guiButton, &::QPushButton::clicked, this, &QPushButton::onGuiClicked);
    } else {
        m_tuiButton = new qtui::TPushButton();
        if (!text.isEmpty()) {
            m_tuiButton->setText(text);
        }
        connect(m_tuiButton, &qtui::TPushButton::clicked, this, &QPushButton::onTuiClicked);
    }
}

QPushButton::~QPushButton()
{
    delete m_guiButton;
    delete m_tuiButton;
}

void QPushButton::setText(const QString& text)
{
    if (m_guiButton) {
        m_guiButton->setText(text);
    } else if (m_tuiButton) {
        m_tuiButton->setText(text);
    }
}

QString QPushButton::text() const
{
    if (m_guiButton) {
        return m_guiButton->text();
    } else if (m_tuiButton) {
        return m_tuiButton->text();
    }
    return QString();
}

void QPushButton::setEnabled(bool enabled)
{
    if (m_guiButton) {
        m_guiButton->setEnabled(enabled);
    } else if (m_tuiButton) {
        m_tuiButton->setEnabled(enabled);
    }
}

bool QPushButton::isEnabled() const
{
    if (m_guiButton) {
        return m_guiButton->isEnabled();
    } else if (m_tuiButton) {
        return m_tuiButton->isEnabled();
    }
    return false;
}

void QPushButton::setPosition(int row, int col)
{
    if (m_tuiButton) {
        m_tuiButton->setPosition(row, col);
    }
}

QWidget* QPushButton::widget() const
{
    return m_guiButton;
}

qtui::Widget* QPushButton::tuiWidget() const
{
    return m_tuiButton;
}

void QPushButton::show()
{
    if (m_guiButton) {
        m_guiButton->show();
    } else if (m_tuiButton) {
        m_tuiButton->show();
    }
}

void QPushButton::hide()
{
    if (m_guiButton) {
        m_guiButton->hide();
    } else if (m_tuiButton) {
        m_tuiButton->hide();
    }
}

void QPushButton::onGuiClicked()
{
    emit clicked();
}

void QPushButton::onTuiClicked()
{
    emit clicked();
}

} // namespace ui
