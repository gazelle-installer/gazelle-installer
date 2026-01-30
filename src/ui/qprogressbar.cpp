#include "qprogressbar.h"

namespace ui {

QProgressBar::QProgressBar(QWidget* parent)
    : QObject(parent)
    , m_guiProgressBar(nullptr)
    , m_tuiProgressBar(nullptr)
{
    if (Context::isGUI()) {
        m_guiProgressBar = new ::QProgressBar(parent);
        connect(m_guiProgressBar, &::QProgressBar::valueChanged, 
                this, &QProgressBar::onGuiValueChanged);
    } else {
        m_tuiProgressBar = new qtui::TProgressBar();
        connect(m_tuiProgressBar, &qtui::TProgressBar::valueChanged, 
                this, &QProgressBar::onTuiValueChanged);
    }
}

QProgressBar::~QProgressBar()
{
    delete m_guiProgressBar;
    delete m_tuiProgressBar;
}

void QProgressBar::setValue(int value)
{
    if (m_guiProgressBar) {
        m_guiProgressBar->setValue(value);
    } else if (m_tuiProgressBar) {
        m_tuiProgressBar->setValue(value);
    }
}

int QProgressBar::value() const
{
    if (m_guiProgressBar) {
        return m_guiProgressBar->value();
    } else if (m_tuiProgressBar) {
        return m_tuiProgressBar->value();
    }
    return 0;
}

void QProgressBar::setMinimum(int minimum)
{
    if (m_guiProgressBar) {
        m_guiProgressBar->setMinimum(minimum);
    } else if (m_tuiProgressBar) {
        m_tuiProgressBar->setMinimum(minimum);
    }
}

int QProgressBar::minimum() const
{
    if (m_guiProgressBar) {
        return m_guiProgressBar->minimum();
    } else if (m_tuiProgressBar) {
        return m_tuiProgressBar->minimum();
    }
    return 0;
}

void QProgressBar::setMaximum(int maximum)
{
    if (m_guiProgressBar) {
        m_guiProgressBar->setMaximum(maximum);
    } else if (m_tuiProgressBar) {
        m_tuiProgressBar->setMaximum(maximum);
    }
}

int QProgressBar::maximum() const
{
    if (m_guiProgressBar) {
        return m_guiProgressBar->maximum();
    } else if (m_tuiProgressBar) {
        return m_tuiProgressBar->maximum();
    }
    return 0;
}

void QProgressBar::setRange(int minimum, int maximum)
{
    if (m_guiProgressBar) {
        m_guiProgressBar->setRange(minimum, maximum);
    } else if (m_tuiProgressBar) {
        m_tuiProgressBar->setRange(minimum, maximum);
    }
}

void QProgressBar::setFormat(const QString& format)
{
    if (m_guiProgressBar) {
        m_guiProgressBar->setFormat(format);
    } else if (m_tuiProgressBar) {
        m_tuiProgressBar->setFormat(format);
    }
}

QString QProgressBar::format() const
{
    if (m_guiProgressBar) {
        return m_guiProgressBar->format();
    } else if (m_tuiProgressBar) {
        return m_tuiProgressBar->format();
    }
    return QString();
}

void QProgressBar::setEnabled(bool enabled)
{
    if (m_guiProgressBar) {
        m_guiProgressBar->setEnabled(enabled);
    } else if (m_tuiProgressBar) {
        m_tuiProgressBar->setEnabled(enabled);
    }
}

bool QProgressBar::isEnabled() const
{
    if (m_guiProgressBar) {
        return m_guiProgressBar->isEnabled();
    } else if (m_tuiProgressBar) {
        return m_tuiProgressBar->isEnabled();
    }
    return false;
}

void QProgressBar::setPosition(int row, int col)
{
    if (m_tuiProgressBar) {
        m_tuiProgressBar->setPosition(row, col);
    }
}

void QProgressBar::setWidth(int width)
{
    if (m_tuiProgressBar) {
        m_tuiProgressBar->setWidth(width);
    }
}

QWidget* QProgressBar::widget() const
{
    return m_guiProgressBar;
}

qtui::Widget* QProgressBar::tuiWidget() const
{
    return m_tuiProgressBar;
}

void QProgressBar::show()
{
    if (m_guiProgressBar) {
        m_guiProgressBar->show();
    } else if (m_tuiProgressBar) {
        m_tuiProgressBar->show();
    }
}

void QProgressBar::hide()
{
    if (m_guiProgressBar) {
        m_guiProgressBar->hide();
    } else if (m_tuiProgressBar) {
        m_tuiProgressBar->hide();
    }
}

void QProgressBar::onGuiValueChanged(int value)
{
    emit valueChanged(value);
}

void QProgressBar::onTuiValueChanged(int value)
{
    emit valueChanged(value);
}

} // namespace ui
