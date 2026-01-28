#pragma once

#include "widget.h"
#include <QString>
#include <QList>

namespace qtui {

class RadioButton;

class ButtonGroup : public QObject
{
    Q_OBJECT

public:
    explicit ButtonGroup(QObject *parent = nullptr) noexcept;
    ~ButtonGroup() override;

    void addButton(RadioButton *button) noexcept;
    void removeButton(RadioButton *button) noexcept;
    QList<RadioButton *> buttons() const noexcept { return buttonList; }
    
    RadioButton *checkedButton() const noexcept;
    void setExclusive(bool exclusive) noexcept { this->exclusive = exclusive; }
    bool isExclusive() const noexcept { return exclusive; }

private:
    QList<RadioButton *> buttonList;
    bool exclusive = true;

    friend class RadioButton;
    void uncheckOthers(RadioButton *except) noexcept;
};

class RadioButton : public Widget
{
    Q_OBJECT

public:
    explicit RadioButton(const QString &text = QString(), Widget *parent = nullptr) noexcept;
    ~RadioButton() override;

    void setText(const QString &text) noexcept { this->labelText = text; }
    QString text() const noexcept { return labelText; }

    void setChecked(bool checked) noexcept;
    bool isChecked() const noexcept { return checked; }

    void toggle() noexcept;

    void render() noexcept override;
    
    void setPosition(int row, int col) noexcept { this->row = row; this->col = col; }
    void setFocus(bool focused) noexcept { this->focused = focused; }
    bool hasFocus() const noexcept { return focused; }
    
    void setButtonGroup(ButtonGroup *group) noexcept;
    ButtonGroup *buttonGroup() const noexcept { return group; }
    
    bool handleMouse(int mouseY, int mouseX) noexcept;
    bool handleKey(int key) noexcept;

signals:
    void toggled(bool checked);
    void clicked();

private:
    QString labelText;
    bool checked = false;
    bool focused = false;
    int row = 0;
    int col = 0;
    ButtonGroup *group = nullptr;
    
    friend class ButtonGroup;
};

} // namespace qtui
