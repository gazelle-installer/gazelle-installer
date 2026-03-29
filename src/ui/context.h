#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

namespace ui {

class Context : public QObject
{
    Q_OBJECT

public:
    enum Mode {
        GUI,
        TUI
    };

    static Context* instance();
    
    static void setMode(Mode mode);
    static Mode mode();
    static bool isGUI();
    static bool isTUI();
    static bool detectXDisplay();
    
    // Force TUI mode (for --tui flag)
    static void forceTUI();

private:
    Context();
    ~Context() override;
    
    Mode m_mode;
    
    static Context* s_instance;
};

} // namespace ui
