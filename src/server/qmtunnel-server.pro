#-------------------------------------------------
#
# Project created by QtCreator 2017-02-14T13:28:02
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

unix:TARGET = ../../bin/qmtunnel-server
win32:TARGET = ../../../bin/qmtunnel-server
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
    ../lib/mgrclient-conn.cpp \
    ../lib/mgrclient-parameters.cpp \
    ../lib/mgrserver-parameters.cpp \
    mgr_server.cpp \
    mgr_server_conn_init.cpp \
    ../lib/client-conn-parameters.cpp \
    mgr_server_config.cpp \
    ../lib/ssl_helper.cpp \
    mgr_server_auth.cpp \
    ../lib/ssl_keygen.cpp \
    ../lib/tunnel-parameters.cpp \
    widget_mgrserver.cpp \
    widget_usergroups.cpp \
    mgr_server_tun.cpp \
    ../lib/tunnel-state.cpp \
    tunnel.cpp \
    tunnel_conn.cpp \
    ../lib/mgr_packet.cpp \
    tunnel_chain.cpp \
    tunnel_appdata.cpp \
    tunnel_appconn.cpp \
    aboutdialog.cpp \
    gui_settings.cpp \
    gui_settings_dialog.cpp

HEADERS  += mainwindow.h \
    ../lib/cJSON.h \
    ../lib/prc_log.h \
    ../lib/sys_util.h \
    ../lib/mgrclient-conn.h \
    ../lib/mgrclient-parameters.h \
    ../lib/mgrserver-parameters.h \
    mgr_server.h \
    ../lib/mgr_packet.h \
    ../lib/client-conn-parameters.h \
    mgrclient-state.h \
    ../lib/ssl_helper.h \
    ../lib/ssl_keygen.h \
    ../lib/tunnel-parameters.h \
    widget_mgrserver.h \
    widget_usergroups.h \
    ../lib/tunnel-state.h \
    tunnel.h \
    tunnel_conn.h \
    aboutdialog.h \
    gui_settings.h \
    gui_settings_dialog.h

FORMS    += mainwindow.ui \
    widget_mgrserver.ui \
    widget_usergroups.ui \
    aboutdialog.ui \
    gui_settings_dialog.ui

RESOURCES += \
    server.qrc

RC_FILE = server.rc
