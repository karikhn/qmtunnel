/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "gui_settings.h"
#include <QFileDialog>
#include <QSslCertificate>
#include <QSslKey>
#include <QMessageBox>
#include "../lib/mgrclient-conn.h"

bool mgr_server_params_gui_modified=false;
bool mgr_server_params_modified_on_server_while_editing=false;
bool cur_mgrconn_changed = false;
MgrServerParameters serverConfig_applied;
bool mgr_server_config_set_sent=false;

//---------------------------------------------------------------------------
void MainWindow::page_mgrConn_init()
{
  widget_mgrClient = new WidgetMgrClient(0);
  ui->vLayoutTabConnection->insertWidget(ui->vLayoutTabConnection->indexOf(ui->lblMgrConnIntro)+1, widget_mgrClient, 10);
  connect(widget_mgrClient, SIGNAL(paramsModified()), this, SLOT(mgrConn_modified()));

  widget_mgrServer = new WidgetMgrServer(true);
  ui->mgrconn_tabWidget->addTab(widget_mgrServer, "Server management");
  connect(widget_mgrServer, SIGNAL(paramsModified()), this, SLOT(mgrServerConfig_modified()));

  widget_userGroups = new WidgetUserGroups(true);
  ui->mgrconn_tabWidget->addTab(widget_userGroups, "Users && Groups");
  connect(widget_userGroups, SIGNAL(paramsModified()), this, SLOT(mgrServerConfig_modified()));

  mgrServerConfig_updateEnabled();

  ui->widget_mgrServerButtons->setVisible(false);

  QFont fnt = ui->lblMgrConnIntro->font();
  fnt.setPointSizeF(fnt.pointSizeF()*0.9);
  ui->lblMgrConnIntro->setFont(fnt);
}

//---------------------------------------------------------------------------
void MainWindow::page_mgrConn_clear()
{
  ui->mgrconn_tabWidget->setCurrentIndex(0);
  mgrServerConfig_updateEnabled();

  // Connection tab
  widget_mgrClient->clearParams();

  cur_mgrconn_changed = false;
  update_mgrConn_buttons();
}

//---------------------------------------------------------------------------
void MainWindow::mgrConn_modified()
{
  cur_mgrconn_changed = true;
  update_mgrConn_buttons();
}

//---------------------------------------------------------------------------
void MainWindow::update_mgrConn_buttons()
{
  QTreeWidgetItem *cur_item = ui->browserTree->currentItem();
  bool can_apply = cur_item && cur_item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_MGRCONN && widget_mgrClient->canApply();
  ui->btnApply->setEnabled(cur_mgrconn_changed && can_apply);
  ui->btnApplyAndSave->setEnabled(can_apply && (cur_mgrconn_changed || cur_profile_modified));
  ui->btnCancel->setEnabled(cur_mgrconn_changed && cur_item && cur_item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_MGRCONN);
}

//---------------------------------------------------------------------------
void MainWindow::on_btnCancel_clicked()
{
  QTreeWidgetItem *cur_item = ui->browserTree->currentItem();
  if (!cur_item || cur_item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_MGRCONN)
    return;
  page_mgrConn_loadItem(cur_item);
}

//---------------------------------------------------------------------------
void MainWindow::on_btnApply_clicked()
{
  QTreeWidgetItem *cur_item = ui->browserTree->currentItem();
  if (!cur_item || cur_item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_MGRCONN)
    return;
  page_mgrConn_saveItem(cur_item);
  profile_modified();
  update_mgrConn_buttons();
}

//---------------------------------------------------------------------------
void MainWindow::on_btnApplyAndSave_clicked()
{
  on_btnApply_clicked();
  on_action_profile_save_triggered();
}

