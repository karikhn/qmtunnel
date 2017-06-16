#-------------------------------------------------
#
# Project created by QtCreator 2017-02-14T13:28:02
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

unix:TARGET = ../../bin/qmtunnel-gui
win32:TARGET = ../../../bin/qmtunnel-gui
TEMPLATE = app

win32{
# include OpenSSL (v. <= 1.0.2) from http://slproweb.com/products/Win32OpenSSL.html
# OpenSSL v. >= 1.1 have different library names and is not compatible with Qt yet
LIBS += -LC:/OpenSSL-Win32/lib -llibeay32 -lssleay32
INCLUDEPATH += C:/OpenSSL-Win32/include
# this one is for cJSON library compilation
DEFINES += CJSON_HIDE_SYMBOLS
# and this one is for "__attribute__ ((__packed__))" to work in MinGW
QMAKE_CXXFLAGS += -mno-ms-bitfields
}

OBJECTS_DIR = ./obj/
MOC_DIR = ./moc/

unix:LIBS += -lcrypto

SOURCES += main.cpp\
        mainwindow.cpp \
    ../lib/cJSON.c \
    ../lib/prc_log.cpp \
    ../lib/sys_util.cpp \
    main_browserTree.cpp \
    profile.cpp \
    thread-connections.cpp \
    ../lib/mgrclient-conn.cpp \
    ../lib/mgrclient-parameters.cpp \
    ../server/widget_mgrserver.cpp \
    ../lib/mgrserver-parameters.cpp \
    ../lib/mclineedit.cpp \
    page-mgrconn.cpp \
    ../server/widget_usergroups.cpp \
    ../lib/ssl_helper.cpp \
    ../lib/ssl_keygen.cpp \
    ../lib/tunnel-parameters.cpp \
    widget_mgrclient.cpp \
    widget_tunnelparams.cpp \
    page-tunnel.cpp \
    ../lib/tunnel-state.cpp \
    ../lib/mgr_packet.cpp \
    widget_tunnelconnlist.cpp \
    gui_settings.cpp \
    gui_settings_dialog.cpp \
    aboutdialog.cpp

HEADERS  += mainwindow.h \
    ../lib/cJSON.h \
    ../lib/prc_log.h \
    ../lib/sys_util.h \
    thread-connections.h \
    ../lib/mgrclient-conn.h \
    ../lib/mgrclient-parameters.h \
    ../lib/mgr_packet.h \
    ../server/widget_mgrserver.h \
    ../lib/mgrserver-parameters.h \
    ../lib/mclineedit.h \
    ../server/widget_usergroups.h \
    ../lib/ssl_helper.h \
    ../lib/ssl_keygen.h \
    ../lib/tunnel-parameters.h \
    widget_tunnelparams.h \
    widget_mgrclient.h \
    ../lib/tunnel-state.h \
    widget_tunnelconnlist.h \
    gui_settings.h \
    gui_settings_dialog.h \
    aboutdialog.h

FORMS    += mainwindow.ui \
    ../server/widget_mgrserver.ui \
    ../server/widget_usergroups.ui \
    widget_mgrclient.ui \
    widget_tunnelparams.ui \
    widget_tunnelconnlist.ui \
    gui_settings_dialog.ui \
    aboutdialog.ui

RESOURCES += \
    gui.qrc

RC_FILE = gui.rc
