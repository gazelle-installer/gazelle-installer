#include "qcombobox.h"

namespace ui {

QComboBox::QComboBox(QWidget* parent)
    : QObject(parent)
    , m_guiComboBox(nullptr)
    , m_tuiComboBox(nullptr)
{
    if (Context::isGUI()) {
        m_guiComboBox = new ::QComboBox(parent);
        connect(m_guiComboBox, QOverload<int>::of(&::QComboBox::currentIndexChanged), 
                this, &QComboBox::onGuiCurrentIndexChanged);
        connect(m_guiComboBox, &::QComboBox::currentTextChanged, 
                this, &QComboBox::onGuiCurrentTextChanged);
    } else {
        m_tuiComboBox = new qtui::TComboBox();
        connect(m_tuiComboBox, &qtui::TComboBox::currentIndexChanged, 
                this, &QComboBox::onTuiCurrentIndexChanged);
    }
}

QComboBox::~QComboBox()
{
    delete m_guiComboBox;
    delete m_tuiComboBox;
}

void QComboBox::addItem(const QString& text, const QVariant& userData)
{
    if (m_guiComboBox) {
        m_guiComboBox->addItem(text, userData);
    } else if (m_tuiComboBox) {
        m_tuiComboBox->addItem(text, userData);
    }
}

void QComboBox::addItems(const QStringList& texts)
{
    if (m_guiComboBox) {
        m_guiComboBox->addItems(texts);
    } else if (m_tuiComboBox) {
        m_tuiComboBox->addItems(texts);
    }
}

void QComboBox::clear()
{
    if (m_guiComboBox) {
        m_guiComboBox->clear();
    } else if (m_tuiComboBox) {
        m_tuiComboBox->clear();
    }
}

int QComboBox::count() const
{
    if (m_guiComboBox) {
        return m_guiComboBox->count();
    } else if (m_tuiComboBox) {
        return m_tuiComboBox->count();
    }
    return 0;
}

QString QComboBox::currentText() const
{
    if (m_guiComboBox) {
        return m_guiComboBox->currentText();
    } else if (m_tuiComboBox) {
        return m_tuiComboBox->currentText();
    }
    return QString();
}

int QComboBox::currentIndex() const
{
    if (m_guiComboBox) {
        return m_guiComboBox->currentIndex();
    } else if (m_tuiComboBox) {
        return m_tuiComboBox->currentIndex();
    }
    return -1;
}

QVariant QComboBox::currentData() const
{
    if (m_guiComboBox) {
        return m_guiComboBox->currentData();
    } else if (m_tuiComboBox) {
        return m_tuiComboBox->currentData();
    }
    return QVariant();
}

void QComboBox::setCurrentIndex(int index)
{
    if (m_guiComboBox) {
        m_guiComboBox->setCurrentIndex(index);
    } else if (m_tuiComboBox) {
        m_tuiComboBox->setCurrentIndex(index);
    }
}

void QComboBox::setCurrentText(const QString& text)
{
    if (m_guiComboBox) {
        m_guiComboBox->setCurrentText(text);
    } else if (m_tuiComboBox) {
        // Find item with matching text and set its index
        for (int i = 0; i < m_tuiComboBox->count(); ++i) {
            if (m_tuiComboBox->itemText(i) == text) {
                m_tuiComboBox->setCurrentIndex(i);
                break;
            }
        }
    }
}

QString QComboBox::itemText(int index) const
{
    if (m_guiComboBox) {
        return m_guiComboBox->itemText(index);
    } else if (m_tuiComboBox) {
        return m_tuiComboBox->itemText(index);
    }
    return QString();
}

QVariant QComboBox::itemData(int index) const
{
    if (m_guiComboBox) {
        return m_guiComboBox->itemData(index);
    } else if (m_tuiComboBox) {
        return m_tuiComboBox->itemData(index);
    }
    return QVariant();
}

void QComboBox::setEnabled(bool enabled)
{
    if (m_guiComboBox) {
        m_guiComboBox->setEnabled(enabled);
    } else if (m_tuiComboBox) {
        m_tuiComboBox->setEnabled(enabled);
    }
}

bool QComboBox::isEnabled() const
{
    if (m_guiComboBox) {
        return m_guiComboBox->isEnabled();
    } else if (m_tuiComboBox) {
        return m_tuiComboBox->isEnabled();
    }
    return false;
}

void QComboBox::setPosition(int row, int col)
{
    if (m_tuiComboBox) {
        m_tuiComboBox->setPosition(row, col);
    }
}

void QComboBox::setWidth(int width)
{
    if (m_tuiComboBox) {
        m_tuiComboBox->setWidth(width);
    }
}

QWidget* QComboBox::widget() const
{
    return m_guiComboBox;
}

qtui::Widget* QComboBox::tuiWidget() const
{
    return m_tuiComboBox;
}

void QComboBox::show()
{
    if (m_guiComboBox) {
        m_guiComboBox->show();
    } else if (m_tuiComboBox) {
        m_tuiComboBox->show();
    }
}

void QComboBox::hide()
{
    if (m_guiComboBox) {
        m_guiComboBox->hide();
    } else if (m_tuiComboBox) {
        m_tuiComboBox->hide();
    }
}

void QComboBox::onGuiCurrentIndexChanged(int index)
{
    emit currentIndexChanged(index);
}

void QComboBox::onGuiCurrentTextChanged(const QString& text)
{
    emit currentTextChanged(text);
}

void QComboBox::onTuiCurrentIndexChanged(int index)
{
    emit currentIndexChanged(index);
    emit currentTextChanged(currentText());
}

} // namespace ui
