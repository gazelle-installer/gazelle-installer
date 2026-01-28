#include "qtui/messagebox.h"
#include "qtui/application.h"
#include <ncurses.h>
#include <QList>

namespace qtui {

MessageBox::MessageBox(Widget *parent) noexcept
    : Widget(parent)
{
}

MessageBox::~MessageBox() = default;

void MessageBox::drawBox(int y, int x, int height, int width) noexcept
{
    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x + width - 1, ACS_URCORNER);
    mvaddch(y + height - 1, x, ACS_LLCORNER);
    mvaddch(y + height - 1, x + width - 1, ACS_LRCORNER);
    
    for (int i = 1; i < width - 1; ++i) {
        mvaddch(y, x + i, ACS_HLINE);
        mvaddch(y + height - 1, x + i, ACS_HLINE);
    }
    
    for (int i = 1; i < height - 1; ++i) {
        mvaddch(y + i, x, ACS_VLINE);
        mvaddch(y + i, x + width - 1, ACS_VLINE);
    }
}

QString MessageBox::getIconText() const noexcept
{
    switch (iconType) {
        case Critical: return "[!]";
        case Warning:  return "[*]";
        case Information: return "[i]";
        case Question: return "[?]";
        default: return "";
    }
}

QList<MessageBox::StandardButton> MessageBox::getButtonList() const noexcept
{
    QList<StandardButton> result;
    
    if (buttons & Ok) result.append(Ok);
    if (buttons & Yes) result.append(Yes);
    if (buttons & No) result.append(No);
    if (buttons & Cancel) result.append(Cancel);
    if (buttons & Save) result.append(Save);
    if (buttons & Discard) result.append(Discard);
    if (buttons & Close) result.append(Close);
    if (buttons & Abort) result.append(Abort);
    if (buttons & Retry) result.append(Retry);
    if (buttons & Ignore) result.append(Ignore);
    
    return result;
}

QString MessageBox::getButtonText(StandardButton button) const noexcept
{
    switch (button) {
        case Ok: return "OK";
        case Yes: return "Yes";
        case No: return "No";
        case Cancel: return "Cancel";
        case Save: return "Save";
        case Discard: return "Discard";
        case Close: return "Close";
        case Abort: return "Abort";
        case Retry: return "Retry";
        case Ignore: return "Ignore";
        default: return "";
    }
}

void MessageBox::render() noexcept
{
    if (!visible) return;
    
    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);
    
    QList<StandardButton> btnList = getButtonList();
    
    int width = qMax(50, messageText.length() + 10);
    if (!titleText.isEmpty()) {
        width = qMax(width, titleText.length() + 6);
    }
    width = qMin(width, maxX - 4);
    
    int buttonAreaWidth = 0;
    for (auto btn : btnList) {
        buttonAreaWidth += getButtonText(btn).length() + 4;
    }
    buttonAreaWidth += (btnList.size() - 1) * 2;
    width = qMax(width, buttonAreaWidth + 4);
    
    int height = 10;
    int startY = (maxY - height) / 2;
    int startX = (maxX - width) / 2;
    
    clear();
    
    drawBox(startY, startX, height, width);
    
    if (!titleText.isEmpty()) {
        attron(COLOR_PAIR(1) | A_BOLD);
        QString title = " " + titleText + " ";
        mvprintw(startY, startX + (width - title.length()) / 2, "%s", title.toUtf8().constData());
        attroff(COLOR_PAIR(1) | A_BOLD);
    }
    
    QString iconText = getIconText();
    int contentY = startY + 2;
    
    if (!iconText.isEmpty()) {
        int iconColor = 0;
        switch (iconType) {
            case Critical: iconColor = 3; break;
            case Warning: iconColor = 4; break;
            case Information: iconColor = 5; break;
            default: break;
        }
        
        if (iconColor > 0) {
            attron(COLOR_PAIR(iconColor) | A_BOLD);
        }
        mvprintw(contentY, startX + 2, "%s", iconText.toUtf8().constData());
        if (iconColor > 0) {
            attroff(COLOR_PAIR(iconColor) | A_BOLD);
        }
    }
    
    int textStartX = startX + (iconText.isEmpty() ? 2 : 7);
    int textWidth = width - (iconText.isEmpty() ? 4 : 9);
    
    QStringList lines = messageText.split('\n');
    int lineY = contentY;
    for (const QString &line : lines) {
        QString wrappedLine = line;
        if (wrappedLine.length() > textWidth) {
            wrappedLine = wrappedLine.left(textWidth - 3) + "...";
        }
        mvprintw(lineY++, textStartX, "%s", wrappedLine.toUtf8().constData());
        if (lineY >= startY + height - 4) break;
    }
    
    int buttonY = startY + height - 3;
    int totalBtnWidth = buttonAreaWidth;
    int btnStartX = startX + (width - totalBtnWidth) / 2;
    
    int currentX = btnStartX;
    for (int i = 0; i < btnList.size(); ++i) {
        QString btnText = "[ " + getButtonText(btnList[i]) + " ]";
        
        if (i == selectedButton) {
            attron(COLOR_PAIR(2) | A_BOLD);
        }
        
        mvprintw(buttonY, currentX, "%s", btnText.toUtf8().constData());
        
        if (i == selectedButton) {
            attroff(COLOR_PAIR(2) | A_BOLD);
        }
        
        currentX += btnText.length() + 2;
    }
    
    refresh();
}

