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
#include <QInputDialog>
#include "../lib/prc_log.h"
#include "../lib/mgrclient-conn.h"
#include <QtGlobal>
#define UI_BROWSERTREE_VERSION           1
#define UI_BROWSERTREE_MAX_DEPTH         20

extern bool mgr_server_params_gui_modified;
int browserTree_header_font_size=8;
extern quint32 unique_mgrconn_id;
QHash<quint32, QTreeWidgetItem *> mgrconn_list;
QHash<quint64, QTreeWidgetItem *> tunnel_list;    // key is: first 32 bits = conn_id, last 32 bits = tunnel_id

//---------------------------------------------------------------------------
void MainWindow::mod_browserTree_init()
{
  browserTree_popupMenu_item = NULL;
  ui->browserTree->sortByColumn(0, Qt::AscendingOrder);

  browserTree_hostMenu = new QMenu(ui->browserTree);
  browserTree_action_connect    = browserTree_hostMenu->addAction(tr("Connect"), this, SLOT(browserTree_on_hostMenu_connect()));
  browserTree_action_disconnect = browserTree_hostMenu->addAction(tr("Disconnect"), this, SLOT(browserTree_on_hostMenu_disconnect()));
  browserTree_hostMenu->addSeparator();
  browserTree_action_newTunnel  = browserTree_hostMenu->addAction(tr("Create new tunnel..."), this, SLOT(browserTree_on_hostMenu_newTunnel()));
  browserTree_hostMenu->addSeparator();
  browserTree_action_removeHost = browserTree_hostMenu->addAction(tr("Remove tunnel server"), this, SLOT(browserTree_on_hostMenu_remove()));

  browserTree_tunnelMenu = new QMenu(ui->browserTree);
  browserTree_action_tun_start  = browserTree_tunnelMenu->addAction(tr("Start"), this, SLOT(browserTree_on_tunnelMenu_start()));
  browserTree_action_tun_stop   = browserTree_tunnelMenu->addAction(tr("Stop"), this, SLOT(browserTree_on_tunnelMenu_stop()));
  browserTree_tunnelMenu->addSeparator();
  browserTree_action_tun_remove = browserTree_tunnelMenu->addAction(tr("Remove tunnel"), this, SLOT(browserTree_on_tunnelMenu_remove()));

  browserTree_folderMenu = new QMenu(ui->browserTree);
  browserTree_action_newFolder    = browserTree_folderMenu->addAction(tr("Add folder..."), this, SLOT(browserTree_on_folderMenu_new()));
  browserTree_action_removeFolder = browserTree_folderMenu->addAction(tr("Remove folder"), this, SLOT(browserTree_on_folderMenu_remove()));
  browserTree_folderMenu->addSeparator();
  browserTree_action_newHost      = browserTree_folderMenu->addAction(tr("Add tunnel server..."), this, SLOT(browserTree_on_hostMenu_new()));

  browserTree_menu = new QMenu(ui->browserTree);
  browserTree_menu->addAction(browserTree_action_newFolder);
  browserTree_menu->addAction(browserTree_action_newHost);

  ui->browserTree->header()->setStyleSheet(QString("QHeaderView { font-size: %1pt; }").arg(browserTree_header_font_size));

#if QT_VERSION >= 0x050000
  ui->browserTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
#else
  ui->browserTree->header()->setResizeMode(0, QHeaderView::ResizeToContents);
#endif
  ui->browserTree->header()->resizeSection(1, ui->browserTree->header()->sectionSizeHint(1));
}

//---------------------------------------------------------------------------
void MainWindow::mod_browserTree_ui_load(cJSON *json)
{
  cJSON *j = cJSON_GetObjectItem(json, "browserTreeStateVersion");
  if (!j || j->type != cJSON_Number || j->valueint != UI_BROWSERTREE_VERSION)
    return;
  j = cJSON_GetObjectItem(json, "browserTreeState");
  if (j && j->type == cJSON_String)
    ui->browserTree->header()->restoreState(QByteArray::fromBase64(j->valuestring));

  j = cJSON_GetObjectItem(json, "browserTreeHeaderFontSize");
  if (j && j->type == cJSON_Number)
    browserTree_header_font_size = j->valueint;

  ui->browserTree->header()->setStyleSheet(QString("QHeaderView { font-size: %1pt; }").arg(browserTree_header_font_size));
}

