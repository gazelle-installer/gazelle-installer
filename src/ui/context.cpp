#include "context.h"
#include <cstdlib>

namespace ui {

Context* Context::s_instance = nullptr;

Context::Context()
    : m_mode(GUI)
{
}

Context::~Context()
{
}

Context* Context::instance()
{
    if (!s_instance) {
        s_instance = new Context();
    }
    return s_instance;
}

void Context::setMode(Mode mode)
{
    instance()->m_mode = mode;
}

Context::Mode Context::mode()
{
    return instance()->m_mode;
}

bool Context::isGUI()
{
    return mode() == GUI;
}

bool Context::isTUI()
{
    return mode() == TUI;
}

bool Context::detectXDisplay()
{
    const char* display = std::getenv("DISPLAY");
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    
    return (display && display[0] != '\0') || (wayland && wayland[0] != '\0');
}

void Context::forceTUI()
{
    setMode(TUI);
}

} // namespace ui
