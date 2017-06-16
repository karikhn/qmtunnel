/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "gui_settings.h"
#include <QMessageBox>
#include "../lib/sys_util.h"
#include "../lib/prc_log.h"

#define UI_PAGE_TUNNEL_VERSION            1

bool cur_tunnel_config_changed=false;
extern QHash<quint64, QTreeWidgetItem *> tunnel_list;    // key is: first 32 bits = conn_id, last 32 bits = tunnel_id

QTime t_last_tunnel_stats_requested;
QTime t_last_tunnel_connstate_requested;
bool tunnel_stats_reply_received=false;
bool tunnel_connstate_reply_received=false;

QTreeWidgetItem *tsi_conn_total_count = 0;
QTreeWidgetItem *tsi_conn_cur_count = 0;
QTreeWidgetItem *tsi_conn_failed_count = 0;
QTreeWidgetItem *tsi_chain_error_count = 0;
QTreeWidgetItem *tsi_bytes_rcv = 0;
QTreeWidgetItem *tsi_bytes_snd = 0;
QTreeWidgetItem *tsi_bytes_snd_encrypted = 0;
QTreeWidgetItem *tsi_data_bytes_rcv = 0;
QTreeWidgetItem *tsi_data_bytes_snd = 0;
//QTreeWidgetItem *tsi_encryption_overhead = 0;
//---------------------------------------------------------------------------
void MainWindow::page_tunnel_init()
{
  widget_tunnelParams = new WidgetTunnelParams();
  ui->vLayoutTunnelConfig->insertWidget(0, widget_tunnelParams, 10);
  connect(widget_tunnelParams, SIGNAL(paramsModified()), this, SLOT(tunnelParams_modified()));

  widget_tunnelConnList = new WidgetTunnelConnList();
  connect(widget_tunnelConnList, SIGNAL(needUpdate(bool)), this, SLOT(tunnelConnList_autoUpdate(bool)));
  ui->vLayout_tunnelConnList->addWidget(widget_tunnelConnList, 10);

  ui->tunnel_tabWidget->setTabEnabled(ui->tunnel_tabWidget->indexOf(widget_tunnelConnList), false);
  ui->tunnel_tabWidget->setCurrentIndex(0);

  timer_tunnelStats_autoUpdate = new QTimer(this);
  timer_tunnelStats_autoUpdate->setInterval(ui->spinTunnelStatsAutoUpdateInterval->value()*1000);
  connect(timer_tunnelStats_autoUpdate, SIGNAL(timeout()), this, SLOT(tunnelStats_autoUpdate()));

  QTreeWidgetItem *group = new QTreeWidgetItem(ui->tunnelStatsTreeWidget, QStringList() << tr("Client connections"));
  tsi_conn_cur_count = new QTreeWidgetItem(group, QStringList() << tr("Active connections"));
  tsi_conn_cur_count->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
  tsi_conn_total_count = new QTreeWidgetItem(group, QStringList() << tr("Total connections"));
  tsi_conn_total_count->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
  tsi_conn_failed_count = new QTreeWidgetItem(group, QStringList() << tr("Failed connections"));
  tsi_conn_failed_count->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
  tsi_data_bytes_rcv = new QTreeWidgetItem(group, QStringList() << tr("Received from clients"));
  tsi_data_bytes_rcv->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
  tsi_data_bytes_snd = new QTreeWidgetItem(group, QStringList() << tr("Sent to clients"));
  tsi_data_bytes_snd->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);

  group = new QTreeWidgetItem(ui->tunnelStatsTreeWidget, QStringList() << tr("Total summary"));
  tsi_bytes_rcv = new QTreeWidgetItem(group, QStringList() << tr("Total bytes received"));
  tsi_bytes_rcv->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
  tsi_bytes_snd = new QTreeWidgetItem(group, QStringList() << tr("Total bytes sent"));
  tsi_bytes_snd->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
  tsi_bytes_snd_encrypted = new QTreeWidgetItem(group, QStringList() << tr("Total encrypted bytes sent"));
  tsi_bytes_snd_encrypted->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
  /*
  tsi_encryption_overhead = new QTreeWidgetItem(group, QStringList() << tr("Encryption overhead (outgoing)"));
//  tsi_encryption_overhead->setTextAlignment(0, Qt::AlignRight | Qt::AlignVCenter);
  tsi_encryption_overhead->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
  */
  tsi_chain_error_count = new QTreeWidgetItem(group, QStringList() << tr("Tunnel chain errors detected"));
  tsi_chain_error_count->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);

  ui->tunnelStatsTreeWidget->expandAll();
