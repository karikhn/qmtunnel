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
#include <QTimer>
#include <QSystemTrayIcon>
#include "../server/widget_mgrserver.h"
#include "../server/widget_usergroups.h"
#include "widget_mgrclient.h"
#include "widget_tunnelparams.h"
#include "widget_tunnelconnlist.h"
#include "../lib/tunnel-state.h"

#define GUI_MAJOR_VERSION     0
#define GUI_MINOR_VERSION     1

#define MgrClientParametersRole          Qt::UserRole+1
#define MgrConnStateRole                 Qt::UserRole+2
#define ItemTypeRole                     Qt::UserRole+3
#define MgrServerConfigRole              Qt::UserRole+4
#define MgrConnAccessFlagsRole           Qt::UserRole+5
#define TunnelConfigRole                 Qt::UserRole+6
#define TunnelStateRole                  Qt::UserRole+7
#define TunnelEditableRole               Qt::UserRole+8
//#define TunnelStateLogRole               Qt::UserRole+9

enum
{
  ITEM_TYPE_FOLDER=1
 ,ITEM_TYPE_MGRCONN=2
 ,ITEM_TYPE_TUNNEL=3
};

Q_DECLARE_METATYPE(QTreeWidgetItem*);
Q_DECLARE_METATYPE(cJSON*);


extern cJSON *j_gui_settings;
extern bool cur_profile_modified;
extern bool cur_mgrconn_changed;
extern QSystemTrayIcon *trayIcon;

extern MgrServerParameters gui_mgrServerParams;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

  void mod_profile_init();

public slots:
  void firstRun();
  void noOpenSSL();

private slots:

  void on_action_profile_new_triggered();
  void on_action_profile_open_triggered();
  void on_action_profile_save_triggered();
  void on_action_profile_saveAs_triggered();
  void on_action_UISettings_triggered();
  void on_action_exit_triggered();
  void on_action_About_triggered();
  void profile_open_recent();
  void profile_modified();
  void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
  void trayIconToggleState();

  void on_mgrconn_tabWidget_currentChanged(int index);
  void mgrServerConfig_modified();
  void on_btnMgrServerConfigApply_clicked();
  void on_btnMgrServerConfigCancel_clicked();

  void mgrConn_modified();
  void on_btnCancel_clicked();
  void on_btnApply_clicked();
  void on_btnApplyAndSave_clicked();

  void tunnelParams_modified();
  void update_tunnelConfig_buttons();
  void on_btnTunnelConfigCancel_clicked();
  void on_btnTunnelConfigApply_clicked();
  void mgrconn_tunnel_create_reply(quint32 conn_id, TunnelId orig_tun_id, TunnelId tun_id, quint16 res_code, const QString &error);
  void tunnel_config_received(quint32 conn_id, cJSON *j_config);
  void tunnel_state_received(quint32 conn_id, TunnelId tun_id, TunnelState new_state);
  void tunnel_connstate_received(quint32 conn_id, const QByteArray &data);

  void on_browserTree_customContextMenuRequested(const QPoint &pos);

  void on_chkTunnelStatsAutoUpdate_toggled(bool checked);
  void on_spinTunnelStatsAutoUpdateInterval_valueChanged(double value);
  void tunnelStats_autoUpdate();
  void tunnelConnList_autoUpdate(bool now=false);

  void browserTree_on_hostMenu_connect();
  void browserTree_on_hostMenu_disconnect();
  void browserTree_on_hostMenu_new();
  void browserTree_on_hostMenu_remove();
  void browserTree_on_hostMenu_newTunnel();

  void browserTree_on_tunnelMenu_remove();
  void browserTree_on_tunnelMenu_start();
  void browserTree_on_tunnelMenu_stop();

  void browserTree_on_folderMenu_new();
  void browserTree_on_folderMenu_remove();

  void on_browserTree_currentItemChanged(QTreeWidgetItem *cur_item, QTreeWidgetItem *prev_item);
  void on_browserTree_itemChanged(QTreeWidgetItem *item, int column);
  void on_browserTree_itemDoubleClicked(QTreeWidgetItem *item, int column);

  void connection_state_changed(quint32 conn_id, quint16 mgr_conn_state);
  void connection_authorized(quint32 conn_id, quint64 access_flags);
  void connection_error(quint32 conn_id, QAbstractSocket::SocketError error);
  void connection_latency_changed(quint32 conn_id, quint32 latency_ms);
  void connection_password_required(quint32 conn_id);
  void connection_passphrase_required(quint32 conn_id);
  void mgrconn_config_received(quint32 conn_id, cJSON *j_config);
  void mgrconn_config_error(quint32 conn_id, quint16 res_code, const QString &error);
  void mgrconn_tunnel_config_set_reply(quint32 conn_id, TunnelId tun_id, quint16 res_code, const QString &error);
  void mgrconn_tunnel_remove(quint32 conn_id, TunnelId tun_id, quint16 res_code, const QString &error);

  void currentMgrConn_disconnected();

