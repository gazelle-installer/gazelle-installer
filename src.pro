QT       += core gui widgets
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_UP_TO=0x050F00

TEMPLATE = app
TARGET = minstall
VERSION = 5.2
CONFIG += debug_and_release warn_on strict_c++ c++17
CONFIG(release, debug|release) {
    DEFINES += NDEBUG
    QMAKE_CXXFLAGS += -flto=auto
    QMAKE_LFLAGS += -flto=auto
}
TRANSLATIONS += translations/gazelle-installer_en.ts

DEFINES += CODEBASE_VERSION=\\\"$${VERSION}\\\"

FORMS += meinstall.ui
HEADERS += \
    minstall.h \
    autopart.h \
    base.h \
    bootman.h \
    checkmd5.h \
    mtreeview.h \
    oobe.h \
    passedit.h \
    swapman.h \
    version.h \
    msettings.h \
    partman.h \
    mprocess.h
SOURCES += \
    app.cpp \
    minstall.cpp \
    autopart.cpp \
    base.cpp \
    bootman.cpp \
    checkmd5.cpp \
    msettings.cpp \
    mtreeview.cpp \
    oobe.cpp \
    partman.cpp \
    passedit.cpp \
    mprocess.cpp \
    swapman.cpp
LIBS += -lzxcvbn

RESOURCES += \
    images.qrc