#if QT_VERSION >= 0x050000
  ui->tunnelStatsTreeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
#else
  ui->tunnelStatsTreeWidget->header()->setResizeMode(0, QHeaderView::ResizeToContents);
#endif

//  mgrServerConfig_updateEnabled();
}

//---------------------------------------------------------------------------
void MainWindow::page_tunnel_ui_load(cJSON *json)
{
  widget_tunnelConnList->ui_load(json);

  cJSON *j = cJSON_GetObjectItem(json, "page_tunnelVersion");
  if (!j || j->type != cJSON_Number || j->valueint != UI_PAGE_TUNNEL_VERSION)
    return;

  j = cJSON_GetObjectItem(json, "page_tunnelAutoUpdate");
  if (j)
    ui->chkTunnelStatsAutoUpdate->setChecked(j->type == cJSON_True);

  j = cJSON_GetObjectItem(json, "page_tunnelAutoUpdateInterval");
  if (j && j->type == cJSON_Number)
  {
    ui->spinTunnelStatsAutoUpdateInterval->setValue(j->valuedouble);
    timer_tunnelStats_autoUpdate->setInterval(ui->spinTunnelStatsAutoUpdateInterval->value()*1000);
  }
}

//---------------------------------------------------------------------------
void MainWindow::page_tunnel_ui_save(cJSON *json)
{
  cJSON_AddNumberToObject(json, "page_tunnelVersion", UI_PAGE_TUNNEL_VERSION);
  if (ui->chkTunnelStatsAutoUpdate->isChecked())
    cJSON_AddTrueToObject(json, "page_tunnelAutoUpdate");
  else
    cJSON_AddFalseToObject(json, "page_tunnelAutoUpdate");
  cJSON_AddNumberToObject(json, "page_tunnelAutoUpdateInterval", ui->spinTunnelStatsAutoUpdateInterval->value());

  widget_tunnelConnList->ui_save(json);
}

//---------------------------------------------------------------------------
void MainWindow::on_spinTunnelStatsAutoUpdateInterval_valueChanged(double)
{
  timer_tunnelStats_autoUpdate->setInterval(ui->spinTunnelStatsAutoUpdateInterval->value()*1000);
}

//---------------------------------------------------------------------------
void MainWindow::on_chkTunnelStatsAutoUpdate_toggled(bool checked)
{
  ui->spinTunnelStatsAutoUpdateInterval->setEnabled(checked);
  if (checked)
  {
    tunnelStats_autoUpdate();
    timer_tunnelStats_autoUpdate->start();
  }
  else if (timer_tunnelStats_autoUpdate->isActive())
    timer_tunnelStats_autoUpdate->stop();
}

//---------------------------------------------------------------------------
void MainWindow::tunnelStats_autoUpdate()
{
  QTreeWidgetItem *cur_item = ui->browserTree->currentItem();
  if (!cur_item || cur_item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_TUNNEL)
    return;
  unsigned int ms_since_last_state_request = qAbs(t_last_tunnel_stats_requested.elapsed());
  if (t_last_tunnel_stats_requested.isValid() &&
      (ms_since_last_state_request < ui->spinTunnelStatsAutoUpdateInterval->value()*1000 || !tunnel_stats_reply_received))
    return;
  MgrClientParameters *mgrconn_params = browserTree_mgrConnItem(cur_item)->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
  TunnelParameters *tun_params = cur_item->data(0, TunnelConfigRole).value<TunnelParameters *>();
  if (tun_params->id > 0)
  {
    t_last_tunnel_stats_requested.restart();
    tunnel_stats_reply_received = false;
    emit gui_tunnel_state_get(mgrconn_params->id, tun_params->id);
  }
}

