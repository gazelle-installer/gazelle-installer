QT       += core gui widgets

TEMPLATE = app
TARGET = minstall
TRANSLATIONS += translations/gazelle-installer_am.ts \
                translations/gazelle-installer_ar.ts \
                translations/gazelle-installer_bg.ts \
                translations/gazelle-installer_bn.ts \
                translations/gazelle-installer_ca.ts \
                translations/gazelle-installer_cs.ts \
                translations/gazelle-installer_da.ts \
                translations/gazelle-installer_de.ts \
                translations/gazelle-installer_el.ts \
                translations/gazelle-installer_es.ts \
                translations/gazelle-installer_et.ts \
                translations/gazelle-installer_eu.ts \
                translations/gazelle-installer_fa.ts \
                translations/gazelle-installer_fi.ts \
                translations/gazelle-installer_fil_PH.ts \
                translations/gazelle-installer_fr.ts \
                translations/gazelle-installer_he_IL.ts \
                translations/gazelle-installer_hi.ts \
                translations/gazelle-installer_hr.ts \
                translations/gazelle-installer_hu.ts \
                translations/gazelle-installer_id.ts \
                translations/gazelle-installer_is.ts \
                translations/gazelle-installer_it.ts \
                translations/gazelle-installer_ja.ts \
                translations/gazelle-installer_kk.ts \
                translations/gazelle-installer_ko.ts \
                translations/gazelle-installer_lt.ts \
                translations/gazelle-installer_mk.ts \
                translations/gazelle-installer_mr.ts \
                translations/gazelle-installer_nb.ts \
                translations/gazelle-installer_nl.ts \
                translations/gazelle-installer_pl.ts \
                translations/gazelle-installer_pt_BR.ts \
                translations/gazelle-installer_pt.ts \
                translations/gazelle-installer_ro.ts \
                translations/gazelle-installer_ru.ts \
                translations/gazelle-installer_sk.ts \
                translations/gazelle-installer_sl.ts \
                translations/gazelle-installer_sq.ts \
                translations/gazelle-installer_sr.ts \
                translations/gazelle-installer_sv.ts \
                translations/gazelle-installer_tr.ts \
                translations/gazelle-installer_uk.ts \
                translations/gazelle-installer_vi.ts \
                translations/gazelle-installer_zh_CN.ts \
                translations/gazelle-installer_zh_TW.ts
FORMS += meinstall.ui
HEADERS += minstall.h \
    mpassedit.h \
    mtreeview.h \
    version.h \
    msettings.h \
    blockdev.h \
    partman.h \
    safecache.h \
    mprocess.h
SOURCES += app.cpp minstall.cpp \
    mpassedit.cpp \
    msettings.cpp \
    blockdev.cpp \
    mtreeview.cpp \
    partman.cpp \
    safecache.cpp \
    mprocess.cpp
LIBS +=
CONFIG += release warn_on thread qt c++11

RESOURCES += \
    images.qrc
