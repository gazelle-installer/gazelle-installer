#pragma once

#include <QString>
#include <ncurses.h>

namespace qtui {

class Application
{
public:
    static Application *instance() noexcept;
    
    Application(int &argc, char **argv);
    ~Application();
    
    int exec() noexcept;
    void quit() noexcept;
    
    void processEvents() noexcept;
    
    WINDOW *mainWindow() const noexcept { return stdscr; }
    
    void enableMouse() noexcept;
    void disableMouse() noexcept;

private:
    static Application *appInstance;
    bool running = false;
    bool mouseEnabled = false;
};

} // namespace qtui