//---------------------------------------------------------------------------
void MainWindow::mod_browserTree_ui_save(cJSON *json)
{
  cJSON_AddStringToObject(json, "browserTreeState", ui->browserTree->header()->saveState().toBase64());
  cJSON_AddNumberToObject(json, "browserTreeStateVersion", UI_BROWSERTREE_VERSION);
  cJSON_AddNumberToObject(json, "browserTreeHeaderFontSize", browserTree_header_font_size);
}

//---------------------------------------------------------------------------
void MainWindow::on_browserTree_currentItemChanged(QTreeWidgetItem *cur_item, QTreeWidgetItem *prev_item)
{
  if (prev_item)
  {
    if (prev_item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_MGRCONN)
    {
      MgrClientParameters *mgrconn_params = prev_item->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
      if (mgrconn_params && mgrconn_params->conn_type == MgrClientParameters::CONN_DEMAND && mgrconn_params->idle_timeout > 0)
        emit gui_mgrConnDemand_idleStart(mgrconn_params->id);
    }
    if (prev_item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_TUNNEL)
    {
      widget_tunnelConnList->activate();
      if (timer_tunnelStats_autoUpdate->isActive())
        timer_tunnelStats_autoUpdate->stop();
    }
  }
  else
  {
    widget_tunnelConnList->deactivate();
    if (timer_tunnelStats_autoUpdate->isActive())
      timer_tunnelStats_autoUpdate->stop();
  }
  if (cur_item)
  {
    if (cur_item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_MGRCONN)
    {
      ui->stackedWidget->setCurrentWidget(ui->page_MgrConn);
      page_mgrConn_loadItem(cur_item);
      MgrClientParameters *mgrconn_params = cur_item->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
      if (mgrconn_params && mgrconn_params->conn_type == MgrClientParameters::CONN_DEMAND && mgrconn_params->idle_timeout > 0)
        emit gui_mgrConnDemand_idleStop(mgrconn_params->id);
    }
    else if (cur_item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_TUNNEL)
    {
      ui->stackedWidget->setCurrentWidget(ui->page_tunnel);
      page_tunnel_loadItem(cur_item);
    }
    else
      ui->stackedWidget->setCurrentWidget(ui->page_empty);
  }
  else
    ui->stackedWidget->setCurrentWidget(ui->page_empty);
}

//---------------------------------------------------------------------------
void MainWindow::on_browserTree_customContextMenuRequested(const QPoint &pos)
{
  QPoint p = ui->browserTree->mapToGlobal(pos);
  browserTree_popupMenu_item = ui->browserTree->itemAt(pos);
  p.setX(p.x()+10);
  if (!browserTree_popupMenu_item)
  {
    browserTree_menu->exec(p);
    return;
  }
  if (browserTree_popupMenu_item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_FOLDER)
  {
    browserTree_folderMenu->exec(p);
  }
  else if (browserTree_popupMenu_item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_MGRCONN)
  {
    MgrClientParameters *mgrconn_params = browserTree_popupMenu_item->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
    MgrClientConnection::MgrConnState connState = (MgrClientConnection::MgrConnState)browserTree_popupMenu_item->data(1, MgrConnStateRole).toInt();
    browserTree_action_connect->setEnabled(mgrconn_params->conn_type == MgrClientParameters::CONN_DEMAND &&
                                          (connState == MgrClientConnection::MGR_NONE || connState == MgrClientConnection::MGR_ERROR));
    browserTree_action_disconnect->setEnabled(connState == MgrClientConnection::MGR_CONNECTED);
    browserTree_action_removeHost->setEnabled(true);
    browserTree_action_newTunnel->setEnabled(connState == MgrClientConnection::MGR_CONNECTED &&
                                             (browserTree_popupMenu_item->data(1, MgrConnAccessFlagsRole).value<quint64>() & UserGroup::AF_TUNNEL_CREATE));
    browserTree_hostMenu->exec(p);
  }
  else if (browserTree_popupMenu_item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_TUNNEL)
  {
    TunnelState *tun_state = browserTree_popupMenu_item->data(1, TunnelStateRole).value<TunnelState *>();

    browserTree_action_tun_remove->setEnabled(browserTree_popupMenu_item->data(0, TunnelEditableRole).value<bool>());
    browserTree_action_tun_start->setEnabled(browserTree_popupMenu_item->data(0, TunnelEditableRole).value<bool>() && !(tun_state->flags & TunnelState::TF_STARTED));
    browserTree_action_tun_stop->setEnabled(browserTree_popupMenu_item->data(0, TunnelEditableRole).value<bool>() && (tun_state->flags & TunnelState::TF_STARTED));
//    TunnelParameters *tun_params = browserTree_popupMenu_item->data(0, TunnelConfigRole).value<TunnelParameters *>();

    browserTree_tunnelMenu->exec(p);
  }
}