//---------------------------------------------------------------------------
void MainWindow::tunnelConnList_autoUpdate(bool now)
{
  QTreeWidgetItem *cur_item = ui->browserTree->currentItem();
  if (!cur_item || cur_item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_TUNNEL)
    return;
  unsigned int ms_since_last_connstate_request = qAbs(t_last_tunnel_connstate_requested.elapsed());
  if (!now && t_last_tunnel_connstate_requested.isValid() &&
      (ms_since_last_connstate_request < widget_tunnelConnList->ui->spinAutoUpdateInterval->value()*1000 || !tunnel_connstate_reply_received))
  {
    widget_tunnelConnList->ui->tableView->setEnabled(true);
    return;
  }
  MgrClientParameters *mgrconn_params = browserTree_mgrConnItem(cur_item)->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
  TunnelParameters *tun_params = cur_item->data(0, TunnelConfigRole).value<TunnelParameters *>();
  if (tun_params->id > 0)
  {
    t_last_tunnel_connstate_requested.restart();
    tunnel_connstate_reply_received = false;
    emit gui_tunnel_connstate_get(mgrconn_params->id, tun_params->id, widget_tunnelConnList->ui->chkGetDisconnected->isChecked());
  }
}

//---------------------------------------------------------------------------
void MainWindow::page_tunnel_loadItem(QTreeWidgetItem *tunnel_item)
{
//  ui->mgrconn_tabWidget->setCurrentIndex(0);
//  mgrServerConfig_updateEnabled();
  ui->page_tunnel->setEnabled(true);
  t_last_tunnel_stats_requested = QTime();
  tunnel_stats_reply_received = true;

  // Connection tab
  if (!tunnel_item || tunnel_item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_TUNNEL)
    return;

  TunnelParameters *tun_params = tunnel_item->data(0, TunnelConfigRole).value<TunnelParameters *>();
  widget_tunnelParams->loadParams(tun_params, tunnel_item->data(0, TunnelEditableRole).value<bool>());

  if (tun_params->id == 0)
  {
    ui->tunnel_tabWidget->setCurrentIndex(ui->tunnel_tabWidget->indexOf(ui->tab_tunnelConfig));
    ui->btnTunnelConfigApply->setText(tr("Create tunnel"));
  }
  else
  {
    tunnelStats_autoUpdate();
    widget_tunnelConnList->activate();
//    ui->tunnel_tabWidget->setCurrentIndex(0);
    if (ui->chkTunnelStatsAutoUpdate->isChecked())
      timer_tunnelStats_autoUpdate->start();
    ui->btnTunnelConfigApply->setText(tr("Rebuild tunnel"));
  }

  cur_tunnel_config_changed = false;
  update_tunnelConfig_buttons();
}

//---------------------------------------------------------------------------
void MainWindow::tunnelParams_modified()
{
  cur_tunnel_config_changed = true;
  update_tunnelConfig_buttons();
}

//---------------------------------------------------------------------------
void MainWindow::update_tunnelConfig_buttons()
{
  QTreeWidgetItem *cur_item = ui->browserTree->currentItem();
  if (!cur_item || cur_item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_TUNNEL)
    return;
  TunnelParameters *tun_params = cur_item->data(0, TunnelConfigRole).value<TunnelParameters *>();
  bool can_apply = widget_tunnelParams->canApply();
  ui->btnTunnelConfigApply->setEnabled(cur_tunnel_config_changed && can_apply);
  ui->btnTunnelConfigCancel->setEnabled(cur_tunnel_config_changed || tun_params->id == 0);
}

//---------------------------------------------------------------------------
void MainWindow::on_btnTunnelConfigCancel_clicked()
{
  QTreeWidgetItem *cur_item = ui->browserTree->currentItem();
  if (!cur_item || cur_item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_TUNNEL)
    return;
  TunnelParameters *tun_params = cur_item->data(0, TunnelConfigRole).value<TunnelParameters *>();
  if (tun_params->id == 0)
  {
    ui->browserTree->setCurrentItem(browserTree_mgrConnItem(cur_item));
    browserTree_removeItem(cur_item);
  }
  else
    page_tunnel_loadItem(cur_item);
}

