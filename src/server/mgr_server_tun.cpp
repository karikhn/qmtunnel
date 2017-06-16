/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "mgr_server.h"
#include "../lib/prc_log.h"
#include "../lib/sys_util.h"

//---------------------------------------------------------------------------
void MgrServer::send_tunnel_config(MgrClientConnection *socket, Tunnel *tunnel)
{
  bool can_modify = isTunnelConfigChangeAllowed(tunnel, socket);
  if (!(tunnel->params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
    can_modify = false;

  cJSON *json = cJSON_CreateObject();
  tunnel->params.printJSON(json);
  if (can_modify)
    cJSON_AddTrueToObject(json, "can_modify");
  else
    cJSON_AddFalseToObject(json, "can_modify");
  char *buffer = cJSON_PrintBuffered(json, 1024*64, 1);
  cJSON_Delete(json);
  socket->sendPacket(CMD_TUN_CONFIG_GET, QByteArray(buffer));
  free(buffer);
}

//---------------------------------------------------------------------------
void MgrServer::send_tunnel_state(MgrClientConnection *socket, Tunnel *tunnel, bool include_stat)
{
  QByteArray packet_data;
  packet_data.append((const char *)&tunnel->params.id, sizeof(TunnelId));
  packet_data.append(tunnel->state.printToBuffer(include_stat));
  socket->sendPacket(CMD_TUN_STATE_GET, packet_data);
}

//---------------------------------------------------------------------------
void MgrServer::send_tunnel_connstate(MgrClientConnection *socket, Tunnel *tunnel, bool include_disconnected)
{
  QByteArray packet_data;
  packet_data.append((const char *)&tunnel->params.id, sizeof(TunnelId));
  packet_data.append(tunnel->in_connPrintToBuffer(include_disconnected));
  socket->sendPacket(CMD_TUN_CONNSTATE_GET, packet_data);
}

//---------------------------------------------------------------------------
void MgrServer::cmd_tunnel_create(MgrClientConnection *socket, const QByteArray &req_data)
{
  User *user = userById(mgrconn_state_list_in[socket]->user_id);
  if (!user)
    return;
  UserGroup *userGroup = userGroupById(user->group_id);
  if (!userGroup)
    return;

  cJSON *j_tunnel = cJSON_Parse(req_data);
  if (!j_tunnel)
  {
    socket->log(LOG_DBG1, QString(": tunnel_create() parsing error"));
    socket->abort();
    return;
  }

  TunnelParameters tun_params;
  tun_params.parseJSON(j_tunnel);
  cJSON_Delete(j_tunnel);

  quint16 res_code = TunnelState::RES_CODE_OK;
  QString error_str;

  if (!(userGroup->access_flags & UserGroup::AF_TUNNEL_CREATE) &&
      !(userGroup->access_flags & UserGroup::AF_TUNNEL_GROUP_ADMIN) &&
      !(userGroup->access_flags & UserGroup::AF_TUNNEL_SUPER_ADMIN))
  {
    res_code = TunnelState::RES_CODE_PERMISSION_DENIED;
    error_str = QString("Permission denied on '%1', tunnel creation is not allowed for user '%2'").arg(SysUtil::machine_name).arg(user->name);
    socket->log(LOG_LOW, QString(": ")+error_str);
    MgrPacket_TunnelCreateReply rep_packet;
    rep_packet.orig_tunnel_id = tun_params.id;
    rep_packet.res_code = res_code;
    rep_packet.error_len = error_str.toUtf8().length();
    QByteArray rep_packet_data;
    rep_packet_data.reserve(sizeof(MgrPacket_TunnelCreateReply)+error_str.toUtf8().length());
    rep_packet_data.append((const char *)&rep_packet, sizeof(MgrPacket_TunnelCreateReply));
    rep_packet_data.append(error_str.toUtf8());
    socket->sendPacket(CMD_TUN_CREATE_REPLY, rep_packet_data);
    return;
  }

  bool tun_allowed = userGroup->tunnel_allow_list.isEmpty();
  QString tunnel_rule = tun_params.tunnelRule();
  for (int rule_index=0; rule_index < userGroup->tunnel_allow_list.count(); rule_index++)
  {
    if (tun_params.matchesRule(userGroup->tunnel_allow_list[rule_index]))
    {
      tun_allowed = true;
      break;
    }
  }
  if (!tun_allowed)
  {
    res_code = TunnelState::RES_CODE_PERMISSION_DENIED;
    error_str = QString("Permission denied on '%1', tunnel '%2' creation is not allowed for user '%3'").arg(SysUtil::machine_name).arg(tunnel_rule).arg(user->name);
    socket->log(LOG_LOW, QString(": ")+error_str);
    MgrPacket_TunnelCreateReply rep_packet;
    rep_packet.orig_tunnel_id = tun_params.id;
    rep_packet.res_code = res_code;
    rep_packet.error_len = error_str.toUtf8().length();
    QByteArray rep_packet_data;
    rep_packet_data.reserve(sizeof(MgrPacket_TunnelCreateReply)+error_str.toUtf8().length());
    rep_packet_data.append((const char *)&rep_packet, sizeof(MgrPacket_TunnelCreateReply));
    rep_packet_data.append(error_str.toUtf8());
    socket->sendPacket(CMD_TUN_CREATE_REPLY, rep_packet_data);
    return;
  }

  if (!tunnel_checkParams(&tun_params, res_code, error_str))
  {
    MgrPacket_TunnelCreateReply rep_packet;
    rep_packet.orig_tunnel_id = tun_params.id;
    rep_packet.res_code = res_code;
    rep_packet.error_len = error_str.toUtf8().length();
    QByteArray rep_packet_data;
    rep_packet_data.reserve(sizeof(MgrPacket_TunnelCreateReply)+error_str.toUtf8().length());
    rep_packet_data.append((const char *)&rep_packet, sizeof(MgrPacket_TunnelCreateReply));
    rep_packet_data.append(error_str.toUtf8());
    socket->sendPacket(CMD_TUN_CREATE_REPLY, rep_packet_data);
    return;
  }

  Tunnel *tunnel = hash_tunnels.value(tun_params.next_id);
  // if such tunnel already exists
  if (tunnel && tunnel->params.orig_id == tun_params.id &&
      tunnel->params.owner_user_id == user->id &&
      tunnel->params.owner_group_id == userGroup->id &&
      (tunnel->state.flags & TunnelState::TF_STARTED))
  {
    if (tunnel->mgrconn_in)
      hash_tunnel_mgconn_in.remove(tunnel->mgrconn_in);
    tunnel->mgrconn_in = socket;
    tunnel->setNewParams(&tun_params);
    hash_tunnel_mgconn_in.insert(socket, tunnel);
    tunnel->mgrconn_in_restored();
    return;
  }

  tunnel = new Tunnel;
  tunnel->moveToThread(QThread::currentThread());
  connect(tunnel, SIGNAL(stopped()), this, SLOT(tunnel_stopped()));
  connect(tunnel, SIGNAL(state_changed()), this, SLOT(tunnel_state_changed()));
  tunnel->state.flags &= ~TunnelState::TF_CHECK_PASSED;
  tunnel->state.flags |= TunnelState::TF_MGRCONN_IN_CLEAR_TO_SEND;
  tunnel->state.flags |= TunnelState::TF_MGRCONN_IN_CONNECTED;
  tunnel->mgrconn_in = socket;
  tunnel->params = tun_params;
  if (!(tunnel->params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
    tunnel->mgrconn_in->log_prefix = QString("Tunnel '%1' mgrconn_in: ").arg(tunnel->params.name);
  tunnel->params.orig_id = tun_params.id;
  tunnel->params.id = unique_tunnel_id++;
  // ensure tunnel ID is unique on this server
  while (tunnel->params.id == 0 || hash_tunnels.contains(tunnel->params.id))
    tunnel->params.id = unique_tunnel_id++;
  tunnel->params.owner_user_id = user->id;
  tunnel->params.owner_group_id = userGroup->id;
  hash_tunnel_mgconn_in.insert(socket, tunnel);
  tunnel_add(tunnel);
  socket->log(LOG_DBG1, QString(": creating tunnel '%1' id=%2").arg(tunnel->params.name).arg(tunnel->params.id));

  // send configuration to subscribed clients
  QHashIterator<MgrClientConnection *, MgrClientState *> iterator(mgrconn_state_list_in);
  while (iterator.hasNext())
  {
    iterator.next();
    if (iterator.value()->flags & MgrClientState::FL_TUNNELS_CONFIG)
    {
      if (isTunnelViewAllowed(tunnel, userById(iterator.value()->user_id)))
        send_tunnel_config(iterator.key(), tunnel);
    }
  }

  tunnel->start();
}

//---------------------------------------------------------------------------
void MgrServer::cmd_tun_start(MgrClientConnection *socket, const QByteArray &data)
{
  Tunnel *tunnel = NULL;
  if (data.length() >= (int)sizeof(TunnelId))
    tunnel = hash_tunnels.value(*((TunnelId *)data.data()));
  else
    tunnel = hash_tunnel_mgconn_in.value(socket);
  if (!tunnel)
    return;
  if (!isTunnelControlAllowed(tunnel, socket))
    return;
  tunnel->log(LOG_DBG1, QString(": starting tunnel due to CMD_TUN_START command from %1 (%2:%3)").arg(socket->peer_hostname).arg(socket->peerAddress().toString()).arg(socket->peerPort()));
  tunnel->start();
}

//---------------------------------------------------------------------------
void MgrServer::cmd_tun_stop(MgrClientConnection *socket, const QByteArray &data)
{
  Tunnel *tunnel = NULL;
  if (data.length() >= (int)sizeof(TunnelId))
    tunnel = hash_tunnels.value(*((TunnelId *)data.data()));
  else
    tunnel = hash_tunnel_mgconn_in.value(socket);
  if (!tunnel)
    return;
  if (!isTunnelControlAllowed(tunnel, socket))
    return;

  tunnel->log(LOG_DBG1, QString(": stopping tunnel due to CMD_TUN_STOP command from %1 (%2:%3)").arg(socket->peer_hostname).arg(socket->peerAddress().toString()).arg(socket->peerPort()));
  tunnel->state.flags |= TunnelState::TF_STOPPING;
  if (tunnel->mgrconn_out && (tunnel->state.flags & TunnelState::TF_MGRCONN_OUT_CONNECTED) && tunnel->mgrconn_out->output_buffer.length()+tunnel->mgrconn_out->bytesToWrite() < 64*1024)
  {
    tunnel->mgrconn_out->sendPacket(CMD_TUN_STOP);
    tunnel->mgrconn_out->sendPacket(CMD_CLOSE);
  }
  else
    tunnel->stop();
}

//---------------------------------------------------------------------------
void MgrServer::cmd_tun_remove(MgrClientConnection *socket, const QByteArray &data)
{
  Tunnel *tunnel = NULL;
  if (data.length() >= (int)sizeof(TunnelId))
    tunnel = hash_tunnels.value(*((TunnelId *)data.data()));
  else
    tunnel = hash_tunnel_mgconn_in.value(socket);
  if (!tunnel)
    return;
  if (!isTunnelConfigChangeAllowed(tunnel, socket))
    return;
  tunnel->log(LOG_DBG1, QString(": stopping and removing tunnel due to CMD_TUN_REMOVE command from %1 (%2:%3)").arg(socket->peer_hostname).arg(socket->peerAddress().toString()).arg(socket->peerPort()));
  tunnel->state.flags |= TunnelState::TF_STOPPING;
  tunnel->to_be_deleted = true;
  tunnel->params.flags &= ~TunnelParameters::FL_SAVE_IN_CONFIG_PERMANENTLY;
  if (!flag_kill && this->isListening())
  {
    prc_log(LOG_DBG2, QString("saving tunnels in configuration file"));
    config_save(&params);
  }
  if (tunnel->mgrconn_out && (tunnel->state.flags & TunnelState::TF_MGRCONN_OUT_CONNECTED) && tunnel->mgrconn_out->output_buffer.length()+tunnel->mgrconn_out->bytesToWrite() < 64*1024)
  {
    tunnel->mgrconn_out->sendPacket(CMD_TUN_STOP);
    tunnel->mgrconn_out->sendPacket(CMD_CLOSE);
  }
  else
    tunnel->stop();
}

//---------------------------------------------------------------------------
void MgrServer::cmd_tunnel_config_get(MgrClientConnection *socket)
{
  User *user = userById(mgrconn_state_list_in[socket]->user_id);
  if (!user)
    return;

  mgrconn_state_list_in[socket]->flags |= MgrClientState::FL_TUNNELS_CONFIG;
  for (int i=0; i < tunnels.count(); i++)
  {
    if (isTunnelViewAllowed(tunnels[i], user))
      send_tunnel_config(socket, tunnels[i]);
  }
}

//---------------------------------------------------------------------------
void MgrServer::cmd_tun_state_get(MgrClientConnection *socket, const QByteArray &data)
{
  User *user = userById(mgrconn_state_list_in[socket]->user_id);
  if (!user)
    return;
  if (data.length() >= (int)sizeof(TunnelId))
  {
    Tunnel *tunnel = hash_tunnels.value(*((TunnelId *)data.data()));
    if (isTunnelViewAllowed(tunnel, user))
      send_tunnel_state(socket, tunnel, true);
    return;
  }

  mgrconn_state_list_in[socket]->flags |= MgrClientState::FL_TUNNELS_STATE;

  for (int i=0; i < tunnels.count(); i++)
  {
    if (isTunnelViewAllowed(tunnels[i], user))
      send_tunnel_state(socket, tunnels[i], true);
  }
}

//---------------------------------------------------------------------------
void MgrServer::cmd_tun_connstate_get(MgrClientConnection *socket, const QByteArray &data)
{
  User *user = userById(mgrconn_state_list_in[socket]->user_id);
  if (!user)
    return;
  if (data.length() < (int)sizeof(quint8))
    return;
  quint8 flags = *((quint8 *)data.data());
  if (data.length() >= (int)(sizeof(quint8)+sizeof(TunnelId)))
  {
    Tunnel *tunnel = hash_tunnels.value(*((TunnelId *)(data.data()+sizeof(quint8))));
    if (isTunnelViewAllowed(tunnel, user))
      send_tunnel_connstate(socket, tunnel, flags & 0x01);
    return;
  }

  for (int i=0; i < tunnels.count(); i++)
  {
    if (isTunnelViewAllowed(tunnels[i], user))
      send_tunnel_connstate(socket, tunnels[i], flags & 0x01);
  }
}

//---------------------------------------------------------------------------
void MgrServer::cmd_tunnel_config_set(MgrClientConnection *socket, const QByteArray &req_data)
{
  cJSON *j_tunnel = cJSON_Parse(req_data);
  if (!j_tunnel)
  {
    socket->log(LOG_DBG1, QString(": tunnel_config_set() parsing error"));
    socket->abort();
    return;
  }
  User *user = userById(mgrconn_state_list_in[socket]->user_id);
  if (!user)
    return;
  UserGroup *userGroup = userGroupById(user->group_id);
  if (!userGroup)
    return;

  TunnelParameters tun_params;
  tun_params.parseJSON(j_tunnel);
  cJSON_Delete(j_tunnel);

  Tunnel *tunnel = hash_tunnels.value(tun_params.id);
  if (!tunnel)
  {
    send_standart_reply(socket, CMD_TUN_CONFIG_SET, QByteArray((const char *)&tun_params.id, sizeof(TunnelId)),
                        TunnelState::RES_CODE_TUNNEL_NOT_FOUND, QString("Tunnel not found"));
    return;
  }

  if (!isTunnelConfigChangeAllowed(tunnel, socket))
  {
    send_standart_reply(socket, CMD_TUN_CONFIG_SET, QByteArray((const char *)&tun_params.id, sizeof(TunnelId)),
                        TunnelState::RES_CODE_PERMISSION_DENIED, QString("Permission denied"));
    return;
  }

  if (!(tunnel->params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
  {
    send_standart_reply(socket, CMD_TUN_CONFIG_SET, QByteArray((const char *)&tun_params.id, sizeof(TunnelId)),
                        TunnelState::RES_CODE_PERMISSION_DENIED, QString("Tunnel configuration can only be modified on the first (master) tunserver"));
    return;
  }

  quint16 res_code = TunnelState::RES_CODE_OK;
  QString error_str;
  if (!tunnel_checkParams(&tun_params, res_code, error_str))
  {
    send_standart_reply(socket, CMD_TUN_CONFIG_SET, QByteArray((const char *)&tun_params.id, sizeof(TunnelId)), res_code, error_str);
    return;
  }

  tunnel->setNewParams(&tun_params);

  send_standart_reply(socket, CMD_TUN_CONFIG_SET, QByteArray((const char *)&tun_params.id, sizeof(TunnelId)), res_code, error_str);

  // send configuration to subscribed clients
  QHashIterator<MgrClientConnection *, MgrClientState *> iterator(mgrconn_state_list_in);
  while (iterator.hasNext())
  {
    iterator.next();
    if (iterator.value()->flags & MgrClientState::FL_TUNNELS_CONFIG)
    {
      if (isTunnelViewAllowed(tunnel, userById(iterator.value()->user_id)))
        send_tunnel_config(iterator.key(), tunnel);
    }
  }
}

//---------------------------------------------------------------------------
bool MgrServer::tunnel_checkParams(TunnelParameters *tun_params, quint16 &res_code, QString &error_str)
{
  // check name
  if (tun_params->name.trimmed().isEmpty())
  {
    res_code = TunnelState::RES_CODE_NAME_ERROR;
    error_str = Tunnel::tr("Tunnel name is empty");
    return false;
  }
  if (tun_params->name.length() > 255)
  {
    res_code = TunnelState::RES_CODE_NAME_ERROR;
    error_str = Tunnel::tr("Tunnel name is too long");
    return false;
  }

  // if we need to check remote host and port right on this server
  if (tun_params->fwd_direction == TunnelParameters::LOCAL_TO_REMOTE && tun_params->tunservers.isEmpty())
  {
    if (tun_params->remote_host.trimmed().isEmpty())
    {
      res_code = TunnelState::RES_CODE_REMOTE_HOST_ERROR;
      error_str = Tunnel::tr("Invalid remote host '%1'").arg(tun_params->remote_host);
      return false;
    }
  }

  return true;
}

//---------------------------------------------------------------------------
void MgrServer::tunnel_owner_mgrconn_removed(MgrClientConnection *socket)
{
  Tunnel *tunnel = hash_tunnel_mgconn_in.take(socket);
  if (tunnel)
    tunnel->mgrconn_in_disconnected();
}

//---------------------------------------------------------------------------
void MgrServer::tunnel_add(Tunnel *tunnel)
{
  tunnels.append(tunnel);
  hash_tunnels.insert(tunnel->params.id, tunnel);
}

//---------------------------------------------------------------------------
void MgrServer::cmd_tun(MgrClientConnection *socket, MgrPacketCmd cmd, const QByteArray &data)
{
  switch (cmd)
  {
    case CMD_TUN_CREATE:
      cmd_tunnel_create(socket, data);
      break;
    case CMD_TUN_CONFIG_GET:
      cmd_tunnel_config_get(socket);
      break;
    case CMD_TUN_CONFIG_SET:
      cmd_tunnel_config_set(socket, data);
      break;
    case CMD_TUN_START:
      cmd_tun_start(socket, data);
      break;
    case CMD_TUN_STOP:
      cmd_tun_stop(socket, data);
      break;
    case CMD_TUN_REMOVE:
      cmd_tun_remove(socket, data);
      break;
    case CMD_TUN_STATE_GET:
      cmd_tun_state_get(socket, data);
      break;
    case CMD_TUN_CONNSTATE_GET:
      cmd_tun_connstate_get(socket, data);
      break;
    case CMD_TUN_CHAIN_HEARTBEAT_REQ:
    case CMD_TUN_CHAIN_HEARTBEAT_REP:
    {
      Tunnel *tunnel = hash_tunnel_mgconn_in.value(socket);
      if (tunnel)
        tunnel->cmd_tun_heartbeat(cmd, socket);
      break;
    }
    case CMD_TUN_BUFFER_ACK:
    {
      Tunnel *tunnel = hash_tunnel_mgconn_in.value(socket);
      if (tunnel)
        tunnel->cmd_tun_buffer_ack_received(socket, data);
      break;
    }
    case CMD_TUN_BUFFER_RESEND_FROM:
    {
      Tunnel *tunnel = hash_tunnel_mgconn_in.value(socket);
      if (tunnel)
        tunnel->cmd_tun_buffer_resend_from(socket, data);
      break;
    }
    case CMD_TUN_CHAIN_BROKEN:
    {
      Tunnel *tunnel = hash_tunnel_mgconn_in.value(socket);
      if (tunnel)
        tunnel->cmd_tun_chain_broken(socket);
      break;
    }
    case CMD_TUN_CHAIN_CHECK:
    {
      Tunnel *tunnel = hash_tunnel_mgconn_in.value(socket);
      if (tunnel)
        tunnel->cmd_tun_chain_check(socket);
      break;
    }
    case CMD_TUN_CONN_OUT_NEW:
    case CMD_TUN_CONN_OUT_DROP:
    case CMD_TUN_CONN_OUT_CONNECTED:
    case CMD_TUN_CONN_IN_DROP:
    case CMD_TUN_CONN_IN_DATA:
    case CMD_TUN_CONN_OUT_DATA:
    {
      Tunnel *tunnel = hash_tunnel_mgconn_in.value(socket);
      if (!tunnel)
        return;
      if (!tunnel->t_last_buffered_packet_ack_rcv.isValid())
        tunnel->t_last_buffered_packet_ack_rcv.start();
      if (tunnel->forward_packet(socket, cmd, data))
        break;
      if (data.length() < (int)(sizeof(TunnelConnPacketId)+sizeof(TunnelConnId)))
      {
        socket->log(LOG_DBG1, QString(": CMD_TUN_CONN_... packet too short"));
        socket->abort();
        return;
      }
      TunnelConnPacketId packet_id = *((TunnelConnPacketId *)data.data());
      TunnelConnId conn_id = *((TunnelConnId *)(data.data()+sizeof(TunnelConnPacketId)));
      if (!tunnel->buffered_packet_received(packet_id, data.length()))
        return;
      switch (cmd)
      {
        case CMD_TUN_CONN_OUT_NEW:
        {
          tunnel->cmd_conn_out_new(conn_id);
          break;
        }
        case CMD_TUN_CONN_OUT_DROP:
        {
          tunnel->cmd_conn_out_drop(conn_id, data.mid(sizeof(TunnelConnPacketId)+sizeof(TunnelConnId)));
          break;
        }
        case CMD_TUN_CONN_OUT_CONNECTED:
        {
          tunnel->cmd_conn_out_connected(conn_id, data.mid(sizeof(TunnelConnPacketId)+sizeof(TunnelConnId)));
          break;
        }
        case CMD_TUN_CONN_IN_DROP:
        {
          tunnel->cmd_conn_in_drop(conn_id, data.mid(sizeof(TunnelConnPacketId)+sizeof(TunnelConnId)));
          break;
        }
        case CMD_TUN_CONN_IN_DATA:
        case CMD_TUN_CONN_OUT_DATA:
        {
          tunnel->cmd_conn_data(cmd == CMD_TUN_CONN_IN_DATA ? TunnelConn::INCOMING : TunnelConn::OUTGOING,
                      conn_id, data.mid(sizeof(TunnelConnPacketId)+sizeof(TunnelConnId)));
        }
        default:
          break;
      }
      break;
    }
    default:
    {
      socket->log(LOG_DBG1, QString(": Unknown packet cmd %1 - dropping connection").arg(cmd));
      socket->abort();
      break;
      break;
    }
  }
}

//---------------------------------------------------------------------------
void MgrServer::tunnel_remove(Tunnel *tunnel)
{
  if (!tunnels.contains(tunnel))
    return;
  tunnel->to_be_deleted = true;
  tunnel->stop();
  tunnels.removeOne(tunnel);
  hash_tunnels.remove(tunnel->params.id);
  if (tunnel->mgrconn_in)
    hash_tunnel_mgconn_in.remove(tunnel->mgrconn_in);

  // send notification to subscribed clients
  QHashIterator<MgrClientConnection *, MgrClientState *> iterator(mgrconn_state_list_in);
  while (iterator.hasNext())
  {
    iterator.next();
    if (iterator.value()->flags & MgrClientState::FL_TUNNELS_CONFIG)
    {
      if (isTunnelViewAllowed(tunnel, userById(iterator.value()->user_id)))
        send_standart_reply(iterator.key(), CMD_TUN_REMOVE, QByteArray((const char *)&tunnel->params.id, sizeof(TunnelId)), TunnelState::RES_CODE_OK, QString());
    }
  }

  tunnel->deleteLater();
}

//---------------------------------------------------------------------------
void MgrServer::tunnel_stopped()
{
  Tunnel *tunnel = qobject_cast<Tunnel *>(sender());
  if (!tunnel)
    return;
  if (!tunnel->restart_after_stop &&
      (!(tunnel->state.flags & TunnelState::TF_CHECK_PASSED) ||
       !(tunnel->params.flags & TunnelParameters::FL_MASTER_TUNSERVER) ||
         tunnel->to_be_deleted))
    tunnel_remove(tunnel);
}


//---------------------------------------------------------------------------
void MgrServer::tunnel_state_changed()
{
  Tunnel *tunnel = qobject_cast<Tunnel *>(sender());
  if (!tunnel)
    return;

//  tunnel->log(LOG_DBG1, QString("flags = 0x%1").arg(tunnel->state.flags, 0, 16));
//  tunnel->log(LOG_DBG1, QString("last_error_code = %1").arg(tunnel->state.last_error_code));

  // limit notifications frequency
  if (tunnel->params.max_state_dispatch_frequency > 0)
  {
    unsigned int ms_since_last_state_dispatch = qAbs(tunnel->t_last_state_sent.elapsed());
    if (tunnel->t_last_state_sent.isValid() && ms_since_last_state_dispatch < tunnel->params.max_state_dispatch_frequency)
    {
      if (!tunnel->state_dispatch_queued)
      {
        tunnel->state_dispatch_queued = true;
        QTimer::singleShot(tunnel->params.max_state_dispatch_frequency-ms_since_last_state_dispatch, tunnel, SIGNAL(state_changed()));
      }
      return;
    }
  }

  // send notification to subscribed clients
  QHashIterator<MgrClientConnection *, MgrClientState *> iterator(mgrconn_state_list_in);
  while (iterator.hasNext())
  {
    iterator.next();
    if (iterator.value()->flags & MgrClientState::FL_TUNNELS_STATE)
    {
      if (isTunnelViewAllowed(tunnel, userById(iterator.value()->user_id)))
        send_tunnel_state(iterator.key(), tunnel, false);
    }
  }
  tunnel->t_last_state_sent.restart();
  tunnel->state_dispatch_queued = false;
}

//---------------------------------------------------------------------------
void MgrServer::tunnels_stop()
{
  for (int i=0; i < tunnels.count(); i++)
    tunnels[i]->stop();
}

//---------------------------------------------------------------------------
void MgrServer::tunnels_start()
{
  for (int i=0; i < tunnels.count(); i++)
    tunnels[i]->start();
}

//---------------------------------------------------------------------------
void MgrServer::tunnels_clear()
{
  tunnels_stop();
  for (int i=tunnels.count()-1; i >= 0; i--)
    tunnel_remove(tunnels[i]);
}

//---------------------------------------------------------------------------
void MgrServer::config_load_tunnels(cJSON *j_config)
{
  for (int i=tunnels.count()-1; i >= 0; i--)
  {
    if (tunnels[i]->params.flags & TunnelParameters::FL_SAVE_IN_CONFIG_PERMANENTLY)
      tunnel_remove(tunnels[i]);
  }

  cJSON *j_tunnels = cJSON_GetObjectItem(j_config, "tunnels");
  if (!j_tunnels || j_tunnels->type != cJSON_Array)
    return;
  int tunnel_count = cJSON_GetArraySize(j_tunnels);
  for (int i=0; i < tunnel_count; i++)
  {
    cJSON *j_item = cJSON_GetArrayItem(j_tunnels, i);
    Tunnel *tunnel = new Tunnel;
    tunnel->moveToThread(QThread::currentThread());
    connect(tunnel, SIGNAL(stopped()), this, SLOT(tunnel_stopped()));
    connect(tunnel, SIGNAL(state_changed()), this, SLOT(tunnel_state_changed()));
    tunnel->params.parseJSON(j_item);
    tunnel->state.flags |= TunnelState::TF_CHECK_PASSED
                        |  TunnelState::TF_SAVED_IN_CONFIG;
    tunnel->log(LOG_DBG1, QString(": loaded from config (id=%1)").arg(tunnel->params.id));
    tunnel_add(tunnel);
  }
}

//---------------------------------------------------------------------------
void MgrServer::config_save_tunnels(cJSON *j_config)
{
  cJSON *j_tunnels = cJSON_GetObjectItem(j_config, "tunnels");
  if (j_tunnels)
    cJSON_DeleteItemFromObject(j_config, "tunnels");
  j_tunnels = cJSON_CreateArray();
  cJSON_AddItemToObject(j_config, "tunnels", j_tunnels);

  for (int i=0; i < tunnels.count(); i++)
  {
    if ((tunnels[i]->params.flags & TunnelParameters::FL_SAVE_IN_CONFIG_PERMANENTLY) &&
        (tunnels[i]->state.flags & TunnelState::TF_CHECK_PASSED))
    {
      cJSON *j_item = cJSON_CreateObject();
      tunnels[i]->params.printJSON(j_item);
      tunnels[i]->state.flags |= TunnelState::TF_SAVED_IN_CONFIG;
      cJSON_AddItemToArray(j_tunnels, j_item);
    }
  }
}