//---------------------------------------------------------------------------
void MainWindow::browserTree_on_folderMenu_new()
{
  QTreeWidgetItem *new_item;
  QString folder_name("New folder");
  if (browserTree_popupMenu_item)
    new_item = new QTreeWidgetItem(browserTree_popupMenu_item, QStringList() << folder_name);
  else
    new_item = new QTreeWidgetItem(ui->browserTree, QStringList() << folder_name);
  new_item->setFlags(new_item->flags() | Qt::ItemIsEditable);
  new_item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
  new_item->setData(0, ItemTypeRole, (int)ITEM_TYPE_FOLDER);
  if (browserTree_popupMenu_item)
    ui->browserTree->expandItem(browserTree_popupMenu_item);

  ui->browserTree->setCurrentItem(new_item);
  ui->browserTree->editItem(new_item, 0);

  mgrConn_modified();
  profile_modified();
}

//---------------------------------------------------------------------------
void MainWindow::browserTree_on_folderMenu_remove()
{
  browserTree_on_hostMenu_remove();
}

//---------------------------------------------------------------------------
void MainWindow::browserTree_on_hostMenu_connect()
{
  if (!browserTree_popupMenu_item)
    return;
  if (browserTree_popupMenu_item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_MGRCONN)
    return;
  MgrClientParameters *mgrconn_params = browserTree_popupMenu_item->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
  if (mgrconn_params)
    emit gui_mgrConnDemand(mgrconn_params->id);
}

//---------------------------------------------------------------------------
void MainWindow::browserTree_on_hostMenu_disconnect()
{
  if (!browserTree_popupMenu_item)
    return;
  if (browserTree_popupMenu_item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_MGRCONN)
    return;
  MgrClientParameters *mgrconn_params = browserTree_popupMenu_item->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
  if (mgrconn_params)
    emit gui_mgrConnDemand_disconnect(mgrconn_params->id);
}

//---------------------------------------------------------------------------
void MainWindow::on_browserTree_itemChanged(QTreeWidgetItem *item, int column)
{
  if (column != 0)
    return;
  if (item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_MGRCONN)
  {
    MgrClientParameters *mgrconn_params = item->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
    if (mgrconn_params && mgrconn_params->name != item->text(0))
    {
      mgrconn_params->name = item->text(0);
      if (item == ui->browserTree->currentItem())
        widget_mgrClient->setMgrConnName(mgrconn_params->name);
      mgrConn_modified();
      profile_modified();
    }
  }
  else if (item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_TUNNEL)
  {
    TunnelParameters *tun_params = item->data(0, TunnelConfigRole).value<TunnelParameters *>();
    if (tun_params && tun_params->name != item->text(0))
    {
      tun_params->name = item->text(0);
      if (item == ui->browserTree->currentItem())
        widget_tunnelParams->setTunnelName(tun_params->name);
    }
  }
}

//---------------------------------------------------------------------------
void MainWindow::on_browserTree_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
  if (column != 0)
    return;
  ui->browserTree->editItem(item, column);
}