//---------------------------------------------------------------------------
void MainWindow::page_tunnel_saveItem(QTreeWidgetItem *tunnel_item)
{
  if (!tunnel_item || tunnel_item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_TUNNEL)
    return;
  TunnelParameters *tun_params = tunnel_item->data(0, TunnelConfigRole).value<TunnelParameters *>();
  widget_tunnelParams->saveParams(tun_params);
  tunnel_item->setText(0, tun_params->name);

  cur_tunnel_config_changed = false;
}

//---------------------------------------------------------------------------
void MainWindow::on_btnTunnelConfigApply_clicked()
{
  QTreeWidgetItem *cur_item = ui->browserTree->currentItem();
  if (!cur_item || cur_item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_TUNNEL)
    return;
  page_tunnel_saveItem(cur_item);

  MgrClientParameters *mgrconn_params = browserTree_mgrConnItem(cur_item)->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
  TunnelParameters *tun_params = cur_item->data(0, TunnelConfigRole).value<TunnelParameters *>();
  cJSON *j_tun_params = cJSON_CreateObject();
  tun_params->printJSON(j_tun_params);
  if (tun_params->id == 0)
    emit gui_tunnel_create(mgrconn_params->id, j_tun_params);
  else
    emit gui_tunnel_config_set(mgrconn_params->id, j_tun_params);

  ui->page_tunnel->setEnabled(false);
}

//---------------------------------------------------------------------------
void MainWindow::mgrconn_tunnel_create_reply(quint32 conn_id, TunnelId orig_tun_id, TunnelId tun_id, quint16 res_code, const QString &error)
{
  if (orig_tun_id != 0)
    return;
  QTreeWidgetItem *tun_item = browserTree_getTunnelById(conn_id, 0);
  if (!tun_item)
    return;
  ui->page_tunnel->setEnabled(true);

  if (res_code != TunnelState::RES_CODE_OK)
  {
    widget_tunnelParams->applyError(res_code);
    QMessageBox::critical(this, tr("Tunnel configuration error"), error, QMessageBox::Ok);
  }
  else
  {
    browserTree_removeItem(tun_item);
    QTreeWidgetItem *new_tun_item = browserTree_getTunnelById(conn_id, tun_id);
    if (new_tun_item)
      ui->browserTree->setCurrentItem(new_tun_item);
  }
}

//---------------------------------------------------------------------------
void MainWindow::mgrconn_tunnel_config_set_reply(quint32 conn_id, TunnelId tun_id, quint16 res_code, const QString &error)
{
  QTreeWidgetItem *tun_item = browserTree_getTunnelById(conn_id, tun_id);
  if (!tun_item)
    return;

  ui->page_tunnel->setEnabled(true);

  if (res_code != TunnelState::RES_CODE_OK)
  {
    widget_tunnelParams->applyError(res_code);
    QMessageBox::critical(this, tr("Tunnel configuration error"), error, QMessageBox::Ok);
  }
}

//---------------------------------------------------------------------------
void MainWindow::mgrconn_tunnel_remove(quint32 conn_id, TunnelId tun_id, quint16 res_code, const QString &error)
{
  QTreeWidgetItem *tun_item = browserTree_getTunnelById(conn_id, tun_id);
  if (!tun_item)
    return;

  if (res_code != TunnelState::RES_CODE_OK)
  {
    QMessageBox::critical(this, tr("Tunnel removal error"), error, QMessageBox::Ok);
  }
  else
  {
    browserTree_removeItem(tun_item);
  }
}

