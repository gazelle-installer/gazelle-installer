QT       += core gui widgets
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_UP_TO=0x050F00

TEMPLATE = app
TARGET = minstall
CONFIG += debug_and_release warn_on strict_c++ c++17
CONFIG(release, debug|release) {
    DEFINES += NDEBUG
    QMAKE_CXXFLAGS += -flto
    QMAKE_LFLAGS += -flto
}
TRANSLATIONS += translations/gazelle-installer_af.ts \
                translations/gazelle-installer_am.ts \
                translations/gazelle-installer_ar.ts \
                translations/gazelle-installer_be.ts \
                translations/gazelle-installer_bg.ts \
                translations/gazelle-installer_bn.ts \
                translations/gazelle-installer_bs.ts \
                translations/gazelle-installer_ca.ts \
                translations/gazelle-installer_ceb.ts \
                translations/gazelle-installer_co.ts \
                translations/gazelle-installer_cs.ts \
                translations/gazelle-installer_cy.ts \
                translations/gazelle-installer_da.ts \
                translations/gazelle-installer_de.ts \
                translations/gazelle-installer_el.ts \
                translations/gazelle-installer_en_GB.ts \
                translations/gazelle-installer_en_US.ts \
                translations/gazelle-installer_eo.ts \
                translations/gazelle-installer_es_ES.ts \
                translations/gazelle-installer_es.ts \
                translations/gazelle-installer_et.ts \
                translations/gazelle-installer_eu.ts \
                translations/gazelle-installer_fa.ts \
                translations/gazelle-installer_fil_PH.ts \
                translations/gazelle-installer_fil.ts \
                translations/gazelle-installer_fi.ts \
                translations/gazelle-installer_fr_BE.ts \
                translations/gazelle-installer_fr.ts \
                translations/gazelle-installer_fy.ts \
                translations/gazelle-installer_ga.ts \
                translations/gazelle-installer_gd.ts \
                translations/gazelle-installer_gl_ES.ts \
                translations/gazelle-installer_gl.ts \
                translations/gazelle-installer_gu.ts \
                translations/gazelle-installer_ha.ts \
                translations/gazelle-installer_haw.ts \
                translations/gazelle-installer_he_IL.ts \
                translations/gazelle-installer_he.ts \
                translations/gazelle-installer_hi.ts \
                translations/gazelle-installer_hr.ts \
                translations/gazelle-installer_ht.ts \
                translations/gazelle-installer_hu.ts \
                translations/gazelle-installer_hy.ts \
                translations/gazelle-installer_id.ts \
                translations/gazelle-installer_is.ts \
                translations/gazelle-installer_it.ts \
                translations/gazelle-installer_ja.ts \
                translations/gazelle-installer_jv.ts \
                translations/gazelle-installer_ka.ts \
                translations/gazelle-installer_kk.ts \
                translations/gazelle-installer_km.ts \
                translations/gazelle-installer_kn.ts \
                translations/gazelle-installer_ko.ts \
                translations/gazelle-installer_ku.ts \
                translations/gazelle-installer_ky.ts \
                translations/gazelle-installer_lb.ts \
                translations/gazelle-installer_lo.ts \
                translations/gazelle-installer_lt.ts \
                translations/gazelle-installer_lv.ts \
                translations/gazelle-installer_mg.ts \
                translations/gazelle-installer_mi.ts \
                translations/gazelle-installer_mk.ts \
                translations/gazelle-installer_ml.ts \
                translations/gazelle-installer_mn.ts \
                translations/gazelle-installer_mr.ts \
                translations/gazelle-installer_ms.ts \
                translations/gazelle-installer_mt.ts \
                translations/gazelle-installer_my.ts \
                translations/gazelle-installer_nb_NO.ts \
                translations/gazelle-installer_nb.ts \
                translations/gazelle-installer_ne.ts \
                translations/gazelle-installer_nl_BE.ts \
                translations/gazelle-installer_nl.ts \
                translations/gazelle-installer_ny.ts \
                translations/gazelle-installer_or.ts \
                translations/gazelle-installer_pa.ts \
                translations/gazelle-installer_pl.ts \
                translations/gazelle-installer_ps.ts \
                translations/gazelle-installer_pt_BR.ts \
                translations/gazelle-installer_pt.ts \
                translations/gazelle-installer_ro.ts \
                translations/gazelle-installer_rue.ts \
                translations/gazelle-installer_ru.ts \
                translations/gazelle-installer_rw.ts \
                translations/gazelle-installer_sd.ts \
                translations/gazelle-installer_si.ts \
                translations/gazelle-installer_sk.ts \
                translations/gazelle-installer_sl.ts \
                translations/gazelle-installer_sm.ts \
                translations/gazelle-installer_sn.ts \
                translations/gazelle-installer_so.ts \
                translations/gazelle-installer_sq.ts \
                translations/gazelle-installer_sr.ts \
                translations/gazelle-installer_st.ts \
                translations/gazelle-installer_su.ts \
                translations/gazelle-installer_sv.ts \
                translations/gazelle-installer_sw.ts \
                translations/gazelle-installer_ta.ts \
                translations/gazelle-installer_te.ts \
                translations/gazelle-installer_tg.ts \
                translations/gazelle-installer_th.ts \
                translations/gazelle-installer_tk.ts \
                translations/gazelle-installer_tr.ts \
                translations/gazelle-installer_tt.ts \
                translations/gazelle-installer_ug.ts \
                translations/gazelle-installer_uk.ts \
                translations/gazelle-installer_ur.ts \
                translations/gazelle-installer_uz.ts \
                translations/gazelle-installer_vi.ts \
                translations/gazelle-installer_xh.ts \
                translations/gazelle-installer_yi.ts \
                translations/gazelle-installer_yo.ts \
                translations/gazelle-installer_yue_CN.ts \
                translations/gazelle-installer_zh_CN.ts \
                translations/gazelle-installer_zh_HK.ts \
                translations/gazelle-installer_zh_TW.ts

FORMS += meinstall.ui
HEADERS += minstall.h \
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
SOURCES += app.cpp minstall.cpp \
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
