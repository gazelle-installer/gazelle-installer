#pragma once

#include "context.h"
#include <QtWidgets/QComboBox>
#include <qtui/tcombobox.h>
#include <QtCore/QObject>
#include <QtCore/QVariant>

namespace ui {

class QComboBox : public QObject
{
    Q_OBJECT

public:
    explicit QComboBox(QWidget* parent = nullptr);
    ~QComboBox() override;
    
    void addItem(const QString& text, const QVariant& userData = QVariant());
    void addItems(const QStringList& texts);
    void clear();
    
    int count() const;
    QString currentText() const;
    int currentIndex() const;
    QVariant currentData() const;
    
    void setCurrentIndex(int index);
    void setCurrentText(const QString& text);
    
    QString itemText(int index) const;
    QVariant itemData(int index) const;
    
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    void setPosition(int row, int col);
    void setWidth(int width);
    
    QWidget* widget() const;
    qtui::Widget* tuiWidget() const;
    
    void show();
    void hide();

signals:
    void currentIndexChanged(int index);
    void currentTextChanged(const QString& text);

private slots:
    void onGuiCurrentIndexChanged(int index);
    void onGuiCurrentTextChanged(const QString& text);
    void onTuiCurrentIndexChanged(int index);

private:
    ::QComboBox* m_guiComboBox;
    qtui::TComboBox* m_tuiComboBox;
};

} // namespace ui
