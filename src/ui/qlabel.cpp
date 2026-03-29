#include "qlabel.h"

namespace ui {

QLabel::QLabel(const QString& text, QWidget* parent)
    : QObject(parent)
    , m_guiLabel(nullptr)
    , m_tuiLabel(nullptr)
{
    if (Context::isGUI()) {
        m_guiLabel = new ::QLabel(text, parent);
    } else {
        m_tuiLabel = new qtui::TLabel();
        if (!text.isEmpty()) {
            m_tuiLabel->setText(text);
        }
    }
}

QLabel::~QLabel()
{
    delete m_guiLabel;
    delete m_tuiLabel;
}

void QLabel::setText(const QString& text)
{
    if (m_guiLabel) {
        m_guiLabel->setText(text);
    } else if (m_tuiLabel) {
        m_tuiLabel->setText(text);
    }
}

QString QLabel::text() const
{
    if (m_guiLabel) {
        return m_guiLabel->text();
    } else if (m_tuiLabel) {
        return m_tuiLabel->text();
    }
    return QString();
}

void QLabel::setAlignment(Qt::Alignment alignment)
{
    if (m_guiLabel) {
        m_guiLabel->setAlignment(alignment);
    } else if (m_tuiLabel) {
        int tuiAlign = qtui::TLabel::AlignLeft;
        if (alignment & Qt::AlignHCenter) {
            tuiAlign = qtui::TLabel::AlignHCenter;
        } else if (alignment & Qt::AlignRight) {
            tuiAlign = qtui::TLabel::AlignRight;
        }
        m_tuiLabel->setAlignment(tuiAlign);
    }
}

Qt::Alignment QLabel::alignment() const
{
    if (m_guiLabel) {
        return m_guiLabel->alignment();
    } else if (m_tuiLabel) {
        int align = m_tuiLabel->alignment();
        if (align & qtui::TLabel::AlignHCenter) {
            return Qt::AlignHCenter;
        } else if (align & qtui::TLabel::AlignRight) {
            return Qt::AlignRight;
        }
        return Qt::AlignLeft;
    }
    return Qt::AlignLeft;
}

void QLabel::setEnabled(bool enabled)
{
    if (m_guiLabel) {
        m_guiLabel->setEnabled(enabled);
    } else if (m_tuiLabel) {
        m_tuiLabel->setEnabled(enabled);
    }
}

bool QLabel::isEnabled() const
{
    if (m_guiLabel) {
        return m_guiLabel->isEnabled();
    } else if (m_tuiLabel) {
        return m_tuiLabel->isEnabled();
    }
    return false;
}

void QLabel::setPosition(int row, int col)
{
    if (m_tuiLabel) {
        m_tuiLabel->setPosition(row, col);
    }
}

void QLabel::setWidth(int width)
{
    if (m_tuiLabel) {
        m_tuiLabel->setWidth(width);
    }
}

QWidget* QLabel::widget() const
{
    return m_guiLabel;
}

qtui::Widget* QLabel::tuiWidget() const
{
    return m_tuiLabel;
}

void QLabel::show()
{
    if (m_guiLabel) {
        m_guiLabel->show();
    } else if (m_tuiLabel) {
        m_tuiLabel->show();
    }
}

void QLabel::hide()
{
    if (m_guiLabel) {
        m_guiLabel->hide();
    } else if (m_tuiLabel) {
        m_tuiLabel->hide();
    }
}

} // namespace ui
