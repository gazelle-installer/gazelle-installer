#pragma once

#include "widget.h"
#include <QString>
#include <QList>
#include <QVariant>

namespace qtui {

class ComboBox : public Widget
{
    Q_OBJECT

public:
    explicit ComboBox(Widget *parent = nullptr) noexcept;
    ~ComboBox() override;

    void addItem(const QString &text, const QVariant &userData = QVariant()) noexcept;
    void addItems(const QStringList &texts) noexcept;
    void insertItem(int index, const QString &text, const QVariant &userData = QVariant()) noexcept;
    void removeItem(int index) noexcept;
    void clear() noexcept;

    int count() const noexcept { return items.size(); }
    
    void setCurrentIndex(int index) noexcept;
    int currentIndex() const noexcept { return currentIdx; }
    
    QString currentText() const noexcept;
    QVariant currentData() const noexcept;
    
    QString itemText(int index) const noexcept;
    QVariant itemData(int index) const noexcept;

    void render() noexcept override;
    
    void setPosition(int row, int col) noexcept { this->row = row; this->col = col; }
    void setWidth(int width) noexcept { this->displayWidth = width; }
    void setFocus(bool focused) noexcept { this->focused = focused; }
    bool hasFocus() const noexcept { return focused; }
    
    void showPopup() noexcept { popupVisible = true; }
    void hidePopup() noexcept { popupVisible = false; }
    bool isPopupVisible() const noexcept { return popupVisible; }
    
    bool handleMouse(int mouseY, int mouseX) noexcept;
    bool handleKey(int key) noexcept;

signals:
    void currentIndexChanged(int index);
    void currentTextChanged(const QString &text);
    void activated(int index);

private:
    struct Item {
        QString text;
        QVariant data;
    };

    QList<Item> items;
    int currentIdx = -1;
    int highlightIdx = 0;
    int row = 0;
    int col = 0;
    int displayWidth = 20;
    bool focused = false;
    bool popupVisible = false;

    void selectItem(int index) noexcept;
};

} // namespace qtui