MessageBox::StandardButton MessageBox::exec() noexcept
{
    if (Application::instance()) {
        Application::instance()->enableMouse();
    }
    
    show();
    
    QList<StandardButton> btnList = getButtonList();
    if (btnList.isEmpty()) {
        btnList.append(Ok);
    }
    
    if (defaultBtn != NoButton) {
        for (int i = 0; i < btnList.size(); ++i) {
            if (btnList[i] == defaultBtn) {
                selectedButton = i;
                break;
            }
        }
    }
    
    bool done = false;
    result = NoButton;
    
    while (!done) {
        render();
        
        timeout(-1);
        int ch = getch();
        
        if (ch == 27) {
            if (escapeBtn != NoButton) {
                result = escapeBtn;
            } else {
                result = btnList.isEmpty() ? NoButton : btnList.last();
            }
            done = true;
        } else if (ch == '\n' || ch == KEY_ENTER || ch == ' ') {
            result = btnList[selectedButton];
            done = true;
        } else if (ch == KEY_LEFT || ch == KEY_UP) {
            selectedButton = (selectedButton - 1 + btnList.size()) % btnList.size();
        } else if (ch == KEY_RIGHT || ch == KEY_DOWN || ch == '\t') {
            selectedButton = (selectedButton + 1) % btnList.size();
        } else if (ch == KEY_MOUSE) {
            MEVENT event;
            if (getmouse(&event) == OK) {
                if (event.bstate & BUTTON1_CLICKED) {
                    int maxY, maxX;
                    getmaxyx(stdscr, maxY, maxX);
                    
                    int width = qMax(50, messageText.length() + 10);
                    if (!titleText.isEmpty()) {
                        width = qMax(width, titleText.length() + 6);
                    }
                    width = qMin(width, maxX - 4);
                    
                    int buttonAreaWidth = 0;
                    for (auto btn : btnList) {
                        buttonAreaWidth += getButtonText(btn).length() + 4;
                    }
                    buttonAreaWidth += (btnList.size() - 1) * 2;
                    width = qMax(width, buttonAreaWidth + 4);
                    
                    int height = 10;
                    int startY = (maxY - height) / 2;
                    int startX = (maxX - width) / 2;
                    int buttonY = startY + height - 3;
                    int totalBtnWidth = buttonAreaWidth;
                    int btnStartX = startX + (width - totalBtnWidth) / 2;
                    
                    if (event.y == buttonY) {
                        int currentX = btnStartX;
                        for (int i = 0; i < btnList.size(); ++i) {
                            QString btnText = "[ " + getButtonText(btnList[i]) + " ]";
                            if (event.x >= currentX && event.x < currentX + btnText.length()) {
                                selectedButton = i;
                                result = btnList[selectedButton];
                                done = true;
                                break;
                            }
                            currentX += btnText.length() + 2;
                        }
                    }
                }
            }
        }
    }
    
    hide();
    clear();
    refresh();
    
    return result;
}

MessageBox::StandardButton MessageBox::critical(Widget *parent, const QString &title,
                                                 const QString &text,
                                                 StandardButtons buttons,
                                                 StandardButton defaultButton) noexcept
{
    MessageBox box(parent);
    box.setIcon(Critical);
    box.setWindowTitle(title);
    box.setText(text);
    box.setStandardButtons(buttons);
    if (defaultButton != NoButton) {
        box.setDefaultButton(defaultButton);
    }
    return box.exec();
}

MessageBox::StandardButton MessageBox::warning(Widget *parent, const QString &title,
                                               const QString &text,
                                               StandardButtons buttons,
                                               StandardButton defaultButton) noexcept
{
    MessageBox box(parent);
    box.setIcon(Warning);
    box.setWindowTitle(title);
    box.setText(text);
    box.setStandardButtons(buttons);
    if (defaultButton != NoButton) {
        box.setDefaultButton(defaultButton);
    }
    return box.exec();
}

MessageBox::StandardButton MessageBox::information(Widget *parent, const QString &title,
                                                   const QString &text,
                                                   StandardButtons buttons,
                                                   StandardButton defaultButton) noexcept
{
    MessageBox box(parent);
    box.setIcon(Information);
    box.setWindowTitle(title);
    box.setText(text);
    box.setStandardButtons(buttons);
    if (defaultButton != NoButton) {
        box.setDefaultButton(defaultButton);
    }
    return box.exec();
}

} // namespace qtui
