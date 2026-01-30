#include "qtui/tlabel.h"
#include "qtui/application.h"
#include <ncurses.h>

namespace qtui {

TLabel::TLabel(const QString &text, Widget *parent) noexcept
    : Widget(parent), labelText(text)
{
}

TLabel::~TLabel() = default;

QStringList TLabel::wrapText(const QString &text, int width) const noexcept
{
    if (width <= 0 || !wordWrap) {
        return QStringList() << text;
    }

    QStringList result;
    QString remaining = text;

    while (!remaining.isEmpty()) {
        if (remaining.length() <= width) {
            result.append(remaining);
            break;
        }

        int breakPos = width;
        for (int i = width; i >= 0; --i) {
            if (remaining[i].isSpace()) {
                breakPos = i;
                break;
            }
        }

        result.append(remaining.left(breakPos).trimmed());
        remaining = remaining.mid(breakPos).trimmed();
    }

    return result;
}

void TLabel::render() noexcept
{
    if (!visible) return;

    if (!enabled) {
        attron(A_DIM);
    }

    QStringList lines;
    if (wordWrap && maxWidth > 0) {
        lines = wrapText(labelText, maxWidth);
    } else {
        lines = labelText.split('\n');
    }

    int currentRow = row;
    for (const QString &line : lines) {
        QString displayLine = line;
        
        if (maxWidth > 0 && displayLine.length() > maxWidth) {
            displayLine = displayLine.left(maxWidth);
        }

        int displayCol = col;
        
        if (align & AlignRight) {
            if (maxWidth > 0) {
                displayCol = col + maxWidth - displayLine.length();
            }
        } else if (align & AlignHCenter) {
            if (maxWidth > 0) {
                displayCol = col + (maxWidth - displayLine.length()) / 2;
            }
        }

        mvprintw(currentRow, displayCol, "%s", displayLine.toUtf8().constData());
        currentRow++;
    }

    if (!enabled) {
        attroff(A_DIM);
    }
}

} // namespace qtui
