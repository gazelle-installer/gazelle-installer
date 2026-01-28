#pragma once

#include <QObject>
#include <QString>

namespace qtui {

class Widget : public QObject
{
    Q_OBJECT

public:
    explicit Widget(Widget *parent = nullptr) noexcept;
    ~Widget() override;

    virtual void show() noexcept;
    virtual void hide() noexcept;
    virtual void render() noexcept = 0;
    
    bool isVisible() const noexcept { return visible; }
    void setEnabled(bool enabled) noexcept { this->enabled = enabled; }
    bool isEnabled() const noexcept { return enabled; }

protected:
    bool visible = false;
    bool enabled = true;
    Widget *parentWidget = nullptr;
};

} // namespace qtui