//---------------------------------------------------------------------------
void MainWindow::page_mgrConn_loadItem(QTreeWidgetItem *host_item)
{
  ui->mgrconn_tabWidget->setCurrentIndex(0);
  mgrServerConfig_updateEnabled();

  // Connection tab
  if (!host_item || host_item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_MGRCONN)
    return;

  MgrClientParameters *mgrconn_params = host_item->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
  widget_mgrClient->loadParams(mgrconn_params);
  mgrServerConfig_loadParams();

  cur_mgrconn_changed = false;
  update_mgrConn_buttons();

  if (mgrconn_params->conn_type == MgrClientParameters::CONN_DEMAND &&
      host_item->data(1, MgrConnStateRole).toInt() != MgrClientConnection::MGR_CONNECTED &&
      host_item->data(1, MgrConnStateRole).toInt() != MgrClientConnection::MGR_CONNECTING)
  {
    emit gui_mgrConnDemand(mgrconn_params->id);
  }
}

//---------------------------------------------------------------------------
void MainWindow::on_mgrconn_tabWidget_currentChanged(int index)
{
  ui->widget_mgrServerButtons->setVisible(index > 0);
}

//---------------------------------------------------------------------------
void MainWindow::page_mgrConn_saveItem(QTreeWidgetItem *host_item)
{
  if (!host_item || host_item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_MGRCONN)
    return;
  // Connection tab
  MgrClientParameters *mgrconn_params = host_item->data(0, MgrClientParametersRole).value<MgrClientParameters *>();

  widget_mgrClient->saveParams(mgrconn_params);
  host_item->setText(0, mgrconn_params->name);

  cur_mgrconn_changed = false;
  if (receivers(SIGNAL(gui_mgrConnChanged(quint32,MgrClientParameters))) > 0)
  {
    emit gui_mgrConnChanged(mgrconn_params->id, *mgrconn_params);
  }
}

//---------------------------------------------------------------------------
void MainWindow::mgrconn_config_received(quint32 conn_id, cJSON *j_config)
{
  QTreeWidgetItem *item = browserTree_getMgrConnById(conn_id);
  if (!item)
  {
    cJSON_Delete(j_config);
    return;
  }
  cJSON *j_old_params = item->data(0, MgrServerConfigRole).value<cJSON *>();
  MgrServerParameters old_params;
  if (j_old_params)
  {
    old_params.parseJSON(j_old_params);
    cJSON_Delete(j_old_params);
  }
  MgrServerParameters *new_params = new MgrServerParameters;
  new_params->parseJSON(j_config);
  item->setData(0, MgrServerConfigRole, QVariant::fromValue(j_config));
  if (item != ui->browserTree->currentItem())
    return;

  if (!mgr_server_params_gui_modified)
  {
    mgrServerConfig_loadParams();
    mgr_server_config_set_sent = false;
  }
  else if (!mgr_server_config_set_sent)
  {
    mgr_server_params_modified_on_server_while_editing = true;
  }
  else if (new_params->config_version == old_params.config_version+1)
  {
    mgrServerConfig_loadParams();
    widget_mgrServer->setEnabled(true);
    widget_userGroups->setEnabled(true);
    mgrServerConfig_updateEnabled();
    mgr_server_params_gui_modified = false;
    mgr_server_config_set_sent = false;
    update_mgrServerConfigButtons();
  }
}

//---------------------------------------------------------------------------
void MainWindow::mgrconn_config_error(quint32 conn_id, quint16 res_code, const QString &error)
{
  QTreeWidgetItem *item = browserTree_getMgrConnById(conn_id);
  if (!item || item != ui->browserTree->currentItem())
    return;

  mgr_server_config_set_sent = false;
  widget_mgrServer->setEnabled(true);
  widget_userGroups->setEnabled(true);
  update_mgrServerConfigButtons();

  if (res_code == MgrServerParameters::RES_CODE_LISTEN_ERROR ||
      res_code == MgrServerParameters::RES_CODE_SSL_CERT_ERROR ||
      res_code == MgrServerParameters::RES_CODE_PRIVATE_KEY_ERROR)
    widget_mgrServer->applyError(res_code);

  QMessageBox::critical(this, tr("Configuration error"), error, QMessageBox::Ok);
}

