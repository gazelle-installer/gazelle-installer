#include "qslider.h"

namespace ui {

QSlider::QSlider(QWidget* parent)
    : QObject(parent)
    , m_guiSlider(nullptr)
    , m_tuiSlider(nullptr)
{
    if (Context::isGUI()) {
        m_guiSlider = new ::QSlider(Qt::Horizontal, parent);
        connect(m_guiSlider, &::QSlider::valueChanged,
                this, &QSlider::onGuiValueChanged);
        connect(m_guiSlider, &::QSlider::sliderMoved,
                this, &QSlider::onGuiSliderMoved);
    } else {
        m_tuiSlider = new qtui::TSlider();
        connect(m_tuiSlider, &qtui::TSlider::valueChanged,
                this, &QSlider::onTuiValueChanged);
        connect(m_tuiSlider, &qtui::TSlider::sliderMoved,
                this, &QSlider::onTuiSliderMoved);
    }
}

QSlider::~QSlider()
{
    delete m_guiSlider;
    delete m_tuiSlider;
}

void QSlider::setValue(int value)
{
    if (m_guiSlider) {
        m_guiSlider->setValue(value);
    } else if (m_tuiSlider) {
        m_tuiSlider->setValue(value);
    }
}

int QSlider::value() const
{
    if (m_guiSlider) {
        return m_guiSlider->value();
    } else if (m_tuiSlider) {
        return m_tuiSlider->value();
    }
    return 0;
}

void QSlider::setMinimum(int minimum)
{
    if (m_guiSlider) {
        m_guiSlider->setMinimum(minimum);
    } else if (m_tuiSlider) {
        m_tuiSlider->setMinimum(minimum);
    }
}

int QSlider::minimum() const
{
    if (m_guiSlider) {
        return m_guiSlider->minimum();
    } else if (m_tuiSlider) {
        return m_tuiSlider->minimum();
    }
    return 0;
}

void QSlider::setMaximum(int maximum)
{
    if (m_guiSlider) {
        m_guiSlider->setMaximum(maximum);
    } else if (m_tuiSlider) {
        m_tuiSlider->setMaximum(maximum);
    }
}

int QSlider::maximum() const
{
    if (m_guiSlider) {
        return m_guiSlider->maximum();
    } else if (m_tuiSlider) {
        return m_tuiSlider->maximum();
    }
    return 0;
}

void QSlider::setRange(int minimum, int maximum)
{
    if (m_guiSlider) {
        m_guiSlider->setRange(minimum, maximum);
    } else if (m_tuiSlider) {
        m_tuiSlider->setRange(minimum, maximum);
    }
}

void QSlider::setSingleStep(int step)
{
    if (m_guiSlider) {
        m_guiSlider->setSingleStep(step);
    } else if (m_tuiSlider) {
        m_tuiSlider->setSingleStep(step);
    }
}

int QSlider::singleStep() const
{
    if (m_guiSlider) {
        return m_guiSlider->singleStep();
    } else if (m_tuiSlider) {
        return m_tuiSlider->singleStep();
    }
    return 1;
}

void QSlider::setPageStep(int step)
{
    if (m_guiSlider) {
        m_guiSlider->setPageStep(step);
    } else if (m_tuiSlider) {
        m_tuiSlider->setPageStep(step);
    }
}

int QSlider::pageStep() const
{
    if (m_guiSlider) {
        return m_guiSlider->pageStep();
    } else if (m_tuiSlider) {
        return m_tuiSlider->pageStep();
    }
    return 10;
}

void QSlider::setLeftLabel(const QString& label)
{
    if (m_tuiSlider) {
        m_tuiSlider->setLeftLabel(label);
    }
}

void QSlider::setRightLabel(const QString& label)
{
    if (m_tuiSlider) {
        m_tuiSlider->setRightLabel(label);
    }
}

void QSlider::setEnabled(bool enabled)
{
    if (m_guiSlider) {
        m_guiSlider->setEnabled(enabled);
    } else if (m_tuiSlider) {
        m_tuiSlider->setEnabled(enabled);
    }
}

bool QSlider::isEnabled() const
{
    if (m_guiSlider) {
        return m_guiSlider->isEnabled();
    } else if (m_tuiSlider) {
        return m_tuiSlider->isEnabled();
    }
    return false;
}

void QSlider::setPosition(int row, int col)
{
    if (m_tuiSlider) {
        m_tuiSlider->setPosition(row, col);
    }
}

void QSlider::setWidth(int width)
{
    if (m_tuiSlider) {
        m_tuiSlider->setWidth(width);
    }
}

QWidget* QSlider::widget() const
{
    return m_guiSlider;
}

qtui::Widget* QSlider::tuiWidget() const
{
    return m_tuiSlider;
}

void QSlider::show()
{
    if (m_guiSlider) {
        m_guiSlider->show();
    } else if (m_tuiSlider) {
        m_tuiSlider->show();
    }
}

void QSlider::hide()
{
    if (m_guiSlider) {
        m_guiSlider->hide();
    } else if (m_tuiSlider) {
        m_tuiSlider->hide();
    }
}

void QSlider::onGuiValueChanged(int value)
{
    emit valueChanged(value);
}

void QSlider::onGuiSliderMoved(int position)
{
    emit sliderMoved(position);
}

void QSlider::onTuiValueChanged(int value)
{
    emit valueChanged(value);
}

void QSlider::onTuiSliderMoved(int position)
{
    emit sliderMoved(position);
}

} // namespace ui
