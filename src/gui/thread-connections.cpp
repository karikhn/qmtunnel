/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "thread-connections.h"
#include "../lib/prc_log.h"
#include "../lib/tunnel-state.h"

CommThread *commThread=NULL;

//---------------------------------------------------------------------------
void commThread_init()
{
  qRegisterMetaType<MgrClientParameters>("MgrClientParameters");
  qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");
  qRegisterMetaType<TunnelId>("TunnelId");
  qRegisterMetaType<TunnelState>("TunnelState");
  commThread = new CommThread;
  commThread->start();
  while (!commThread->isRunning())
    ;
  commThread->obj->moveToThread(commThread);
}

//---------------------------------------------------------------------------
void commThread_free()
{
  delete commThread;
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_mgrConnChanged(quint32 conn_id, const MgrClientParameters &params)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (conn)
    conn->setParameters(&params);
  else
    create_new_connection(&params);
}

//---------------------------------------------------------------------------
void CommThreadObject::resetConnections()
{
  for (int i=0; i < sockets.count(); i++)
  {
    if (sockets[i]->state() == QSslSocket::ConnectedState)
    {
      emit mgrconn_state_changed(sockets[i]->params.id, MgrClientConnection::MGR_NONE);
      sockets[i]->disconnectFromHost();
      if (sockets[i]->state() == QSslSocket::ClosingState)
        sockets[i]->waitForDisconnected(100);
    }
  }
  while (!sockets.isEmpty())
  {
    delete sockets.takeFirst();
  }
}

//---------------------------------------------------------------------------
void CommThreadObject::slot_mgrconn_state_changed(quint16 mgr_conn_state)
{
  MgrClientConnection *conn = qobject_cast<MgrClientConnection *>(sender());
  if (!conn)
    return;
  emit mgrconn_state_changed(conn->params.id, mgr_conn_state);
}

//---------------------------------------------------------------------------
void CommThreadObject::slot_connection_error(QAbstractSocket::SocketError error)
{
  MgrClientConnection *conn = qobject_cast<MgrClientConnection *>(sender());
  if (!conn)
    return;
  emit mgrconn_socket_error(conn->params.id, error);
}

//---------------------------------------------------------------------------
void CommThreadObject::slot_latency_changed(quint32 latency_ms)
{
  MgrClientConnection *conn = qobject_cast<MgrClientConnection *>(sender());
  if (!conn)
    return;
  emit mgrconn_latency_changed(conn->params.id, latency_ms);
}

//---------------------------------------------------------------------------
void CommThreadObject::slot_password_required()
{
  MgrClientConnection *conn = qobject_cast<MgrClientConnection *>(sender());
  if (!conn)
    return;
  emit mgrconn_password_required(conn->params.id);
}

//---------------------------------------------------------------------------
void CommThreadObject::slot_passphrase_required()
{
  MgrClientConnection *conn = qobject_cast<MgrClientConnection *>(sender());
  if (!conn)
    return;
  emit mgrconn_passphrase_required(conn->params.id);
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_mgrconn_set_password(quint32 conn_id, const QString &password)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (!conn)
    return;
  conn->params.auth_password = password;
  conn->beginConnection();
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_mgrconn_set_passphrase(quint32 conn_id, const QString &passphrase)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (!conn)
    return;
  conn->private_key_passphrase = passphrase;
  conn->beginConnection();
}