//---------------------------------------------------------------------------
void MainWindow::tunnel_config_received(quint32 conn_id, cJSON *j_config)
{
  QTreeWidgetItem *mgrconn_item = browserTree_getMgrConnById(conn_id);
  if (!mgrconn_item)
  {
    cJSON_Delete(j_config);
    return;
  }
  TunnelParameters new_tunnel_params;
  new_tunnel_params.parseJSON(j_config);
  cJSON *j = cJSON_GetObjectItem(j_config, "can_modify");
  bool can_modify = j && j->type == cJSON_True;
  cJSON_Delete(j_config);

  QTreeWidgetItem *tunnel_item = browserTree_getTunnelById(conn_id, new_tunnel_params.id);
  TunnelParameters *tun_params = NULL;
  TunnelState *tun_state = NULL;
  if (!tunnel_item)
  {
    tun_params = new TunnelParameters;
    *tun_params = new_tunnel_params;
    tun_state = new TunnelState;

    tunnel_item = new QTreeWidgetItem(mgrconn_item, QStringList() << tun_params->name);
    tunnel_list.insert(((quint64)conn_id << 32) + (quint64)tun_params->id, tunnel_item);
    tunnel_item->setIcon(0, QIcon("://images/icons/icon32.png"));
    tunnel_item->setData(0, ItemTypeRole, (int)ITEM_TYPE_TUNNEL);
    tunnel_item->setData(0, TunnelConfigRole, QVariant::fromValue(tun_params));
    tunnel_item->setData(1, TunnelStateRole, QVariant::fromValue(tun_state));
    tunnel_item->setIcon(1, QIcon(":/images/icons/circle16-gray.png"));
    tunnel_item->setText(1, tr("N/A"));
  }
  else
  {
    tun_params = tunnel_item->data(0, TunnelConfigRole).value<TunnelParameters *>();
    tun_state = tunnel_item->data(1, TunnelStateRole).value<TunnelState *>();
    *tun_params = new_tunnel_params;
    tunnel_item->setText(0, tun_params->name);
  }

  tunnel_item->setData(0, TunnelEditableRole, can_modify);
  if (can_modify)
    tunnel_item->setFlags(tunnel_item->flags() | Qt::ItemIsEditable);
  else
    tunnel_item->setFlags(tunnel_item->flags() & ~Qt::ItemIsEditable);

  if (tunnel_item != ui->browserTree->currentItem())
    return;
  page_tunnel_loadItem(tunnel_item);
}

