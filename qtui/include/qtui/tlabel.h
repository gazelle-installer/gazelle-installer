#pragma once

#include "widget.h"
#include <QString>

namespace qtui {

class TLabel : public Widget
{
    Q_OBJECT

public:
    enum Alignment {
        AlignLeft = 0x01,
        AlignRight = 0x02,
        AlignHCenter = 0x04,
        AlignTop = 0x20,
        AlignBottom = 0x40,
        AlignVCenter = 0x80
    };

    explicit TLabel(const QString &text = QString(), Widget *parent = nullptr) noexcept;
    ~TLabel() override;

    void setText(const QString &text) noexcept { this->labelText = text; }
    QString text() const noexcept { return labelText; }

    void setAlignment(int alignment) noexcept { this->align = alignment; }
    int alignment() const noexcept { return align; }

    void setWordWrap(bool wrap) noexcept { this->wordWrap = wrap; }
    bool hasWordWrap() const noexcept { return wordWrap; }

    void render() noexcept override;
    
    void setPosition(int row, int col) noexcept { this->row = row; this->col = col; }
    void setWidth(int width) noexcept { this->maxWidth = width; }

private:
    QString labelText;
    int align = AlignLeft | AlignTop;
    bool wordWrap = false;
    int row = 0;
    int col = 0;
    int maxWidth = 0;

    QStringList wrapText(const QString &text, int width) const noexcept;
};

} // namespace qtui