//---------------------------------------------------------------------------
void MainWindow::mgrServerConfig_updateEnabled()
{
  QTreeWidgetItem *cur_item = ui->browserTree->currentItem();
  bool server_config_avail = cur_item &&
      cur_item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_MGRCONN &&
      cur_item->data(0, MgrServerConfigRole).value<cJSON *>() != NULL &&
      cur_item->data(1, MgrConnStateRole).toInt() == MgrClientConnection::MGR_CONNECTED;
  ui->mgrconn_tabWidget->setTabEnabled(ui->mgrconn_tabWidget->indexOf(widget_mgrServer), server_config_avail);
  ui->mgrconn_tabWidget->setTabEnabled(ui->mgrconn_tabWidget->indexOf(widget_userGroups), server_config_avail);
}

//---------------------------------------------------------------------------
void MainWindow::mgrServerConfig_loadParams()
{
  QTreeWidgetItem *cur_item = ui->browserTree->currentItem();
  if (!cur_item || cur_item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_MGRCONN)
  {
    mgrServerConfig_updateEnabled();
    return;
  }
  cJSON *j_serverConfig = cur_item->data(0, MgrServerConfigRole).value<cJSON *>();
  if (!j_serverConfig)
  {
    mgrServerConfig_updateEnabled();
    return;
  }
  MgrServerParameters serverConfig;
  serverConfig.parseJSON(j_serverConfig);
  serverConfig_applied = serverConfig;
  cJSON *j_network_interfaces = cJSON_GetObjectItem(j_serverConfig, "networkInterfaces");
  if (j_network_interfaces && j_network_interfaces->type == cJSON_Array)
    widget_mgrServer->loadNetworkInterfaces(j_network_interfaces);
  widget_mgrServer->loadParams(&serverConfig);
  widget_userGroups->loadParams(&serverConfig);
  mgrServerConfig_updateEnabled();

  mgr_server_params_gui_modified = false;
  mgr_server_params_modified_on_server_while_editing = false;
  update_mgrServerConfigButtons();
}

//---------------------------------------------------------------------------
void MainWindow::update_mgrServerConfigButtons()
{
  bool can_apply = widget_mgrServer->canApply() && widget_userGroups->canApply();
  ui->btnMgrServerConfigApply->setEnabled(mgr_server_params_gui_modified && can_apply);
  ui->btnMgrServerConfigCancel->setEnabled(mgr_server_params_gui_modified);
}

//---------------------------------------------------------------------------
void MainWindow::mgrServerConfig_modified()
{
  mgr_server_params_gui_modified = true;
  update_mgrServerConfigButtons();
}

//---------------------------------------------------------------------------
void MainWindow::on_btnMgrServerConfigApply_clicked()
{
  QTreeWidgetItem *cur_item = ui->browserTree->currentItem();
  if (!cur_item || cur_item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_MGRCONN)
    return;
  MgrClientParameters *mgrconn_params = cur_item->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
  if (receivers(SIGNAL(gui_mgrserver_config_set(quint32, cJSON *))) == 0)
    return;

  if (mgr_server_params_modified_on_server_while_editing)
  {
    if (QMessageBox::warning(this, tr("Configuration modified"), tr("ATTENTION!\n"
                                                                    "Server configuration has been modified by someone else since you started making your changes.\n"
                                                                    "Are you sure you want to override those modifications with your changes?\n"
                                                                    "If not, press No and then Cancel & Restore to reload current server configuration."),
                                QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
      return;
  }

  widget_mgrServer->saveParams(&serverConfig_applied);
  widget_userGroups->saveParams(&serverConfig_applied);
  cJSON *j_config = cJSON_CreateObject();
  serverConfig_applied.printJSON(j_config);
  widget_mgrServer->setEnabled(false);
  widget_userGroups->setEnabled(false);
  ui->btnMgrServerConfigApply->setEnabled(false);
  ui->btnMgrServerConfigCancel->setEnabled(false);

  mgr_server_config_set_sent = true;
  emit gui_mgrserver_config_set(mgrconn_params->id, j_config);
}

//---------------------------------------------------------------------------
void MainWindow::on_btnMgrServerConfigCancel_clicked()
{
  mgrServerConfig_loadParams();
}