protected:
  void closeEvent(QCloseEvent *event);
  void changeEvent(QEvent *event);

signals:
  void gui_mgrConnChanged(quint32 conn_id, const MgrClientParameters &params);
  void gui_profileLoaded(cJSON *j_profile);
  void gui_closing();
  void gui_mgrserver_config_get(quint32 conn_id);
  void gui_mgrserver_config_set(quint32 conn_id, cJSON *j_config);

  void gui_mgrconn_set_password(quint32 conn_id, const QString &password);
  void gui_mgrconn_set_passphrase(quint32 conn_id, const QString &passphrase);

  void gui_mgrConnDemand(quint32 conn_id);
  void gui_mgrConnDemand_idleStart(quint32 conn_id);
  void gui_mgrConnDemand_idleStop(quint32 conn_id);
  void gui_mgrConnDemand_disconnect(quint32 conn_id);

  void gui_tunnel_create(quint32 conn_id, cJSON *j_tun_params);
  void gui_tunnel_config_get(quint32 conn_id, TunnelId tun_id=0);
  void gui_tunnel_config_set(quint32 conn_id, cJSON *j_tun_params);
  void gui_tunnel_remove(quint32 conn_id, TunnelId tun_id);
  void gui_tunnel_state_get(quint32 conn_id, TunnelId tun_id=0);
  void gui_tunnel_connstate_get(quint32 conn_id, TunnelId tun_id, bool include_disconnected);
  void gui_tunnel_start(quint32 conn_id, TunnelId tun_id);
  void gui_tunnel_stop(quint32 conn_id, TunnelId tun_id);

private:
  Ui::MainWindow *ui;

  WidgetMgrClient *widget_mgrClient;
  WidgetMgrServer *widget_mgrServer;
  WidgetUserGroups *widget_userGroups;
  WidgetTunnelParams *widget_tunnelParams;
  WidgetTunnelConnList *widget_tunnelConnList;

  QTimer *timer_tunnelStats_autoUpdate;

  QMenu *browserTree_hostMenu;
  QMenu *browserTree_tunnelMenu;
  QMenu *browserTree_folderMenu;
  QMenu *browserTree_menu;
  QTreeWidgetItem *browserTree_popupMenu_item;
  QAction *browserTree_action_connect;
  QAction *browserTree_action_disconnect;
  QAction *browserTree_action_newHost;
  QAction *browserTree_action_removeHost;
  QAction *browserTree_action_newFolder;
  QAction *browserTree_action_removeFolder;
  QAction *browserTree_action_newTunnel;
  QAction *browserTree_action_tun_start;
  QAction *browserTree_action_tun_stop;
  QAction *browserTree_action_tun_remove;
  void mod_browserTree_init();
  void browserTree_clear();
  void mod_browserTree_ui_load(cJSON *json);
  void mod_browserTree_ui_save(cJSON *json);
  void browserTree_removeItem(QTreeWidgetItem *item);
  QTreeWidgetItem *browserTree_getMgrConnById(quint32 conn_id);
  QTreeWidgetItem *browserTree_getTunnelById(quint32 conn_id, quint32 tunnel_id);
  QTreeWidgetItem *browserTree_currentMgrConn();
  QTreeWidgetItem *browserTree_mgrConnItem(QTreeWidgetItem *child_item);

  void page_mgrConn_init();
  void page_mgrConn_clear();
  void page_mgrConn_loadItem(QTreeWidgetItem *host_item);
  void page_mgrConn_saveItem(QTreeWidgetItem *host_item);
  void mgrServerConfig_updateEnabled();
  void update_mgrConn_buttons();
  void update_mgrServerConfigButtons();
  void mgrServerConfig_loadParams();

  void page_tunnel_init();
  void page_tunnel_loadItem(QTreeWidgetItem *tunnel_item);
  void page_tunnel_saveItem(QTreeWidgetItem *tunnel_item);
  void page_tunnel_ui_load(cJSON *json);
  void page_tunnel_ui_save(cJSON *json);

  void load_ui_settings();
  void save_ui_settings();

  QList<QAction *> recent_profile_actions;
  void update_recent_profile_actions();
  void save_recent_profiles();
  bool profile_save(const QString &filename);
  void profile_load(const QString &filename);
  void printBrowserTreeItems(cJSON *j_items, QTreeWidgetItem *parent_item);
  void parseBrowserTreeItems(cJSON *j_items, QTreeWidgetItem *parent_item);

};

extern MainWindow *mainWindow;

#endif // MAINWINDOW_H