//---------------------------------------------------------------------------
void MainWindow::tunnel_state_received(quint32 conn_id, TunnelId tun_id, TunnelState new_state)
{
  QTreeWidgetItem *tunnel_item = browserTree_getTunnelById(conn_id, tun_id);
  if (!tunnel_item)
    return;
  QTreeWidgetItem *cur_item = ui->browserTree->currentItem();

  TunnelState *tun_state = tunnel_item->data(1, TunnelStateRole).value<TunnelState *>();
  *tun_state = new_state;

  TunnelParameters *tun_params = tunnel_item->data(0, TunnelConfigRole).value<TunnelParameters *>();

  QString state_text;

  if (!(tun_state->flags & TunnelState::TF_STARTED))
  {
    state_text = QString("Stopped");
    tunnel_item->setIcon(1, QIcon(":/images/icons/circle16-gray.png"));
  }
  else if (tun_state->flags & TunnelState::TF_STOPPING)
  {
    state_text = QString("Stopping");
    tunnel_item->setIcon(1, QIcon(":/images/icons/circle16-yellow.png"));
  }
  else if (!(tun_state->flags & TunnelState::TF_CHECK_PASSED))
  {
    state_text = QString("Checking");
    tunnel_item->setIcon(1, QIcon(":/images/icons/circle16-yellow.png"));
  }
  else if ((tun_params->flags & TunnelParameters::FL_MASTER_TUNSERVER) &&
           (tun_state->flags & TunnelState::TF_CHAIN_OK) &&
            tun_state->last_error_code == TunnelState::RES_CODE_OK)
  {
    state_text = QString("OK");
    tunnel_item->setIcon(1, QIcon(":/images/icons/circle16-green.png"));
  }
  else if ((tun_params->flags & TunnelParameters::FL_MASTER_TUNSERVER) &&
           (tun_state->flags & TunnelState::TF_IDLE) &&
           (tun_state->flags & TunnelState::TF_BINDING))
  {
    state_text = QString("Idle (binding)");
    tunnel_item->setIcon(1, QIcon(":/images/icons/circle16-green.png"));
  }
  else if (tun_params->tunservers.isEmpty() &&
          (tun_state->flags & TunnelState::TF_MGRCONN_IN_CLEAR_TO_SEND) &&
           tun_state->last_error_code == TunnelState::RES_CODE_OK)
  {
    state_text = QString("OK");
    tunnel_item->setIcon(1, QIcon(":/images/icons/circle16-green.png"));
  }
  else if (!(tun_params->flags & TunnelParameters::FL_MASTER_TUNSERVER) && !tun_params->tunservers.isEmpty() &&
           (tun_state->flags & TunnelState::TF_MGRCONN_IN_CONNECTED) &&
           (tun_state->flags & TunnelState::TF_MGRCONN_OUT_CONNECTED) &&
           tun_state->last_error_code == TunnelState::RES_CODE_OK)
  {
    state_text = QString("OK");
    tunnel_item->setIcon(1, QIcon(":/images/icons/circle16-green.png"));
  }
  else
  {
    state_text = QString("Failure");
    tunnel_item->setIcon(1, QIcon(":/images/icons/circle16-red.png"));
  }

  if (tun_state->latency_ms >= 0)
    state_text += QString(" (%1 ms)").arg(tun_state->latency_ms);
  tunnel_item->setText(1, state_text);

  if (cur_item == tunnel_item)
  {
    t_last_tunnel_stats_requested.restart();
    tunnel_stats_reply_received = true;

    if (tsi_bytes_rcv)
      tsi_bytes_rcv->setText(1, pretty_size(tun_state->stats.bytes_rcv, 1));
    if (tsi_bytes_snd)
      tsi_bytes_snd->setText(1, pretty_size(tun_state->stats.bytes_snd, 1));
    if (tsi_bytes_snd_encrypted)
      tsi_bytes_snd_encrypted->setText(1, pretty_size(tun_state->stats.bytes_snd_encrypted, 1));
    if (tsi_chain_error_count)
      tsi_chain_error_count->setText(1, (tun_params->flags & TunnelParameters::FL_MASTER_TUNSERVER) ? QString::number(tun_state->stats.chain_error_count) : QString("N/A"));
    if (tsi_conn_cur_count)
      tsi_conn_cur_count->setText(1, ((tun_params->flags & TunnelParameters::FL_MASTER_TUNSERVER) || (tun_params->tunservers.isEmpty())) && tun_params->app_protocol != TunnelParameters::UDP ? QString::number(tun_state->stats.conn_cur_count) : QString("N/A"));
    if (tsi_conn_failed_count)
      tsi_conn_failed_count->setText(1, ((tun_params->flags & TunnelParameters::FL_MASTER_TUNSERVER) || (tun_params->tunservers.isEmpty())) && tun_params->app_protocol != TunnelParameters::UDP ? QString::number(tun_state->stats.conn_failed_count) : QString("N/A"));
    if (tsi_conn_total_count)
      tsi_conn_total_count->setText(1, ((tun_params->flags & TunnelParameters::FL_MASTER_TUNSERVER) || (tun_params->tunservers.isEmpty())) && tun_params->app_protocol != TunnelParameters::UDP ? QString::number(tun_state->stats.conn_total_count) : QString("N/A"));
    if (tsi_data_bytes_rcv)
      tsi_data_bytes_rcv->setText(1, pretty_size(tun_state->stats.data_bytes_rcv, 1));
    if (tsi_data_bytes_snd)
      tsi_data_bytes_snd->setText(1, pretty_size(tun_state->stats.data_bytes_snd, 1));
    ui->lblTunnelLastErrorStr->setText(tun_state->last_error_str);

//    if (tsi_encryption_overhead)
//      tsi_encryption_overhead->setText(1, QString("%1%").arg((int)qAbs(tun_state->stats.bytes_snd > 0 ? (tun_state->stats.bytes_snd_encrypted*100/tun_state->stats.bytes_snd)-100 : 0)));
    return;
  }
}

//---------------------------------------------------------------------------
void MainWindow::tunnel_connstate_received(quint32 conn_id, const QByteArray &data)
{
  if (data.length() < (int)sizeof(TunnelId))
    return;
  TunnelId tun_id = *((TunnelId *)data.data());
  QTreeWidgetItem *tunnel_item = browserTree_getTunnelById(conn_id, tun_id);
  if (!tunnel_item || tunnel_item != ui->browserTree->currentItem())
    return;

  t_last_tunnel_connstate_requested.restart();
  tunnel_connstate_reply_received = true;
  widget_tunnelConnList->tunnel_connstate_received(data.mid(sizeof(TunnelId)));
}
