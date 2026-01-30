#include "qtui/application.h"
#include "qtui/progressbar.h"
#include "qtui/label.h"
#include "qtui/pushbutton.h"
#include <QCoreApplication>
#include <ncurses.h>

// Simple progress simulation without QTimer to avoid ncurses timeout() conflict
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    auto *tuiApp = qtui::Application::instance();
    
    qtui::TLabel titleLabel;
    titleLabel.setText("File Download Progress");
    titleLabel.setAlignment(Qt::AlignCenter);
    titleLabel.setPosition(5, 0);
    titleLabel.setWidth(60);
    titleLabel.show();
    
    qtui::TLabel statusLabel;
    statusLabel.setText("Initializing...");
    statusLabel.setPosition(7, 10);
    statusLabel.setWidth(50);
    statusLabel.show();
    
    qtui::TProgressBar downloadProgress;
    downloadProgress.setPosition(9, 10);
    downloadProgress.setWidth(50);
    downloadProgress.setRange(0, 100);
    downloadProgress.setValue(0);
    downloadProgress.setFormat("Downloading: %p%");
    downloadProgress.show();
    
    qtui::TProgressBar extractProgress;
    extractProgress.setPosition(12, 10);
    extractProgress.setWidth(50);
    extractProgress.setRange(0, 100);
    extractProgress.setValue(0);
    extractProgress.setFormat("Extracting: %p%");
    extractProgress.show();
    
    qtui::TPushButton cancelButton;
    cancelButton.setText("Cancel");
    cancelButton.setPosition(15, 10);
    cancelButton.show();
    
    // Simulate download
    bool running = true;
    bool cancelled = false;
    int phase = 0; // 0=download, 1=extract, 2=done
    int counter = 0;
    
    QObject::connect(&cancelButton, &qtui::TPushButton::clicked, [&]() {
        cancelled = true;
        running = false;
    });
    
    // Main loop with progress simulation
    ::timeout(50);  // ncurses timeout for getch() in milliseconds
    
    while (running && phase < 2) {
        clear();
        
        // Update progress every few iterations
        if (counter % 2 == 0) {
            if (phase == 0) {
                // Download phase
                int val = downloadProgress.value();
                if (val < 100) {
                    downloadProgress.setValue(val + 2);
                    statusLabel.setText(QString("Downloading file... %1 KB/s").arg(1024 + (val * 10)));
                } else {
                    phase = 1;
                    statusLabel.setText("Download complete. Extracting...");
                }
            } else if (phase == 1) {
                // Extract phase
                int val = extractProgress.value();
                if (val < 100) {
                    extractProgress.setValue(val + 3);
                    statusLabel.setText(QString("Extracting files... %1 files").arg(val * 5));
                } else {
                    phase = 2;
                    statusLabel.setText("Installation complete!");
                }
            }
        }
        
        titleLabel.render();
        statusLabel.render();
        downloadProgress.render();
        extractProgress.render();
        cancelButton.render();
        mvprintw(18, 10, "Click Cancel to abort (or wait for completion)");
        refresh();
        
        counter++;
        
        int ch = getch();
        if (ch == 27 || ch == 'q' || ch == 'Q') {
            cancelled = true;
            running = false;
        } else if (ch == KEY_MOUSE) {
            MEVENT event;
            if (getmouse(&event) == OK && (event.bstate & BUTTON1_CLICKED)) {
                if (cancelButton.handleMouse(event.y, event.x)) {
                    cancelButton.click();
                }
            }
        }
    }
    
    ::timeout(-1);  // Reset to blocking mode
    
    if (!cancelled && phase == 2) {
        clear();
        mvprintw(10, 10, "Installation completed successfully!");
        mvprintw(12, 10, "Press any key to exit...");
        refresh();
        getch();
    }
    
    return 0;
}
