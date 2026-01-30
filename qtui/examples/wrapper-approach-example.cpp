// Example: Widget wrapper approach for minimal code changes
// This creates unified widgets that work in both GUI and TUI modes

#ifndef UICHECKBOX_H
#define UICHECKBOX_H

#include <QObject>
#include <QString>
#include <QCheckBox>
#include "qtui/checkbox.h"

// Wrapper that works in both GUI and TUI modes
class UICheckBox : public QObject {
    Q_OBJECT
    
public:
    explicit UICheckBox(QWidget *parent = nullptr);
    ~UICheckBox();
    
    // Unified API (matches Qt)
    void setText(const QString &text);
    QString text() const;
    
    void setChecked(bool checked);
    bool isChecked() const;
    
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    // TUI-specific (no-op in GUI mode)
    void setPosition(int row, int col);
    void render();
    bool handleMouse(int y, int x);
    bool handleKey(int key);
    
    // For adding to layouts in GUI mode
    QWidget* widget() const { return guiWidget; }
    
signals:
    void toggled(bool checked);
    void clicked();
    
private:
    QCheckBox *guiWidget;
    qtui::TCheckBox *tuiWidget;
    bool isGUIMode;
};

#endif // UICHECKBOX_H

// === Implementation ===
#include "uicheckbox.h"
#include <QCoreApplication>

// Determine mode based on QApplication vs QCoreApplication
static bool isGUIMode() {
    return qobject_cast<QApplication*>(QCoreApplication::instance()) != nullptr;
}

UICheckBox::UICheckBox(QWidget *parent)
    : QObject(parent)
    , guiWidget(nullptr)
    , tuiWidget(nullptr)
    , isGUIMode(::isGUIMode())
{
    if (isGUIMode) {
        guiWidget = new QCheckBox(parent);
        connect(guiWidget, &QCheckBox::toggled, this, &UICheckBox::toggled);
        connect(guiWidget, &QCheckBox::clicked, this, &UICheckBox::clicked);
    } else {
        tuiWidget = new qtui::TCheckBox();
        connect(tuiWidget, &qtui::TCheckBox::toggled, this, &UICheckBox::toggled);
        connect(tuiWidget, &qtui::TCheckBox::clicked, this, &UICheckBox::clicked);
    }
}

UICheckBox::~UICheckBox() {
    delete guiWidget;
    delete tuiWidget;
}

void UICheckBox::setText(const QString &text) {
    if (isGUIMode) {
        guiWidget->setText(text);
    } else {
        tuiWidget->setText(text);
    }
}

QString UICheckBox::text() const {
    if (isGUIMode) {
        return guiWidget->text();
    } else {
        return tuiWidget->text();
    }
}

void UICheckBox::setChecked(bool checked) {
    if (isGUIMode) {
        guiWidget->setChecked(checked);
    } else {
        tuiWidget->setChecked(checked);
    }
}

bool UICheckBox::isChecked() const {
    if (isGUIMode) {
        return guiWidget->isChecked();
    } else {
        return tuiWidget->isChecked();
    }
}

void UICheckBox::setEnabled(bool enabled) {
    if (isGUIMode) {
        guiWidget->setEnabled(enabled);
    } else {
        tuiWidget->setEnabled(enabled);
    }
}

bool UICheckBox::isEnabled() const {
    if (isGUIMode) {
        return guiWidget->isEnabled();
    } else {
        return tuiWidget->isEnabled();
    }
}

void UICheckBox::setPosition(int row, int col) {
    if (!isGUIMode) {
        tuiWidget->setPosition(row, col);
    }
    // No-op in GUI mode
}

void UICheckBox::render() {
    if (!isGUIMode) {
        tuiWidget->render();
    }
    // No-op in GUI mode
}

bool UICheckBox::handleMouse(int y, int x) {
    if (!isGUIMode) {
        return tuiWidget->handleMouse(y, x);
    }
    return false;
}

bool UICheckBox::handleKey(int key) {
    if (!isGUIMode) {
        return tuiWidget->handleKey(key);
    }
    return false;
}


// === USAGE EXAMPLE ===

// Before (GUI only):
void OldCode() {
    QCheckBox *checkbox = new QCheckBox("Enable option", this);
    connect(checkbox, &QCheckBox::toggled, this, &MyClass::onToggled);
    layout->addWidget(checkbox);
}

// After (works in both GUI and TUI):
void NewCode() {
    UICheckBox *checkbox = new UICheckBox(this);
    checkbox->setText("Enable option");
    connect(checkbox, &UICheckBox::toggled, this, &MyClass::onToggled);
    
    // GUI mode: add to layout
    if (checkbox->widget()) {
        layout->addWidget(checkbox->widget());
    }
    // TUI mode: set position and show
    else {
        checkbox->setPosition(5, 10);
        checkbox->show();
    }
}

// Even simpler with factory:
void FactoryCode() {
    UICheckBox *checkbox = UIFactory::createCheckBox(this, "Enable option");
    connect(checkbox, &UICheckBox::toggled, this, &MyClass::onToggled);
    
    // Factory handles GUI vs TUI setup automatically
}


// === SUMMARY ===

/*
 * Widget Wrapper Approach:
 *
 * Pros:
 *  - Minimal code changes (mostly search/replace)
 *  - Same signal/slot connections work
 *  - Can switch modes at runtime
 *  - Gradual migration possible
 *
 * Cons:
 *  - Need to create wrapper for each widget type
 *  - Small runtime overhead (virtual calls)
 *  - Need to handle GUI-specific things (layouts) specially
 *  - More complex than parallel implementation
 *
 * Best for:
 *  - Projects that need runtime mode switching
 *  - Codebases with many widget instances
 *  - When you want minimal diff from original code
 *
 * Estimate for gazelle-installer:
 *  - Create 8-10 wrapper classes: ~1 day
 *  - Convert existing code: ~2-3 days
 *  - Testing: ~1 day
 *  - Total: ~4-5 days
 *
 * Compare to parallel implementation:
 *  - Extract core logic: ~1 day
 *  - Create TUI frontend: ~2 days
 *  - Testing: ~1 day
 *  - Total: ~4 days
 *
 * Recommendation: Use parallel implementation (separate GUI/TUI frontends
 * sharing core logic) - it's cleaner, easier to maintain, and actually
 * faster to implement than creating a complete wrapper layer.
 */
