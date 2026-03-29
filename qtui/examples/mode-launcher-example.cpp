// Example: Mode-detecting launcher for gazelle-installer
// This shows how to auto-detect X availability and launch appropriate mode

#include <QApplication>
#include <QCoreApplication>
#include <QString>
#include <cstdlib>

// Forward declarations
int gui_main(int argc, char *argv[]);
int tui_main(int argc, char *argv[]);

// Check if X display is available
bool isXAvailable() {
    // Check common display environment variables
    const char* display = std::getenv("DISPLAY");
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    
    return (display != nullptr && display[0] != '\0') ||
           (wayland != nullptr && wayland[0] != '\0');
}

// Parse command line for mode override
enum class RequestedMode {
    Auto,
    ForceGUI,
    ForceTUI
};

RequestedMode parseArgs(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromUtf8(argv[i]);
        if (arg == "--tui" || arg == "--cli" || arg == "--text") {
            return RequestedMode::ForceTUI;
        }
        if (arg == "--gui" || arg == "--x11" || arg == "--graphical") {
            return RequestedMode::ForceGUI;
        }
    }
    return RequestedMode::Auto;
}

int main(int argc, char *argv[]) {
    // Determine which mode to use
    RequestedMode requested = parseArgs(argc, argv);
    bool useTUI = false;
    
    switch (requested) {
        case RequestedMode::ForceGUI:
            useTUI = false;
            if (!isXAvailable()) {
                fprintf(stderr, "Warning: --gui requested but no X display available\n");
                fprintf(stderr, "Falling back to TUI mode\n");
                useTUI = true;
            }
            break;
            
        case RequestedMode::ForceTUI:
            useTUI = true;
            break;
            
        case RequestedMode::Auto:
            useTUI = !isXAvailable();
            if (useTUI) {
                fprintf(stderr, "No X display detected, using TUI mode\n");
                fprintf(stderr, "Use --gui to force GUI mode (requires X)\n");
            }
            break;
    }
    
    // Launch appropriate interface
    if (useTUI) {
        return tui_main(argc, argv);
    } else {
        return gui_main(argc, argv);
    }
}

// GUI version entry point
int gui_main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Your existing GUI code here
    // #include "meinstall.h"
    // Meinstall installer;
    // installer.show();
    
    return app.exec();
}

// TUI version entry point  
int tui_main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    // TUI code here
    // #include "qtui/application.h"
    // #include "tui/installer-tui.h"
    // auto *tuiApp = qtui::Application::instance();
    // tuiApp->enableMouse();
    // 
    // TUIInstaller installer;
    // return installer.exec();
    
    return 0;
}
