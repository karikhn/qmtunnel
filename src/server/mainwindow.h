/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidgetItem>
#include "../lib/cJSON.h"
#include "../lib/mgrclient-parameters.h"
#include <QMetaType>
#include <QCloseEvent>
#include <QSystemTrayIcon>
#include <QAbstractSocket>
#include "widget_mgrserver.h"
#include "widget_usergroups.h"

Q_DECLARE_METATYPE(QTreeWidgetItem*);


extern cJSON *j_gui_settings;
extern QSystemTrayIcon *trayIcon;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

private slots:

  void on_action_UISettings_triggered();
  void on_action_exit_triggered();
  void on_action_About_triggered();
  void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
  void trayIconToggleState();

  void mgrServerConfig_modified();
  void mgrServerConfig_load(cJSON *j_config);
  void on_btnMgrApplyAndSave_clicked();
  void on_btnMgrCancel_clicked();
  void mgrServerSocketError(QAbstractSocket::SocketError error, const QString &errorString);

  void server_started();
  void server_stopped();
  void on_btnStartStop_clicked();

public slots:
  void firstRun();
  void noOpenSSL();

protected:
  void closeEvent(QCloseEvent *event);
  void changeEvent(QEvent *event);

signals:
  void gui_closing();
  void mgrServerConfig_changed(cJSON *json);
  void gui_start_server();
  void gui_stop_server();

private:
  Ui::MainWindow *ui;

  WidgetMgrServer *widget_mgrServer;
  WidgetUserGroups *widget_userGroups;

  void load_ui_settings();
  void save_ui_settings();

  void mgrconfig_loadParams();
  void mgrconfig_saveParams();
  void update_mgrConfigButtons();


};

extern MainWindow *mainWindow;

#endif // MAINWINDOW_H
