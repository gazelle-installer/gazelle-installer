#pragma once

#include "widget.h"
#include <QString>

namespace qtui {

class TDialog : public Widget
{
    Q_OBJECT

public:
    enum DialogCode {
        Rejected = 0,
        Accepted = 1
    };

    explicit TDialog(Widget *parent = nullptr) noexcept;
    ~TDialog() override;

    void setWindowTitle(const QString &title) noexcept { this->titleText = title; }
    QString windowTitle() const noexcept { return titleText; }

    void setModal(bool modal) noexcept { this->isModal = modal; }
    bool modal() const noexcept { return isModal; }

    int exec() noexcept;
    void render() noexcept override;

    void accept() noexcept;
    void reject() noexcept;
    void done(int result) noexcept;

    int result() const noexcept { return dialogResult; }

signals:
    void accepted();
    void rejected();
    void finished(int result);

protected:
    virtual void renderContent() noexcept;  // Override in subclasses

    QString titleText;
    int row = 2;
    int col = 5;
    int height = 20;
    int width = 60;
    bool isModal = true;
    int dialogResult = Rejected;
    bool dialogActive = false;
};

} // namespace qtui
