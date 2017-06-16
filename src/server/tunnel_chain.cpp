/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "tunnel.h"
#include "../lib/sys_util.h"
#include "mgr_server.h"

//---------------------------------------------------------------------------
void Tunnel::cmd_tun_create_reply_received(const QByteArray &req_data)
{
  if (req_data.length() < (int)(sizeof(MgrPacket_TunnelCreateReply)))
  {
    mgrconn_out->log(LOG_DBG1, QString(": CMD_TUN_CREATE_REPLY packet too short"));
    mgrconn_out->abort();
    return;
  }
  MgrPacket_TunnelCreateReply *packet = (MgrPacket_TunnelCreateReply *)req_data.data();
  if (req_data.length() < (int)sizeof(MgrPacket_TunnelCreateReply)+packet->error_len)
  {
    mgrconn_out->log(LOG_DBG1, QString(": CMD_TUN_CREATE_REPLY packet too short"));
    mgrconn_out->abort();
    return;
  }
  QString error_str;
  if (packet->error_len > 0)
    error_str = QString::fromUtf8(req_data.mid(sizeof(MgrPacket_TunnelCreateReply),packet->error_len));
  if (packet->orig_tunnel_id == params.id)
  {
    if (packet->res_code == TunnelState::RES_CODE_OK)
    {
      log(LOG_DBG2, QString(": next tunserver has reported that tunnel is established"));
      if (timer_failure_tolerance->isActive() && (mgrconn_in || (params.flags & TunnelParameters::FL_MASTER_TUNSERVER)))
      {
        log(LOG_DBG4, QString(": stopping failure tolerance timer"));
        timer_failure_tolerance->stop();
        mgrconn_out->sendPacket(CMD_TUN_CHAIN_CHECK);
      }

      params.next_id = packet->tunnel_id;
      if (params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE && (params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
      {
        if ((params.flags & TunnelParameters::FL_PERMANENT_TUNNEL))
        {
          if (!bind_start())
          {
            state.flags |= TunnelState::TF_STOPPING;
            if (mgrconn_in)
            {
              connect(mgrconn_in, SIGNAL(output_buffer_empty()), this, SLOT(stop()));
              MgrPacket_TunnelCreateReply rep_packet;
              rep_packet.orig_tunnel_id = params.orig_id;
              rep_packet.tunnel_id = params.id;
              rep_packet.res_code = TunnelState::RES_CODE_BIND_ERROR;
              rep_packet.error_len = state.last_error_str.toUtf8().length();
              QByteArray rep_packet_data;
              rep_packet_data.append((const char *)&rep_packet, sizeof(MgrPacket_TunnelCreateReply));
              rep_packet_data.append((const char *)state.last_error_str.toUtf8(), state.last_error_str.toUtf8().length());
              mgrconn_in->sendPacket(CMD_TUN_CREATE_REPLY, rep_packet_data);
            }
            else
              stop();
            return;
          }
        }
      }
      else if (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL)
        state.flags |= TunnelState::TF_BINDING;
      if (mgrconn_in && !(state.flags & TunnelState::TF_CHECK_PASSED))
      {
        MgrPacket_TunnelCreateReply rep_packet;
        rep_packet.orig_tunnel_id = params.orig_id;
        rep_packet.tunnel_id = params.id;
        rep_packet.res_code = TunnelState::RES_CODE_OK;
        QByteArray rep_packet_data;
        rep_packet_data.append((const char *)&rep_packet, sizeof(MgrPacket_TunnelCreateReply));
        mgrconn_in->sendPacket(CMD_TUN_CREATE_REPLY, rep_packet_data);
      }
      state.flags |= TunnelState::TF_MGRCONN_OUT_CONNECTED;
      state.flags |= TunnelState::TF_CHECK_PASSED;
      state.last_error_code = TunnelState::RES_CODE_OK;
      state.last_error_str.clear();
      if (params.flags & TunnelParameters::FL_MASTER_TUNSERVER)
      {
        if ((params.flags & TunnelParameters::FL_SAVE_IN_CONFIG_PERMANENTLY) &&
            !(state.flags & TunnelState::TF_SAVED_IN_CONFIG))
        {
          this->log(LOG_DBG2, QString(": saving tunnel in configuration file"));
          mgrServer->config_save(&mgrServer->params);
        }
        if (params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE &&
            in_conn_list.isEmpty() && !(params.flags & TunnelParameters::FL_PERMANENT_TUNNEL))
        {
          state.flags |= TunnelState::TF_IDLE;
          mgrconn_out->timer_idle->setInterval(params.idle_timeout);
          mgrconn_out->log(LOG_DBG1, QString(": starting idle timer (%1 ms)").arg(params.idle_timeout));
          mgrconn_out->timer_idle->start();
        }
      }
    }
    else
    {
      state.last_error_code = packet->res_code;
      state.last_error_str = error_str;
      this->log(LOG_NORMAL, QString(" creation failed: ")+error_str);
      if (mgrconn_in)
      {
        MgrPacket_TunnelCreateReply rep_packet;
        rep_packet.orig_tunnel_id = params.orig_id;
        rep_packet.tunnel_id = params.id;
        rep_packet.res_code = packet->res_code;
        rep_packet.error_len = error_str.toUtf8().length();
        QByteArray rep_packet_data;
        rep_packet_data.reserve(sizeof(MgrPacket_TunnelCreateReply)+error_str.toUtf8().length());
        rep_packet_data.append((const char *)&rep_packet, sizeof(MgrPacket_TunnelCreateReply));
        rep_packet_data.append(error_str.toUtf8());
        mgrconn_in->sendPacket(CMD_TUN_CREATE_REPLY, rep_packet_data);
      }
      if (!(this->state.flags & TunnelState::TF_CHECK_PASSED) || !(this->params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
        this->stop();
    }
    emit state_changed();
  }
}

//---------------------------------------------------------------------------
void Tunnel::start_mgrconn_out()
{
  if (!mgrconn_out)
  {
    mgrconn_out = new MgrClientConnection(MgrClientConnection::OUTGOING);
    mgrconn_out->log_prefix = QString("Tunnel '%1' mgrconn_out: ").arg(params.name);
    connect(mgrconn_out, SIGNAL(state_changed(quint16)), this, SLOT(mgrconn_out_state_changed(quint16)));
    connect(mgrconn_out, SIGNAL(connection_error(QAbstractSocket::SocketError)), this, SLOT(mgrconn_out_connection_error(QAbstractSocket::SocketError)));
    connect(mgrconn_out, SIGNAL(packetReceived(MgrPacketCmd,QByteArray)), this, SLOT(mgrconn_out_packetReceived(MgrPacketCmd,QByteArray)));

    connect(mgrconn_out, SIGNAL(stat_bytesReceived(quint64)), this, SLOT(mgrconn_bytesReceived(quint64)));
    connect(mgrconn_out, SIGNAL(stat_bytesSent(quint64)), this, SLOT(mgrconn_bytesSent(quint64)));
    connect(mgrconn_out, SIGNAL(stat_bytesSentEncrypted(quint64)), this, SLOT(mgrconn_bytesSentEncrypted(quint64)));
  }
  mgrconn_out->params = params.tunservers.first();
  mgrconn_out->params.conn_type = ((params.flags & TunnelParameters::FL_PERMANENT_TUNNEL)
                             || params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL)
                                        ? MgrClientParameters::CONN_AUTO
                                        : MgrClientParameters::CONN_DEMAND;
  mgrconn_out->params.idle_timeout = params.idle_timeout;
  mgrconn_out->params.private_key_filename = mgrServer->params.private_key_filename;
  mgrconn_out->params.ssl_cert_filename = mgrServer->params.ssl_cert_filename;
  if (mgrconn_out->params.conn_type == MgrClientParameters::CONN_AUTO ||
     !(this->state.flags & TunnelState::TF_CHECK_PASSED))
    mgrconn_out->beginConnection();
}

//---------------------------------------------------------------------------
void Tunnel::mgrconn_out_state_changed(quint16 mgr_conn_state)
{
  if (mgr_conn_state == MgrClientConnection::MGR_CONNECTED)
  {
    // send tunnel creation request
    cJSON *j_tun_params = cJSON_CreateObject();
    TunnelParameters tun_params = this->params;

    // clear some tunnel parameters which are not neccessary for next tunserver
    tun_params.orig_id = 0;
    if (tun_params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE)
    {
      tun_params.bind_address.clear();
      tun_params.bind_port = 0;
    }
    else if (tun_params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL)
    {
      tun_params.remote_host.clear();
      tun_params.remote_port = 0;
    }
    tun_params.tunservers.removeFirst();
    tun_params.owner_user_id = 0;
    tun_params.owner_group_id = 0;
    tun_params.flags &= ~TunnelParameters::FL_MASTER_TUNSERVER;
    tun_params.flags &= ~TunnelParameters::FL_SAVE_IN_CONFIG_PERMANENTLY;

    tun_params.printJSON(j_tun_params);
    char *buffer = cJSON_PrintBuffered(j_tun_params, 1024*64, 1);
    cJSON_Delete(j_tun_params);

    mgrconn_out->sendPacket(CMD_TUN_CREATE, QByteArray(buffer));
    free(buffer);

    if (params.flags & TunnelParameters::FL_MASTER_TUNSERVER)
    {
      buffered_packets_rcv_count = 0;
      buffered_packets_rcv_total_len = 0;
    }
  }
  else if (mgr_conn_state == MgrClientConnection::MGR_ERROR || mgr_conn_state == MgrClientConnection::MGR_NONE)
  {
    bool was_connected = (state.flags & TunnelState::TF_MGRCONN_OUT_CONNECTED);
    state.flags &= ~TunnelState::TF_MGRCONN_OUT_CONNECTED;
    state.flags &= ~TunnelState::TF_MGRCONN_OUT_CLEAR_TO_SEND;
    state.flags &= ~TunnelState::TF_CHAIN_OK;
    if (was_connected && (state.flags & TunnelState::TF_IDLE))
    {
      buffered_packets_list.clear();
      buffered_packets_id_list.clear();
      buffered_packets_total_len = 0;
      buffered_packets_rcv_count = 0;
      buffered_packets_rcv_total_len = 0;
      t_last_buffered_packet_rcv = QTime();
      t_last_buffered_packet_ack_rcv = QTime();

      seq_packet_id = 1;
      expected_packet_id = 1;
      last_rcv_packet_id = 0;
    }

    state.stats.chain_error_count++;
    chain_heartbeat_rep_received = true;
    state.latency_ms = -1;
    emit state_changed();

    if (was_connected && mgrconn_in && !(params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
      mgrconn_in->sendPacket(CMD_TUN_CHAIN_BROKEN);

    if (params.failure_tolerance_timeout > 0 &&
        (state.flags & TunnelState::TF_CHECK_PASSED) &&
        !(state.flags & TunnelState::TF_STOPPING) &&
        !(state.flags & TunnelState::TF_IDLE))
    {
      if (!timer_failure_tolerance->isActive() && was_connected)
      {
        log(LOG_DBG4, QString(": mgrconn_out disconnected - starting failure tolerance timer (%1 ms)").arg(params.failure_tolerance_timeout));
        timer_failure_tolerance->setInterval(params.failure_tolerance_timeout);
        timer_failure_tolerance->start();
      }
    }
    else
    {
      mgrconn_out_after_disconnected();
    }
  }
}

//---------------------------------------------------------------------------
void Tunnel::mgrconn_in_after_disconnected()
{
  if (!(params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
  {
    log(LOG_DBG2, QString(": removing tunnel due to disconnected incoming mgrconn"));
    this->stop();
    return;
  }
}

//---------------------------------------------------------------------------
void Tunnel::mgrconn_out_after_disconnected()
{
  if (!(params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
  {
    log(LOG_DBG2, QString(": removing tunnel due to disconnected outgoing mgrconn"));
    this->stop();
    return;
  }
  if (state.flags & TunnelState::TF_STOPPING)
  {
    this->stop();
    return;
  }
  if (params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE)
  {
    if (!(params.flags & TunnelParameters::FL_PERMANENT_TUNNEL))
    {
      close_incoming_connections();
      emit state_changed();
    }
    else
      bind_stop();
  }
}

//---------------------------------------------------------------------------
void Tunnel::mgrconn_in_disconnected()
{
  if (!mgrconn_in)
    return;
  log(LOG_DBG2, QString(": incoming mgrconn disconnected"));
  if (mgrconn_in->closing_by_cmd_close)
  {
    state.flags |= TunnelState::TF_STOPPING;
    if (mgrconn_out && (state.flags & TunnelState::TF_MGRCONN_OUT_CONNECTED))
      mgrconn_out->sendPacket(CMD_CLOSE);
  }
  mgrconn_in = NULL;
  state.flags &= ~TunnelState::TF_MGRCONN_IN_CLEAR_TO_SEND;
  state.flags &= ~TunnelState::TF_MGRCONN_IN_CONNECTED;
  emit state_changed();
  if ((params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
    return;

  if (!(state.flags & TunnelState::TF_IDLE))
    state.stats.chain_error_count++;
  state.flags &= ~TunnelState::TF_CHAIN_OK;
  emit state_changed();

  if (mgrconn_out && (state.flags & TunnelState::TF_MGRCONN_OUT_CONNECTED))
    mgrconn_out->sendPacket(CMD_TUN_CHAIN_BROKEN);
  if (params.failure_tolerance_timeout > 0 && (state.flags & TunnelState::TF_CHECK_PASSED) && !(state.flags & TunnelState::TF_STOPPING))
  {
    if (!timer_failure_tolerance->isActive())
    {
      log(LOG_DBG4, QString(": mgrconn_in disconnected - starting failure tolerance timer (%1 ms)").arg(params.failure_tolerance_timeout));
      timer_failure_tolerance->setInterval(params.failure_tolerance_timeout);
      timer_failure_tolerance->start();
    }
  }
  else
  {
    mgrconn_in_after_disconnected();
    return;
  }
}

//---------------------------------------------------------------------------
void Tunnel::mgrconn_in_restored()
{
  log(LOG_DBG2, QString(": incoming mgrconn restored"));

  if (params.tunservers.isEmpty())
  {
    connect(mgrconn_in, SIGNAL(stat_bytesReceived(quint64)), this, SLOT(mgrconn_bytesReceived(quint64)));
    connect(mgrconn_in, SIGNAL(stat_bytesSent(quint64)), this, SLOT(mgrconn_bytesSent(quint64)));
    connect(mgrconn_in, SIGNAL(stat_bytesSentEncrypted(quint64)), this, SLOT(mgrconn_bytesSentEncrypted(quint64)));
  }

  state.flags |= TunnelState::TF_MGRCONN_IN_CLEAR_TO_SEND;
  state.flags |= TunnelState::TF_MGRCONN_IN_CONNECTED;
  emit state_changed();
  if (timer_failure_tolerance->isActive() &&
      (params.tunservers.isEmpty() || (mgrconn_out && (state.flags & TunnelState::TF_MGRCONN_OUT_CONNECTED))))
  {
    log(LOG_DBG4, QString(": stopping failure tolerance timer"));
    timer_failure_tolerance->stop();
  }
  if (!(params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
  {
    MgrPacket_TunnelCreateReply rep_packet;
    rep_packet.orig_tunnel_id = params.orig_id;
    rep_packet.tunnel_id = params.id;
    rep_packet.res_code = TunnelState::RES_CODE_OK;
    QByteArray rep_packet_data;
    rep_packet_data.append((const char *)&rep_packet, sizeof(MgrPacket_TunnelCreateReply));
    mgrconn_in->sendPacket(CMD_TUN_CREATE_REPLY, rep_packet_data);
  }
}

//---------------------------------------------------------------------------
void Tunnel::mgrconn_out_connection_error(QAbstractSocket::SocketError error)
{
  QString error_str = QString("Failed to connect to %1:%2 from %3: ").arg(mgrconn_out->params.host).arg(mgrconn_out->params.port).arg(SysUtil::machine_name);
  switch(error)
  {
    case QAbstractSocket::ConnectionRefusedError:
      error_str += QString("Connection refused");
      break;
    case QAbstractSocket::RemoteHostClosedError:
      error_str += QString("Server closed the connection");
      break;
    case QAbstractSocket::HostNotFoundError:
      error_str += QString("Host not found");
      break;
    case QAbstractSocket::SocketAccessError:
      error_str += QString("Permission denied");
      break;
    case QAbstractSocket::SocketResourceError:
      error_str += QString("Out of resources");
      break;
    case QAbstractSocket::SocketTimeoutError:
      error_str += QString("Timed out");
      break;
    case QAbstractSocket::DatagramTooLargeError:
      error_str += QString("Packet too large");
      break;
    case QAbstractSocket::NetworkError:
      error_str += QString("Network error");
      break;
    case QAbstractSocket::AddressInUseError:
      error_str += QString("Address in use");
      break;
    case QAbstractSocket::SocketAddressNotAvailableError:
      error_str += QString("Address not available");
      break;
    case QAbstractSocket::UnsupportedSocketOperationError:
      error_str += QString("No IPv6 support");
      break;
    case QAbstractSocket::ProxyAuthenticationRequiredError:
      error_str += QString("Authentication failed");
      break;
    case QAbstractSocket::SslHandshakeFailedError:
      error_str += QString("SSL Handshake failed");
      break;
    case QAbstractSocket::ProxyConnectionClosedError:
      error_str += QString("Server certificate is invalid");
      break;
    case QAbstractSocket::ProxyProtocolError:
      error_str += QString("Protocol error");
      break;
    default:
      error_str += QString("Error %1").arg((int)error);
      break;
  }

  if (!(state.flags & TunnelState::TF_IDLE) || error != QAbstractSocket::RemoteHostClosedError)
  {
    state.last_error_code = TunnelState::RES_CODE_MGRCONN_FAILED;
    state.last_error_str = error_str;
    emit state_changed();
  }
  if (!(this->state.flags & TunnelState::TF_CHECK_PASSED))
  {
    if (mgrconn_in)
    {
      MgrPacket_TunnelCreateReply rep_packet;
      rep_packet.orig_tunnel_id = params.orig_id;
      rep_packet.tunnel_id = params.id;
      rep_packet.res_code = TunnelState::RES_CODE_MGRCONN_FAILED;
      rep_packet.error_len = error_str.toUtf8().length();
      QByteArray rep_packet_data;
      rep_packet_data.reserve(sizeof(MgrPacket_TunnelCreateReply)+error_str.toUtf8().length());
      rep_packet_data.append((const char *)&rep_packet, sizeof(MgrPacket_TunnelCreateReply));
      rep_packet_data.append(error_str.toUtf8());
      mgrconn_in->sendPacket(CMD_TUN_CREATE_REPLY, rep_packet_data);
    }
    this->stop();
  }
}

//---------------------------------------------------------------------------
bool Tunnel::forward_packet(MgrClientConnection *conn, MgrPacketCmd cmd, const QByteArray &data)
{
  if (conn == mgrconn_in && !params.tunservers.isEmpty())
  {
    if (mgrconn_out)
    {
      mgrconn_out->sendPacket(cmd, data);
      if (cmd == CMD_TUN_CONN_IN_DATA)
        state.stats.data_bytes_rcv += data.length()-sizeof(TunnelConnPacketId)-sizeof(TunnelConnId);
      else if (cmd == CMD_TUN_CONN_OUT_DATA)
        state.stats.data_bytes_snd += data.length()-sizeof(TunnelConnPacketId)-sizeof(TunnelConnId);
    }
    return true;
  }
  else if (conn == mgrconn_out && !(params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
  {
    if (mgrconn_in)
    {
      mgrconn_in->sendPacket(cmd, data);
      if (cmd == CMD_TUN_CONN_IN_DATA)
        state.stats.data_bytes_rcv += data.length()-sizeof(TunnelConnPacketId)-sizeof(TunnelConnId);
      else if (cmd == CMD_TUN_CONN_OUT_DATA)
        state.stats.data_bytes_snd += data.length()-sizeof(TunnelConnPacketId)-sizeof(TunnelConnId);
    }
    return true;
  }
  return false;
}

//---------------------------------------------------------------------------
void Tunnel::cmd_tun_chain_broken(MgrClientConnection *conn)
{
  if (forward_packet(conn, CMD_TUN_CHAIN_BROKEN, QByteArray()))
    return;

  if (!(state.flags & TunnelState::TF_IDLE))
    state.stats.chain_error_count++;
  state.flags &= ~TunnelState::TF_CHAIN_OK;
  emit state_changed();
}

//---------------------------------------------------------------------------
void Tunnel::cmd_tun_chain_check(MgrClientConnection *conn)
{
  if (forward_packet(conn, CMD_TUN_CHAIN_CHECK, QByteArray()) || !mgrconn_in)
    return;

  mgrconn_in->sendPacket(CMD_TUN_CHAIN_RESTORED);
  if (last_rcv_packet_id == 0)
    mgrconn_in->sendPacket(CMD_TUN_BUFFER_RESEND_FROM, QByteArray((const char *)&last_rcv_packet_id, sizeof(TunnelConnPacketId)));
  else
    mgrconn_in->sendPacket(CMD_TUN_BUFFER_RESEND_FROM, QByteArray((const char *)&expected_packet_id, sizeof(TunnelConnPacketId)));
}

//---------------------------------------------------------------------------
void Tunnel::cmd_tun_chain_restored(MgrClientConnection *conn)
{
  if (forward_packet(conn, CMD_TUN_CHAIN_RESTORED, QByteArray()) || !mgrconn_out)
    return;

  chain_heartbeat_rep_received = true;
  state.flags |= TunnelState::TF_CHAIN_OK;
  emit state_changed();

  if (last_rcv_packet_id == 0)
    mgrconn_out->sendPacket(CMD_TUN_BUFFER_RESEND_FROM, QByteArray((const char *)&last_rcv_packet_id, sizeof(TunnelConnPacketId)));
  else
    mgrconn_out->sendPacket(CMD_TUN_BUFFER_RESEND_FROM, QByteArray((const char *)&expected_packet_id, sizeof(TunnelConnPacketId)));
}

//---------------------------------------------------------------------------
void Tunnel::mgrconn_out_authRepPacketReceived(const QByteArray &req_data)
{
  if (req_data.length() < (int)sizeof(MgrPacket_AuthRep))
  {
    mgrconn_out->log(LOG_DBG1, QString(": MgrPacket_AuthRep packet too short"));
    mgrconn_out->abort();
    return;
  }
  MgrPacket_AuthRep *rep = (MgrPacket_AuthRep *)req_data.data();
  if (req_data.length() < (int)sizeof(MgrPacket_AuthRep)+rep->server_hostname_len)
  {
    mgrconn_out->log(LOG_DBG1, QString(": MgrPacket_AuthRep packet too short"));
    mgrconn_out->abort();
    return;
  }

  mgrconn_out->peer_hostname = QString::fromUtf8(req_data.mid(sizeof(MgrPacket_AuthRep),rep->server_hostname_len));

  if (rep->auth_result != MgrPacket_AuthRep::RES_CODE_OK)
  {
    mgrconn_out->log(LOG_DBG1, QString(": user %1 login to %2 failed").arg(mgrconn_out->params.auth_username).arg(mgrconn_out->peer_hostname));
    mgrconn_out_state_changed(MgrClientConnection::MGR_ERROR);
    mgrconn_out_connection_error(QAbstractSocket::ProxyAuthenticationRequiredError);
    mgrconn_out->disconnectFromHost();
  }
  else
  {
    mgrconn_out->socket_startOperational();
    mgrconn_out->log(LOG_DBG1, QString(": user %1 logged in to %2").arg(mgrconn_out->params.auth_username).arg(mgrconn_out->peer_hostname));
  }
}

//---------------------------------------------------------------------------
void Tunnel::failure_tolerance_timedout()
{
  log(LOG_DBG4, QString(": Failure tolerance timeout"));
  if (!mgrconn_in && !(params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
  {
    mgrconn_in_after_disconnected();
  }
  if (!(state.flags & TunnelState::TF_MGRCONN_OUT_CONNECTED) && !params.tunservers.isEmpty())
  {
    mgrconn_out_after_disconnected();
  }
}

//---------------------------------------------------------------------------
void Tunnel::mgrconn_bytesReceived(quint64 bytes)
{
  state.stats.bytes_rcv += bytes;
}

//---------------------------------------------------------------------------
void Tunnel::mgrconn_bytesSent(quint64 bytes)
{
  state.stats.bytes_snd += bytes;
}

//---------------------------------------------------------------------------
void Tunnel::mgrconn_bytesSentEncrypted(quint64 encrypted_bytes)
{
  state.stats.bytes_snd_encrypted += encrypted_bytes;
}

//---------------------------------------------------------------------------
// tunnel heartbeat request/reply received
void Tunnel::cmd_tun_heartbeat(MgrPacketCmd cmd, MgrClientConnection *conn)
{
  if (forward_packet(conn, cmd, QByteArray()))
  {
    if (cmd == CMD_TUN_CHAIN_HEARTBEAT_REQ)
    {
      t_last_chain_heartbeat_req_sent.restart();
    }
    else if (cmd == CMD_TUN_CHAIN_HEARTBEAT_REP)
    {
      chain_heartbeat_rep_received = true;
      state.latency_ms = qAbs(t_last_chain_heartbeat_req_sent.elapsed());
    }
    return;
  }

  if (cmd == CMD_TUN_CHAIN_HEARTBEAT_REQ)
  {
    conn->sendPacket(CMD_TUN_CHAIN_HEARTBEAT_REP);
  }
  else if (cmd == CMD_TUN_CHAIN_HEARTBEAT_REP)
  {
    chain_heartbeat_rep_received = true;
    int old_latency_ms = state.latency_ms;
    state.latency_ms = qAbs(t_last_chain_heartbeat_req_sent.elapsed());
    if (old_latency_ms < 0)
      emit state_changed();
  }
}

//---------------------------------------------------------------------------
// send tunnel heartbeat request if possible
void Tunnel::chain_heartbeat_timeout()
{
  if (!(params.flags & TunnelParameters::FL_MASTER_TUNSERVER) ||
      !(state.flags & TunnelState::TF_CHAIN_OK) ||
      !(state.flags & TunnelState::TF_CHECK_PASSED) ||
      !(state.flags & TunnelState::TF_STARTED) ||
      (state.flags & TunnelState::TF_STOPPING))
    return;

  // only send tunnel chain heartbeats if tunnel seems to be more or less idle
  if (mgrconn_out && (state.flags & TunnelState::TF_MGRCONN_OUT_CONNECTED) &&
      mgrconn_out->output_buffer.length()+mgrconn_out->bytesToWrite() < 1024*64 &&
      (!t_last_buffered_packet_rcv.isValid() || qAbs(t_last_buffered_packet_rcv.elapsed()) > 100))
  {
    t_last_chain_heartbeat_req_sent.restart();
    chain_heartbeat_rep_received = false;
    mgrconn_out->sendPacket(CMD_TUN_CHAIN_HEARTBEAT_REQ);
  }
}
