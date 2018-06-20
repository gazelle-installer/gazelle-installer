QT       += core gui widgets

TEMPLATE = app
TARGET = minstall
TRANSLATIONS += translations/gazelle-installer_am.ts \
                translations/gazelle-installer_bg.ts \
                translations/gazelle-installer_ca.ts \
                translations/gazelle-installer_cs.ts \
                translations/gazelle-installer_de.ts \
                translations/gazelle-installer_el.ts \
                translations/gazelle-installer_es.ts \
                translations/gazelle-installer_fi.ts \
                translations/gazelle-installer_fr.ts \
                translations/gazelle-installer_hi.ts \
                translations/gazelle-installer_hr.ts \
                translations/gazelle-installer_hu.ts \
                translations/gazelle-installer_it.ts \
                translations/gazelle-installer_ja.ts \
                translations/gazelle-installer_kk.ts \
                translations/gazelle-installer_lt.ts \
                translations/gazelle-installer_nl.ts \
                translations/gazelle-installer_pl.ts \
                translations/gazelle-installer_pt_BR.ts \
                translations/gazelle-installer_pt.ts \
                translations/gazelle-installer_ro.ts \
                translations/gazelle-installer_ru.ts \
                translations/gazelle-installer_sk.ts \
                translations/gazelle-installer_sv.ts \
                translations/gazelle-installer_tr.ts \
                translations/gazelle-installer_uk.ts \
                translations/gazelle-installer_zh_TW.ts
FORMS += memain.ui meinstall.ui
HEADERS += mmain.h minstall.h
SOURCES += app.cpp mmain.cpp minstall.cpp
LIBS += -lcrypt -lcmd
CONFIG += release warn_on thread qt

#RESOURCES += \
#    images.qrc
