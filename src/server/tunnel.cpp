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
void Tunnel::start()
{
  if (state.flags & TunnelState::TF_STARTED)
    return;

  state.flags &= ~TunnelState::TF_STOPPING;
  state.flags |= TunnelState::TF_STARTED;
  log(LOG_DBG1, QString(": starting"));
  state.stats = TunnelStatistics();

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

  params.next_id = 0;
  if (next_udp_port == 0)
    next_udp_port = params.udp_port_range_from;

  if (params.app_protocol == TunnelParameters::UDP &&
      ((params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE && (params.tunservers.isEmpty())) ||
       (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL && (params.flags & TunnelParameters::FL_MASTER_TUNSERVER))))
  {
    udp_remote_addr = QHostAddress(params.remote_host);
    if (udp_remote_addr.isNull() && !udp_remote_addr_lookup_in_progress)
    {
      udp_remote_addr_lookup_in_progress = true;
      this->log(LOG_DBG1, QString(": starting host '%1' lookup").arg(params.remote_host));
      udp_remote_addr_lookup_id = QHostInfo::lookupHost(params.remote_host, this, SLOT(udp_hostLookupFinished(QHostInfo)));
    }
  }

  if (params.tunservers.isEmpty())
  {
    // this is the final tunserver in chain
    if (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL)
    {
      if (!bind_start())
      {
        state.flags |= TunnelState::TF_STOPPING;
        if (mgrconn_in)
        {
          MgrPacket_TunnelCreateReply rep_packet;
          rep_packet.orig_tunnel_id = params.orig_id;
          rep_packet.tunnel_id = params.id;
          rep_packet.res_code = TunnelState::RES_CODE_BIND_ERROR;
          rep_packet.error_len = state.last_error_str.toUtf8().length();
          QByteArray rep_packet_data;
          rep_packet_data.append((const char *)&rep_packet, sizeof(MgrPacket_TunnelCreateReply));
          rep_packet_data.append((const char *)state.last_error_str.toUtf8(), state.last_error_str.toUtf8().length());
          mgrconn_in->sendPacket(CMD_TUN_CREATE_REPLY, rep_packet_data);
          mgrconn_in->sendPacket(CMD_CLOSE);
        }
        else
          stop();
        return;
      }
    }
    state.flags |= TunnelState::TF_CHECK_PASSED;
    state.last_error_code = TunnelState::RES_CODE_OK;
    state.last_error_str.clear();
    if (mgrconn_in)
    {
      MgrPacket_TunnelCreateReply rep_packet;
      rep_packet.orig_tunnel_id = params.orig_id;
      rep_packet.tunnel_id = params.id;
      rep_packet.res_code = TunnelState::RES_CODE_OK;
      QByteArray rep_packet_data;
      rep_packet_data.append((const char *)&rep_packet, sizeof(MgrPacket_TunnelCreateReply));
      mgrconn_in->sendPacket(CMD_TUN_CREATE_REPLY, rep_packet_data);
      mgrconn_in->sendPacket(CMD_TUN_BUFFER_RESET);
      connect(mgrconn_in, SIGNAL(stat_bytesReceived(quint64)), this, SLOT(mgrconn_bytesReceived(quint64)));
      connect(mgrconn_in, SIGNAL(stat_bytesSent(quint64)), this, SLOT(mgrconn_bytesSent(quint64)));
      connect(mgrconn_in, SIGNAL(stat_bytesSentEncrypted(quint64)), this, SLOT(mgrconn_bytesSentEncrypted(quint64)));
    }
    this->log(LOG_DBG2, QString(": tunnel is established"));
  }
  else
  {
    start_mgrconn_out();

    if (params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE && (params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
    {
      if (!(params.flags & TunnelParameters::FL_PERMANENT_TUNNEL))
      {
        if (!bind_start())
        {
          state.flags |= TunnelState::TF_STOPPING;
          if (mgrconn_in)
          {
            MgrPacket_TunnelCreateReply rep_packet;
            rep_packet.orig_tunnel_id = params.orig_id;
            rep_packet.tunnel_id = params.id;
            rep_packet.res_code = TunnelState::RES_CODE_BIND_ERROR;
            rep_packet.error_len = state.last_error_str.toUtf8().length();
            QByteArray rep_packet_data;
            rep_packet_data.append((const char *)&rep_packet, sizeof(MgrPacket_TunnelCreateReply));
            rep_packet_data.append((const char *)state.last_error_str.toUtf8(), state.last_error_str.toUtf8().length());
            mgrconn_in->sendPacket(CMD_TUN_CREATE_REPLY, rep_packet_data);
            mgrconn_in->sendPacket(CMD_CLOSE);
          }
          else
            stop();
          return;
        }
        else
          state.flags |= TunnelState::TF_IDLE;
      }
    }
  }
  emit started();
  emit state_changed();

  if (params.heartbeat_interval > 0)
  {
    timer_chain_heartbeat->setInterval(params.heartbeat_interval);
    timer_chain_heartbeat->start();
  }
}

//---------------------------------------------------------------------------
void Tunnel::setNewParams(TunnelParameters *new_params)
{
  TunnelParameters old_params = params;
  params = *new_params;
  params.orig_id = old_params.orig_id;
  params.id = old_params.id;
  params.next_id = old_params.next_id;
  params.owner_user_id = old_params.owner_user_id;
  params.owner_group_id = old_params.owner_group_id;

  if (mgrconn_out)
    mgrconn_out->log_prefix = QString("Tunnel '%1' mgrconn_out: ").arg(params.name);
  if (mgrconn_in && !(params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
    mgrconn_in->log_prefix = QString("Tunnel '%1' mgrconn_in: ").arg(params.name);

  QHashIterator<TunnelConnId, TunnelConn *> in_conn_list_iterator(in_conn_list);
  while (in_conn_list_iterator.hasNext())
  {
    in_conn_list_iterator.next();
    in_conn_list_iterator.value()->tun_params = params;
  }

  QHashIterator<TunnelConnId, TunnelConn *> out_conn_list_iterator(out_conn_list);
  while (out_conn_list_iterator.hasNext())
  {
    out_conn_list_iterator.next();
    out_conn_list_iterator.value()->tun_params = params;
  }

  if (params != old_params)
  {
    state.flags &= ~TunnelState::TF_SAVED_IN_CONFIG;
//    state.flags &= ~TunnelState::TF_CHECK_PASSED;
  }

  if (params.udp_port_range_from != old_params.udp_port_range_from ||
      params.udp_port_range_till != old_params.udp_port_range_till)
    next_udp_port = params.udp_port_range_from;
  if (params.remote_host != old_params.remote_host)
    udp_remote_addr = QHostAddress();

  if (params.needRestart(old_params))
  {
    log(LOG_DBG1, QString(": stopping and restarting tunnel due to changed tunnel parameters"));
    state.flags |= TunnelState::TF_STOPPING;
    restart_after_stop = true;
    if (mgrconn_out && (state.flags & TunnelState::TF_MGRCONN_OUT_CONNECTED) && mgrconn_out->output_buffer.length()+mgrconn_out->bytesToWrite() < 64*1024)
    {
      mgrconn_out->sendPacket(CMD_TUN_STOP);
      mgrconn_out->sendPacket(CMD_CLOSE);
    }
    else
      stop();
  }
}

//---------------------------------------------------------------------------
void Tunnel::log(LogPriority prio, const QString &text)
{
  prc_log(prio, QString("Tunnel '%1'").arg(params.name)+text);
}

//---------------------------------------------------------------------------
void Tunnel::stop()
{
  if (!(state.flags & TunnelState::TF_STARTED))
    return;
  if (timer_failure_tolerance->isActive())
    timer_failure_tolerance->stop();
  if (timer_chain_heartbeat->isActive())
    timer_chain_heartbeat->stop();
  if (udp_remote_addr_lookup_in_progress)
  {
    QHostInfo::abortHostLookup(udp_remote_addr_lookup_id);
    udp_remote_addr = QHostAddress();
    udp_remote_addr_lookup_in_progress = false;
  }
  bind_stop();
  state.flags &= ~TunnelState::TF_STARTED;
  state.flags &= ~TunnelState::TF_STOPPING;
  params.next_id = 0;
  if (mgrconn_out)
  {
    // disable auto-reconnect
    mgrconn_out->params.conn_type = MgrClientParameters::CONN_DEMAND;
    // disconnect or abort if connected
    if (mgrconn_out->state() != QAbstractSocket::UnconnectedState && mgrconn_out->state() != QAbstractSocket::ClosingState)
      mgrconn_out->disconnectFromHost();
    else
      mgrconn_out->abort();
    disconnect(mgrconn_out, 0, this, 0);
    mgrconn_out->deleteLater();
    mgrconn_out = NULL;
  }
  if (!(params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
  {
    if (mgrconn_in)
    {
      // disconnect or abort if connected
      if (mgrconn_in->state() != QAbstractSocket::UnconnectedState && mgrconn_in->state() != QAbstractSocket::ClosingState)
        mgrconn_in->disconnectFromHost();
      else
        mgrconn_in->abort();
      disconnect(mgrconn_in, 0, this, 0);
      mgrconn_in->deleteLater();
      mgrconn_in = NULL;
    }
  }
  this->log(LOG_DBG1, QString(": stopped"));
  buffered_packets_list.clear();
  buffered_packets_id_list.clear();
  buffered_packets_total_len = 0;
  buffered_packets_rcv_count = 0;
  buffered_packets_rcv_total_len = 0;
  in_conn_info_list.clear();
  emit stopped();
  emit state_changed();
  if (restart_after_stop)
  {
    QTimer::singleShot(0, this, SLOT(start()));
    restart_after_stop = false;
  }
}

//---------------------------------------------------------------------------
void Tunnel::mgrconn_out_packetReceived(MgrPacketCmd cmd, const QByteArray &data)
{
  switch (cmd)
  {
    case CMD_AUTH_REP:
      mgrconn_out_authRepPacketReceived(data);
      break;
    case CMD_TUN_CREATE_REPLY:
      cmd_tun_create_reply_received(data);
      break;
    case CMD_TUN_BUFFER_RESET:
      cmd_tun_buffer_reset();
      break;
    case CMD_TUN_CHAIN_BROKEN:
      cmd_tun_chain_broken(mgrconn_out);
      break;
    case CMD_TUN_CHAIN_RESTORED:
      cmd_tun_chain_restored(mgrconn_out);
      break;
    case CMD_TUN_BUFFER_ACK:
      cmd_tun_buffer_ack_received(mgrconn_out, data);
      break;
    case CMD_TUN_BUFFER_RESEND_FROM:
      cmd_tun_buffer_resend_from(mgrconn_out, data);
      break;
    case CMD_TUN_CHAIN_HEARTBEAT_REQ:
    case CMD_TUN_CHAIN_HEARTBEAT_REP:
      cmd_tun_heartbeat(cmd, mgrconn_out);
      break;
    case CMD_TUN_CONN_OUT_NEW:
    case CMD_TUN_CONN_OUT_DROP:
    case CMD_TUN_CONN_OUT_CONNECTED:
    case CMD_TUN_CONN_IN_DROP:
    case CMD_TUN_CONN_IN_DATA:
    case CMD_TUN_CONN_OUT_DATA:
    {
      if (forward_packet(mgrconn_out, cmd, data))
        break;
      int data_header_len = sizeof(TunnelConnPacketId)+sizeof(TunnelConnId);
      if (data.length() < data_header_len)
      {
        mgrconn_out->log(LOG_DBG1, QString(": CMD_TUN_CONN_... packet too short"));
        mgrconn_out->abort();
        return;
      }
      if (!t_last_buffered_packet_ack_rcv.isValid())
        t_last_buffered_packet_ack_rcv.start();
      TunnelConnPacketId packet_id = *((TunnelConnPacketId *)data.data());
      TunnelConnId conn_id = *((TunnelConnId *)(data.data()+sizeof(TunnelConnPacketId)));
      if (!buffered_packet_received(packet_id, data.length()))
        return;
      switch(cmd)
      {
        case CMD_TUN_CONN_OUT_NEW:
        {
          cmd_conn_out_new(conn_id);
          break;
        }
        case CMD_TUN_CONN_OUT_DROP:
        {
          cmd_conn_out_drop(conn_id, data.mid(data_header_len));
          break;
        }
        case CMD_TUN_CONN_OUT_CONNECTED:
        {
          cmd_conn_out_connected(conn_id, data.mid(data_header_len));
          break;
        }
        case CMD_TUN_CONN_IN_DROP:
        {
          cmd_conn_in_drop(conn_id, data.mid(data_header_len));
          break;
        }
        case CMD_TUN_CONN_IN_DATA:
        case CMD_TUN_CONN_OUT_DATA:
        {
          cmd_conn_data(cmd == CMD_TUN_CONN_IN_DATA ? TunnelConn::INCOMING : TunnelConn::OUTGOING,
                        conn_id, data.mid(data_header_len));
          break;
        }
      }
      break;
    }
    default:
      mgrconn_out->log(LOG_DBG1, QString(": Unknown packet cmd %1 - dropping connection").arg(cmd));
      mgrconn_out->abort();
      break;
  }
}

//---------------------------------------------------------------------------
void Tunnel::udp_hostLookupFinished(const QHostInfo &hostInfo)
{
  udp_remote_addr_lookup_in_progress = false;
  if (!hostInfo.addresses().isEmpty())
    udp_remote_addr = hostInfo.addresses().first();
  if (udp_remote_addr.isNull())
  {
    this->log(LOG_DBG1, QString(": host '%1' lookup failed: ").arg(hostInfo.hostName())+hostInfo.errorString());
    close_outgoing_connections();
    return;
  }
  this->log(LOG_DBG1, QString(": host '%1' lookup finished, IP address is %2").arg(hostInfo.hostName()).arg(udp_remote_addr.toString()));
  QHashIterator<TunnelConnId, TunnelUdpConn *> i_udp_conn(udp_conn_list_by_id);
  while (i_udp_conn.hasNext())
  {
    i_udp_conn.next();
    TunnelUdpConn *conn = i_udp_conn.value();
    if (!conn->output_buffer.isEmpty() && conn->udp_sock)
    {
      for (int i=0; i < conn->output_buffer.count(); i++)
      {
        conn->udp_sock->writeDatagram(conn->output_buffer[i], udp_remote_addr, params.remote_port);
        log(LOG_DBG4, QString(", conn %1: sending datagram to %2:%3, len=%4").arg(conn->id).arg(udp_remote_addr.toString()).arg(params.remote_port).arg(conn->output_buffer[i].length()));
      }
    }
    conn->output_buffer.clear();
  }
}