//---------------------------------------------------------------------------
MgrClientConnection *CommThreadObject::connectionById(quint32 conn_id)
{
  for (int i=0; i < sockets.count(); i++)
  {
    if (sockets[i]->params.id == conn_id)
      return sockets[i];
  }
  return NULL;
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_mgrConnDemand(quint32 conn_id)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (!conn)
    return;
  conn->beginConnection();
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_mgrConnDemand_disconnect(quint32 conn_id)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (!conn)
    return;
  if (conn->state() == QSslSocket::ConnectedState)
  {
    conn->disconnectFromHost();
    emit mgrconn_state_changed(conn_id, MgrClientConnection::MGR_NONE);
  }
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_mgrConnDemand_idleStart(quint32 conn_id)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (!conn || !conn->timer_idle)
    return;
  conn->timer_idle->start();
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_mgrConnDemand_idleStop(quint32 conn_id)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (!conn || !conn->timer_idle)
    return;
  if (conn->timer_idle->isActive())
    conn->timer_idle->stop();
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_mgrserver_config_get(quint32 conn_id)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (!conn)
    return;
  // send config request
  conn->sendPacket(CMD_CONFIG_GET);
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_tunnel_config_get(quint32 conn_id, TunnelId tun_id)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (!conn)
    return;
  // send tunnel config request
  if (tun_id > 0)
    conn->sendPacket(CMD_TUN_CONFIG_GET, QByteArray((const char *)&tun_id, sizeof(TunnelId)));
  else
    conn->sendPacket(CMD_TUN_CONFIG_GET);
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_tunnel_state_get(quint32 conn_id, TunnelId tun_id)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (!conn)
    return;
  // send tunnel state request
  if (tun_id > 0)
    conn->sendPacket(CMD_TUN_STATE_GET, QByteArray((const char *)&tun_id, sizeof(TunnelId)));
  else
    conn->sendPacket(CMD_TUN_STATE_GET);
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_tunnel_connstate_get(quint32 conn_id, TunnelId tun_id, bool include_disconnected)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (!conn)
    return;
  // send tunnel connections state request
  quint8 flags = 0;
  if (include_disconnected)
    flags |= 0x01;
  if (tun_id > 0)
    conn->sendPacket(CMD_TUN_CONNSTATE_GET, QByteArray((const char *)&flags, sizeof(flags))+QByteArray((const char *)&tun_id, sizeof(TunnelId)));
  else
    conn->sendPacket(CMD_TUN_CONNSTATE_GET, QByteArray((const char *)&flags, sizeof(flags)));
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_tunnel_start(quint32 conn_id, TunnelId tun_id)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (!conn)
    return;
  if (tun_id > 0)
    conn->sendPacket(CMD_TUN_START, QByteArray((const char *)&tun_id, sizeof(TunnelId)));
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_tunnel_stop(quint32 conn_id, TunnelId tun_id)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (!conn)
    return;
  if (tun_id > 0)
    conn->sendPacket(CMD_TUN_STOP, QByteArray((const char *)&tun_id, sizeof(TunnelId)));
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_tunnel_remove(quint32 conn_id, TunnelId tun_id)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (!conn)
    return;
  // send tunnel remove request
  if (tun_id > 0)
    conn->sendPacket(CMD_TUN_REMOVE, QByteArray((const char *)&tun_id, sizeof(TunnelId)));
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_mgrserver_config_set(quint32 conn_id, cJSON *j_config)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (!conn)
    return;
  char *buffer = cJSON_PrintBuffered(j_config, 1024*64, 1);
  cJSON_Delete(j_config);
  // send config set request
  conn->sendPacket(CMD_CONFIG_SET, QByteArray(buffer));
  free(buffer);
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_tunnel_create(quint32 conn_id, cJSON *j_tun_params)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (!conn)
    return;
  char *buffer = cJSON_PrintBuffered(j_tun_params, 1024*64, 1);
  cJSON_Delete(j_tun_params);
  conn->sendPacket(CMD_TUN_CREATE, QByteArray(buffer));
  free(buffer);
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_tunnel_config_set(quint32 conn_id, cJSON *j_tun_params)
{
  MgrClientConnection *conn = connectionById(conn_id);
  if (!conn)
    return;
  char *buffer = cJSON_PrintBuffered(j_tun_params, 1024*64, 1);
  cJSON_Delete(j_tun_params);
  conn->sendPacket(CMD_TUN_CONFIG_SET, QByteArray(buffer));
  free(buffer);
}

//---------------------------------------------------------------------------
MgrClientConnection * CommThreadObject::create_new_connection(const MgrClientParameters *params)
{
  MgrClientConnection *conn = new MgrClientConnection(MgrClientConnection::OUTGOING);
  conn->moveToThread(QThread::currentThread());
  conn->params = *params;
  conn->log_prefix = QString("Mgrconn '%1': ").arg(params->name);
  sockets.append(conn);
  connect(conn, SIGNAL(state_changed(quint16)), this, SLOT(slot_mgrconn_state_changed(quint16)));
  connect(conn, SIGNAL(connection_error(QAbstractSocket::SocketError)), this, SLOT(slot_connection_error(QAbstractSocket::SocketError)));
  connect(conn, SIGNAL(latency_changed(quint32)), this, SLOT(slot_latency_changed(quint32)));
  connect(conn, SIGNAL(packetReceived(MgrPacketCmd,QByteArray)), this, SLOT(packetReceived(MgrPacketCmd,QByteArray)));
  connect(conn, SIGNAL(password_required()), this, SLOT(slot_password_required()));
  connect(conn, SIGNAL(passphrase_required()), this, SLOT(slot_passphrase_required()));
  conn->init();
  return conn;
}

//---------------------------------------------------------------------------
void CommThreadObject::load_profile_json_items(cJSON *j_items)
{
  int item_count = cJSON_GetArraySize(j_items);
  for (int item_index=0; item_index < item_count; item_index++)
  {
    cJSON *j_item = cJSON_GetArrayItem(j_items, item_index);
    cJSON *j = cJSON_GetObjectItem(j_item, "folder_name");
    if (!j)
    {
      MgrClientParameters conn_params;
      if (conn_params.parseJSON(j_item))
        create_new_connection(&conn_params);
    }

    cJSON *j_subitems = cJSON_GetObjectItem(j_item, "items");
    if (j_subitems && j_subitems->type == cJSON_Array)
      load_profile_json_items(j_subitems);
  }
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_profileLoaded(cJSON *j_profile)
{
  prc_log(LOG_DBG1, QString("Reset all connections - profile changed"));
  resetConnections();
  cJSON *j_items = cJSON_GetObjectItem(j_profile, "items");
  if (!j_items || j_items->type != cJSON_Array)
  {
    cJSON_Delete(j_profile);
    return;
  }
  load_profile_json_items(j_items);
  cJSON_Delete(j_profile);
}

//---------------------------------------------------------------------------
void CommThreadObject::packetReceived(MgrPacketCmd cmd, const QByteArray &data)
{
  MgrClientConnection *socket = qobject_cast<MgrClientConnection *>(sender());
  if (!socket)
    return;

  switch (cmd)
  {
    case CMD_AUTH_REP:
      authRepPacketReceived(socket, data);
      break;
    case CMD_CONFIG_GET:
    {
      if (receivers(SIGNAL(mgrconn_config_received(quint32, cJSON *))) > 0)
      {
        cJSON *j_config = cJSON_Parse(data);
        if (j_config)
          emit mgrconn_config_received(socket->params.id, j_config);
      }
      break;
    }
    case CMD_TUN_CREATE_REPLY:
    {
      cmd_tun_create_reply_received(socket, data);
      break;
    }
    case CMD_TUN_CONFIG_GET:
    {
      if (receivers(SIGNAL(tunnel_config_received(quint32, cJSON *))) > 0)
      {
        cJSON *j_config = cJSON_Parse(data);
        if (j_config)
          emit tunnel_config_received(socket->params.id, j_config);
      }
      break;
    }
    case CMD_TUN_STATE_GET:
    {
      if (data.length() > (int)sizeof(TunnelId)+2)
      {
        const char *data_buf = data.constData();
        TunnelId tun_id = *((TunnelId *)data_buf);
        TunnelState tun_state;
        if (tun_state.parseFromBuffer(data_buf+sizeof(TunnelId), data.length()-sizeof(TunnelId)))
          emit tunnel_state_received(socket->params.id, tun_id, tun_state);
      }
      break;
    }
    case CMD_TUN_CONNSTATE_GET:
    {
      emit tunnel_connstate_received(socket->params.id, data);
      break;
    }
    case CMD_TUN_CONFIG_SET:
    case CMD_TUN_REMOVE:
    case CMD_CONFIG_SET_ERROR:
    {
      standartReplyPacketReceived(socket, cmd, data);
      break;
    }
    default:
      socket->log(LOG_HIGH, QString(": Unknown packet cmd %1 - dropping connection").arg(cmd));
      socket->abort();
      break;
  }
}

//---------------------------------------------------------------------------
void CommThreadObject::cmd_tun_create_reply_received(MgrClientConnection *socket, const QByteArray &req_data)
{
  if (req_data.length() < (int)(sizeof(MgrPacket_TunnelCreateReply)))
  {
    socket->log(LOG_HIGH, QString(": CMD_TUN_CREATE_REPLY packet too short"));
    socket->abort();
    return;
  }
  MgrPacket_TunnelCreateReply *packet = (MgrPacket_TunnelCreateReply *)req_data.data();
  if (req_data.length() < (int)sizeof(MgrPacket_TunnelCreateReply)+packet->error_len)
  {
    socket->log(LOG_HIGH, QString(": CMD_TUN_CREATE_REPLY packet too short"));
    socket->abort();
    return;
  }
  QString error_str;
  if (packet->error_len > 0)
    error_str = QString::fromUtf8(req_data.mid(sizeof(MgrPacket_TunnelCreateReply),packet->error_len));
  emit mgrconn_tunnel_create_reply(socket->params.id, packet->orig_tunnel_id, packet->tunnel_id, packet->res_code, error_str);
}

//---------------------------------------------------------------------------
void CommThreadObject::standartReplyPacketReceived(MgrClientConnection *socket, MgrPacketCmd cmd, const QByteArray &req_data)
{
  if (req_data.length() < (int)sizeof(MgrPacket_StandartReply))
  {
    socket->log(LOG_HIGH, QString(": MgrPacket_StandartReply packet too short"));
    socket->abort();
    return;
  }
  MgrPacket_StandartReply *packet = (MgrPacket_StandartReply *)req_data.data();
  if (req_data.length() < (int)sizeof(MgrPacket_StandartReply)+packet->obj_id_len+packet->error_len)
  {
    socket->log(LOG_HIGH, QString(": MgrPacket_StandartReply packet too short"));
    socket->abort();
    return;
  }
  QByteArray obj_id;
  if (packet->obj_id_len > 0)
    obj_id = req_data.mid(sizeof(MgrPacket_StandartReply),packet->obj_id_len);
  QString error_str;
  if (packet->error_len > 0)
    error_str = QString::fromUtf8(req_data.mid(sizeof(MgrPacket_StandartReply)+packet->obj_id_len,packet->error_len));
  if (cmd == CMD_CONFIG_SET_ERROR)
    emit mgrconn_config_error(socket->params.id, packet->res_code, error_str);
  else if (cmd == CMD_TUN_CONFIG_SET)
    emit mgrconn_tunnel_config_set_reply(socket->params.id, *((TunnelId *)obj_id.data()), packet->res_code, error_str);
  else if (cmd == CMD_TUN_REMOVE)
    emit mgrconn_tunnel_remove(socket->params.id, *((TunnelId *)obj_id.data()), packet->res_code, error_str);
}

//---------------------------------------------------------------------------
void CommThreadObject::authRepPacketReceived(MgrClientConnection *socket, const QByteArray &req_data)
{
  if (req_data.length() < (int)sizeof(MgrPacket_AuthRep))
  {
    socket->log(LOG_HIGH, QString(": MgrPacket_AuthRep packet too short"));
    socket->abort();
    return;
  }
  MgrPacket_AuthRep *rep = (MgrPacket_AuthRep *)req_data.data();
  if (req_data.length() < (int)sizeof(MgrPacket_AuthRep)+rep->server_hostname_len)
  {
    socket->log(LOG_HIGH, QString(": MgrPacket_AuthRep packet too short"));
    socket->abort();
    return;
  }

  socket->peer_hostname = QString::fromUtf8(req_data.mid(sizeof(MgrPacket_AuthRep),rep->server_hostname_len));

  if (rep->auth_result != MgrPacket_AuthRep::RES_CODE_OK)
  {
    socket->log(LOG_HIGH, QString(": user %1 login to %2 failed").arg(socket->params.auth_username).arg(socket->peer_hostname));
    emit mgrconn_state_changed(socket->params.id, MgrClientConnection::MGR_ERROR);
    emit mgrconn_socket_error(socket->params.id, QAbstractSocket::ProxyAuthenticationRequiredError);
    socket->disconnectFromHost();
  }
  else
  {
    socket->socket_startOperational();
    emit mgrconn_authorized(socket->params.id, rep->access_flags);
    socket->log(LOG_DBG1, QString(": user %1 logged in to %2").arg(socket->params.auth_username).arg(socket->peer_hostname));
  }
}

//---------------------------------------------------------------------------
void CommThreadObject::gui_closing()
{
  QThread::currentThread()->quit();
}

//---------------------------------------------------------------------------
void CommThread::run()
{
  prc_log(LOG_DBG2, QString("communication thread started"));
  this->exec();
  prc_log(LOG_DBG3, QString("stopping communication thread"));
  obj->resetConnections();
  prc_log(LOG_DBG2, QString("communication thread stopped"));
}