//---------------------------------------------------------------------------
void MainWindow::browserTree_on_hostMenu_new()
{
  QTreeWidgetItem *new_item;
  MgrClientParameters *mgrconn_params = new MgrClientParameters;

  mgrconn_params->id = unique_mgrconn_id++;
  // ensure all items have distinct unique IDs
  while (mgrconn_list.contains(mgrconn_params->id))
  {
    mgrconn_params->id = unique_mgrconn_id++;
    if (mgrconn_params->id == 0)
      mgrconn_params->id = unique_mgrconn_id++;
  }

  cJSON *j = cJSON_GetObjectItem(j_gui_settings, "defaultSSL_pkeyFilename");
  if (j && j->type == cJSON_String)
    mgrconn_params->private_key_filename = QString::fromUtf8(j->valuestring);

  j = cJSON_GetObjectItem(j_gui_settings, "defaultSSL_certFilename");
  if (j && j->type == cJSON_String)
    mgrconn_params->ssl_cert_filename = QString::fromUtf8(j->valuestring);

  mgrconn_params->name = QString("Tunnel Server %1").arg(mgrconn_params->id);
  if (browserTree_popupMenu_item)
    new_item = new QTreeWidgetItem(browserTree_popupMenu_item, QStringList() << mgrconn_params->name);
  else
    new_item = new QTreeWidgetItem(ui->browserTree, QStringList() << mgrconn_params->name);
  mgrconn_list.insert(mgrconn_params->id, new_item);
  new_item->setFlags(new_item->flags() | Qt::ItemIsEditable);
  new_item->setIcon(0, style()->standardIcon(QStyle::SP_ComputerIcon));
  new_item->setData(0, ItemTypeRole, (int)ITEM_TYPE_MGRCONN);
  new_item->setData(0, MgrClientParametersRole, QVariant::fromValue(mgrconn_params));
  new_item->setData(0, MgrServerConfigRole, QVariant::fromValue(((cJSON*)NULL)));
  new_item->setIcon(1, QIcon(":/images/icons/circle16-gray.png"));
  new_item->setData(1, MgrConnStateRole, (int)MgrClientConnection::MGR_NONE);
  if (browserTree_popupMenu_item)
    ui->browserTree->expandItem(browserTree_popupMenu_item);

  ui->browserTree->setCurrentItem(new_item);
  widget_mgrClient->setFocus();

  mgrConn_modified();
  profile_modified();
}

//---------------------------------------------------------------------------
void MainWindow::browserTree_on_hostMenu_newTunnel()
{
  if (!browserTree_popupMenu_item)
    return;
  if (browserTree_popupMenu_item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_MGRCONN)
    return;
  MgrClientParameters *mgrconn_params = browserTree_popupMenu_item->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
  if (!mgrconn_params)
    return;
  if (tunnel_list.contains(((quint64)mgrconn_params->id << 32)))
  {
    ui->browserTree->setCurrentItem(tunnel_list.value((quint64)mgrconn_params->id << 32));
    widget_tunnelParams->setFocus();
    return;
  }

  TunnelParameters *tun_params = new TunnelParameters;
  tun_params->flags |= TunnelParameters::FL_MASTER_TUNSERVER;
  TunnelState *tun_state = new TunnelState;

  tun_params->id = 0;
  tun_params->name = QString("New tunnel");
  QTreeWidgetItem *new_item = new QTreeWidgetItem(browserTree_popupMenu_item, QStringList() << tun_params->name);
  tunnel_list.insert(((quint64)mgrconn_params->id << 32) + (quint64)tun_params->id, new_item);
  new_item->setFlags(new_item->flags() | Qt::ItemIsEditable);
  new_item->setIcon(0, QIcon("://images/icons/icon32.png"));
  new_item->setData(0, ItemTypeRole, (int)ITEM_TYPE_TUNNEL);
  new_item->setData(0, TunnelConfigRole, QVariant::fromValue(tun_params));
  new_item->setData(0, TunnelEditableRole, true);
  new_item->setData(1, TunnelStateRole, QVariant::fromValue(tun_state));
  new_item->setIcon(1, QIcon(":/images/icons/circle16-gray.png"));
  new_item->setText(1, tr("N/A"));
  ui->browserTree->expandItem(browserTree_popupMenu_item);

  ui->browserTree->setCurrentItem(new_item);
  widget_tunnelParams->setFocus();
}

