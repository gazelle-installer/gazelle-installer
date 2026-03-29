#pragma once

#include "widget.h"
#include <QString>
#include <QList>

namespace qtui {

class TRadioButton;

class TButtonGroup : public QObject
{
    Q_OBJECT

public:
    explicit TButtonGroup(QObject *parent = nullptr) noexcept;
    ~TButtonGroup() override;

    void addButton(TRadioButton *button) noexcept;
    void removeButton(TRadioButton *button) noexcept;
    QList<TRadioButton *> buttons() const noexcept { return buttonList; }
    
    TRadioButton *checkedButton() const noexcept;
    void setExclusive(bool exclusive) noexcept { this->exclusive = exclusive; }
    bool isExclusive() const noexcept { return exclusive; }

private:
    QList<TRadioButton *> buttonList;
    bool exclusive = true;

    friend class TRadioButton;
    void uncheckOthers(TRadioButton *except) noexcept;
};

class TRadioButton : public Widget
{
    Q_OBJECT

public:
    explicit TRadioButton(const QString &text = QString(), Widget *parent = nullptr) noexcept;
    ~TRadioButton() override;

    void setText(const QString &text) noexcept { this->labelText = text; }
    QString text() const noexcept { return labelText; }

    void setChecked(bool checked) noexcept;
    bool isChecked() const noexcept { return checked; }

    void toggle() noexcept;

    void render() noexcept override;
    
    void setPosition(int row, int col) noexcept { this->row = row; this->col = col; }
    void setFocus(bool focused) noexcept { this->focused = focused; }
    bool hasFocus() const noexcept { return focused; }
    
    void setButtonGroup(TButtonGroup *group) noexcept;
    TButtonGroup *buttonGroup() const noexcept { return group; }
    
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
    TButtonGroup *group = nullptr;
    
    friend class TButtonGroup;
};

} // namespace qtui
