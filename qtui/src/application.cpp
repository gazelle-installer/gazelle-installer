#include "qtui/application.h"
#include <locale.h>

namespace qtui {

Application *Application::appInstance = nullptr;

Application *Application::instance() noexcept
{
    return appInstance;
}

Application::Application(int &argc, char **argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    
    appInstance = this;
    
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_WHITE, COLOR_BLUE);    // Title
        init_pair(2, COLOR_BLACK, COLOR_WHITE);    // Selected button
        init_pair(3, COLOR_RED, -1);               // Critical icon
        init_pair(4, COLOR_YELLOW, -1);            // Warning icon
        init_pair(5, COLOR_CYAN, -1);              // Info icon
    }
}

Application::~Application()
{
    if (mouseEnabled) {
        disableMouse();
    }
    endwin();
    appInstance = nullptr;
}

int Application::exec() noexcept
{
    running = true;
    return 0;
}

void Application::quit() noexcept
{
    running = false;
}

void Application::processEvents() noexcept
{
    timeout(0);
    int ch = getch();
    if (ch != ERR) {
        ungetch(ch);
    }
}

void Application::enableMouse() noexcept
{
    if (!mouseEnabled) {
        mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, nullptr);
        mouseEnabled = true;
    }
}

void Application::disableMouse() noexcept
{
    if (mouseEnabled) {
        mousemask(0, nullptr);
        mouseEnabled = false;
    }
}

} // namespace qtui