//---------------------------------------------------------------------------
void MainWindow::currentMgrConn_disconnected()
{
  QTreeWidgetItem *cur_mgrconn_item = browserTree_currentMgrConn();
  for (int i=cur_mgrconn_item->childCount()-1; i >= 0; i--)
    browserTree_removeItem(cur_mgrconn_item->child(i));
}

//---------------------------------------------------------------------------
void MainWindow::browserTree_clear()
{
  for (int i=ui->browserTree->topLevelItemCount()-1; i >= 0; i--)
    browserTree_removeItem(ui->browserTree->topLevelItem(i));
}

//---------------------------------------------------------------------------
void MainWindow::browserTree_removeItem(QTreeWidgetItem *item)
{
  if (browserTree_popupMenu_item == item)
    browserTree_popupMenu_item = NULL;
  for (int i=item->childCount()-1; i >= 0; i--)
    browserTree_removeItem(item->child(i));

  if (item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_MGRCONN)
  {
    MgrClientParameters *mgrconn_params = item->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
    if (mgrconn_params)
    {
      mgrconn_list.remove(mgrconn_params->id);
      delete mgrconn_params;
    }
    cJSON *serverConfig = item->data(0, MgrServerConfigRole).value<cJSON *>();
    if (serverConfig)
      cJSON_Delete(serverConfig);
  }
  else if (item->data(0, ItemTypeRole).toInt() == ITEM_TYPE_TUNNEL)
  {
    MgrClientParameters *mgrconn_params = browserTree_mgrConnItem(item)->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
    TunnelParameters *tunnel_params = item->data(0, TunnelConfigRole).value<TunnelParameters *>();
    if (mgrconn_params && tunnel_params)
    {
      tunnel_list.remove(((quint64)mgrconn_params->id << 32) + (quint64)tunnel_params->id);
      delete tunnel_params;
    }
    TunnelState *tunnel_state = item->data(1, TunnelStateRole).value<TunnelState *>();
    if (tunnel_state)
      delete tunnel_state;
  }
  delete item;
}

//---------------------------------------------------------------------------
void MainWindow::browserTree_on_hostMenu_remove()
{
  if (!browserTree_popupMenu_item)
    return;
  if (browserTree_popupMenu_item->childCount() > 0)
  {
    if (QMessageBox::question(this, tr("Remove item"), tr("Are you sure you want to remove item '%1'?\n"
                                                          "This item is being used by other (children) item(s).")
                                .arg(browserTree_popupMenu_item->text(0)),
                                QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
      return;
  }
  else if (QMessageBox::question(this, tr("Remove item"), tr("Are you sure you want to remove item '%1'?")
                            .arg(browserTree_popupMenu_item->text(0)),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
    return;
  browserTree_removeItem(browserTree_popupMenu_item);
  profile_modified();
  update_mgrConn_buttons();
}

//---------------------------------------------------------------------------
void MainWindow::browserTree_on_tunnelMenu_start()
{
  if (!browserTree_popupMenu_item)
    return;
  MgrClientParameters *mgrconn_params = browserTree_mgrConnItem(browserTree_popupMenu_item)->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
  TunnelParameters *tunnel_params = browserTree_popupMenu_item->data(0, TunnelConfigRole).value<TunnelParameters *>();
  if (tunnel_params->id > 0)
    emit gui_tunnel_start(mgrconn_params->id, tunnel_params->id);
}

//---------------------------------------------------------------------------
void MainWindow::browserTree_on_tunnelMenu_stop()
{
  if (!browserTree_popupMenu_item)
    return;
  MgrClientParameters *mgrconn_params = browserTree_mgrConnItem(browserTree_popupMenu_item)->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
  TunnelParameters *tunnel_params = browserTree_popupMenu_item->data(0, TunnelConfigRole).value<TunnelParameters *>();
  if (tunnel_params->id > 0)
    emit gui_tunnel_stop(mgrconn_params->id, tunnel_params->id);
}

//---------------------------------------------------------------------------
void MainWindow::browserTree_on_tunnelMenu_remove()
{
  if (!browserTree_popupMenu_item)
    return;
  if (QMessageBox::question(this, tr("Remove tunnel"), tr("Are you sure you want to remove tunnel '%1'?")
                            .arg(browserTree_popupMenu_item->text(0)),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
    return;
  MgrClientParameters *mgrconn_params = browserTree_mgrConnItem(browserTree_popupMenu_item)->data(0, MgrClientParametersRole).value<MgrClientParameters *>();
  TunnelParameters *tunnel_params = browserTree_popupMenu_item->data(0, TunnelConfigRole).value<TunnelParameters *>();
  if (tunnel_params->id > 0)
    emit gui_tunnel_remove(mgrconn_params->id, tunnel_params->id);
  else
    browserTree_removeItem(browserTree_popupMenu_item);
}

//---------------------------------------------------------------------------
QTreeWidgetItem *MainWindow::browserTree_getMgrConnById(quint32 conn_id)
{
  return mgrconn_list.value(conn_id, NULL);
}

//---------------------------------------------------------------------------
QTreeWidgetItem *MainWindow::browserTree_getTunnelById(quint32 conn_id, quint32 tunnel_id)
{
  return tunnel_list.value(((quint64)conn_id << 32) + (quint64)tunnel_id, NULL);
}

//---------------------------------------------------------------------------
QTreeWidgetItem *MainWindow::browserTree_currentMgrConn()
{
  return browserTree_mgrConnItem(ui->browserTree->currentItem());
}

//---------------------------------------------------------------------------
QTreeWidgetItem *MainWindow::browserTree_mgrConnItem(QTreeWidgetItem *child_item)
{
  QTreeWidgetItem *item = child_item;
  while (item && item->data(0, ItemTypeRole).toInt() != ITEM_TYPE_MGRCONN)
    item = item->parent();
  return item;
}

//---------------------------------------------------------------------------
void MainWindow::connection_state_changed(quint32 conn_id, quint16 mgr_conn_state)
{
  QTreeWidgetItem *item = browserTree_getMgrConnById(conn_id);
  if (!item)
    return;
  item->setData(1, MgrConnStateRole, (int)mgr_conn_state);
  if (item == ui->browserTree->currentItem())
  {
    mgrServerConfig_updateEnabled();
    mgr_server_params_gui_modified = false;
  }
  if (mgr_conn_state != MgrClientConnection::MGR_CONNECTED)
  {
    for (int i=item->childCount()-1; i >= 0; i--)
      browserTree_removeItem(item->child(i));
  }
  switch (mgr_conn_state)
  {
    case MgrClientConnection::MGR_NONE:
    {
      item->setText(1, QString());
      item->setIcon(1, QIcon(":/images/icons/circle16-gray.png"));
      break;
    }
    case MgrClientConnection::MGR_ERROR:
    {
      item->setIcon(1, QIcon(":/images/icons/circle16-red.png"));
      if (item == browserTree_mgrConnItem(ui->browserTree->currentItem()))
        currentMgrConn_disconnected();
      break;
    }
    case MgrClientConnection::MGR_AUTH:
    case MgrClientConnection::MGR_CONNECTING:
    {
      if (mgr_conn_state == MgrClientConnection::MGR_CONNECTING)
        item->setText(1, QString());
      item->setIcon(1, QIcon(":/images/icons/circle16-yellow.png"));
      break;
    }
    case MgrClientConnection::MGR_CONNECTED:
    {
      item->setIcon(1, QIcon(":/images/icons/circle16-green.png"));
      item->setText(1, QString("Connected"));
      emit gui_mgrserver_config_get(conn_id);
      emit gui_tunnel_config_get(conn_id);
      emit gui_tunnel_state_get(conn_id);
      break;
    }
    default:
      break;
  }
}

//---------------------------------------------------------------------------
void MainWindow::connection_latency_changed(quint32 conn_id, quint32 latency_ms)
{
  QTreeWidgetItem *item = browserTree_getMgrConnById(conn_id);
  if (!item)
    return;
  item->setText(1, QString("%1 ms").arg(latency_ms));
}

//---------------------------------------------------------------------------
void MainWindow::connection_error(quint32 conn_id, QAbstractSocket::SocketError error)
{
  QTreeWidgetItem *item = browserTree_getMgrConnById(conn_id);
  if (!item)
    return;
  switch(error)
  {
    case QAbstractSocket::ConnectionRefusedError:
      item->setText(1, QString("Conn.refused"));
      break;
    case QAbstractSocket::RemoteHostClosedError:
      if (item->text(1).isEmpty())
        item->setText(1, QString("Server closed"));
      break;
    case QAbstractSocket::HostNotFoundError:
      item->setText(1, QString("Host not found"));
      break;
    case QAbstractSocket::SocketAccessError:
      item->setText(1, QString("Permission denied"));
      break;
    case QAbstractSocket::SocketResourceError:
      item->setText(1, QString("Out of resources"));
      break;
    case QAbstractSocket::SocketTimeoutError:
      item->setText(1, QString("Timed out"));
      break;
    case QAbstractSocket::DatagramTooLargeError:
      item->setText(1, QString("Packet too large"));
      break;
    case QAbstractSocket::NetworkError:
      item->setText(1, QString("Network error"));
      break;
    case QAbstractSocket::AddressInUseError:
      item->setText(1, QString("Address in use"));
      break;
    case QAbstractSocket::SocketAddressNotAvailableError:
      item->setText(1, QString("Address not available"));
      break;
    case QAbstractSocket::UnsupportedSocketOperationError:
      item->setText(1, QString("No IPv6 support"));
      break;
    case QAbstractSocket::ProxyAuthenticationRequiredError:
      item->setText(1, QString("Auth.failed"));
      break;
    case QAbstractSocket::SslHandshakeFailedError:
      item->setText(1, QString("Handshake failed"));
      break;
    case QAbstractSocket::ProxyConnectionClosedError:
      item->setText(1, QString("Server cert.invalid"));
      break;
    case QAbstractSocket::ProxyProtocolError:
      item->setText(1, QString("Protocol error"));
      break;
    default:
      item->setText(1, QString("Error %1").arg((int)error));
      break;
  }
}

//---------------------------------------------------------------------------
void MainWindow::connection_password_required(quint32 conn_id)
{
  QTreeWidgetItem *item = browserTree_getMgrConnById(conn_id);
  if (!item)
    return;
  bool ok=false;
  QString password = QInputDialog::getText(NULL, tr("Password required"),
                                           tr("User password required for connection '%1':").arg(item->text(0)),
                                           QLineEdit::Password, QString(), &ok);
  if (ok && !password.isEmpty())
  {
    emit gui_mgrconn_set_password(conn_id, password);
  }
}

//---------------------------------------------------------------------------
void MainWindow::connection_passphrase_required(quint32 conn_id)
{
  QTreeWidgetItem *item = browserTree_getMgrConnById(conn_id);
  if (!item)
    return;
  bool ok=false;
  QString passphrase = QInputDialog::getText(NULL, tr("Passphrase required"),
                                           tr("Passphrase required for private key used in connection '%1':").arg(item->text(0)),
                                           QLineEdit::Password, QString(), &ok);
  if (ok && !passphrase.isEmpty())
  {
    emit gui_mgrconn_set_passphrase(conn_id, passphrase);
  }
}
//---------------------------------------------------------------------------
void MainWindow::connection_authorized(quint32 conn_id, quint64 access_flags)
{
  QTreeWidgetItem *item = browserTree_getMgrConnById(conn_id);
  if (!item)
    return;
  item->setData(1, MgrConnAccessFlagsRole, access_flags);
}
