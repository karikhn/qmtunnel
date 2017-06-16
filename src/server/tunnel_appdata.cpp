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
// returns true if packet_id is correct (as expected) and processing should proceed
bool Tunnel::buffered_packet_received(TunnelConnPacketId packet_id, quint32 packet_len)
{
  if (!params.tunservers.isEmpty() && !(params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
    return true;
  if (packet_id != expected_packet_id)
  {
    if (buffered_packets_rcv_count > 0)
      buffered_packets_send_ack();

    if ((packet_id > expected_packet_id && qAbs((int)packet_id-(int)expected_packet_id) <= BUFFERED_PACKETS_MAX_COUNT) ||
        (packet_id < expected_packet_id && qAbs((int)packet_id-(int)expected_packet_id) > BUFFERED_PACKETS_MAX_COUNT))
    {
      log(LOG_DBG3, QString(": got packet_id=%1 (expected %2) - sending CMD_TUN_BUFFER_RESEND_FROM, packet_id=%2").arg(packet_id).arg(expected_packet_id));
      QByteArray packet_resend_data;
      packet_resend_data.append((const char *)&expected_packet_id, sizeof(TunnelConnPacketId));
      if (params.tunservers.isEmpty() && mgrconn_in)
        mgrconn_in->sendPacket(CMD_TUN_BUFFER_RESEND_FROM, packet_resend_data);
      else if ((params.flags & TunnelParameters::FL_MASTER_TUNSERVER) && mgrconn_out)
        mgrconn_out->sendPacket(CMD_TUN_BUFFER_RESEND_FROM, packet_resend_data);
    }
    return false;
  }

  bool need_ack = !t_last_buffered_packet_rcv.isValid() ||
                  qAbs(t_last_buffered_packet_rcv.elapsed()) >= BUFFERED_PACKETS_TIMEOUT_BEFORE_ACK;

  t_last_buffered_packet_rcv.restart();

  last_rcv_packet_id = packet_id;
  expected_packet_id = packet_id+1;
  if (expected_packet_id == 0)
    expected_packet_id = 1;

  if (buffered_packets_rcv_count == 0 && !need_ack)
    timer_buffered_packets_ack->start();
  buffered_packets_rcv_count++;
  buffered_packets_rcv_total_len += packet_len;
  if (buffered_packets_rcv_count >= BUFFERED_PACKETS_MIN_COUNT_BEFORE_ACK ||
      buffered_packets_rcv_total_len >= BUFFERED_PACKETS_MIN_SIZE_BEFORE_ACK ||
      need_ack)
    buffered_packets_send_ack();
  return true;
}

//---------------------------------------------------------------------------
void Tunnel::buffered_packets_send_ack()
{
  if (timer_buffered_packets_ack->isActive())
    timer_buffered_packets_ack->stop();

  if (!(params.tunservers.isEmpty() && mgrconn_in) &&
      !((params.flags & TunnelParameters::FL_MASTER_TUNSERVER) && mgrconn_out))
    return;

  log(LOG_DBG4, QString(": %1 buffered packets received, now sending ack (last_rcv_id=%2)").arg(buffered_packets_rcv_count).arg(last_rcv_packet_id));
  QByteArray packet_data;
  packet_data.reserve(sizeof(TunnelConnPacketId)+sizeof(TunnelConnPacketCount));
  packet_data.append((const char *)&last_rcv_packet_id, sizeof(TunnelConnPacketId));
  packet_data.append((const char *)&buffered_packets_rcv_count, sizeof(TunnelConnPacketCount));
  if ((params.tunservers.isEmpty() && mgrconn_in && mgrconn_in->sendPacket(CMD_TUN_BUFFER_ACK, packet_data)) ||
      ((params.flags & TunnelParameters::FL_MASTER_TUNSERVER) && mgrconn_out && mgrconn_out->sendPacket(CMD_TUN_BUFFER_ACK, packet_data)))
  {
    buffered_packets_rcv_count = 0;
    buffered_packets_rcv_total_len = 0;
  }
}

//---------------------------------------------------------------------------
void Tunnel::cmd_tun_buffer_ack_received(MgrClientConnection *conn, const QByteArray &data)
{
  if (data.length() < (int)(sizeof(TunnelConnPacketId)+sizeof(TunnelConnPacketCount)))
  {
    conn->log(LOG_DBG1, QString(": CMD_TUN_BUFFER_ACK packet too short"));
    conn->abort();
    return;
  }

  if (forward_packet(conn, CMD_TUN_BUFFER_ACK, data))
    return;

  TunnelConnPacketId last_packet_id = *((TunnelConnPacketId *)data.data());
  TunnelConnPacketCount packet_count = *((TunnelConnPacketCount *)(data.data()+sizeof(TunnelConnPacketId)));

  TunnelConnPacketId first_packet_id = last_packet_id;
  for (TunnelConnPacketCount i=1; i < packet_count; i++)
  {
    first_packet_id--;
    if (first_packet_id == 0)
      first_packet_id--;
  }
  int first_packet_index = buffered_packets_id_list.indexOf(first_packet_id);
  if (first_packet_index < 0)
    return;
  unsigned int acked_packets_total_len = 0;
  for (TunnelConnPacketCount i=0; i < packet_count && first_packet_index < buffered_packets_id_list.count(); i++)
  {
    buffered_packets_total_len -= buffered_packets_list[first_packet_index].length();
    acked_packets_total_len += buffered_packets_list[first_packet_index].length();
    buffered_packets_id_list.removeAt(first_packet_index);
    buffered_packets_list.removeAt(first_packet_index);
  }

  // calculate optimal data packet size
  unsigned int transfer_time = qAbs(t_last_buffered_packet_ack_rcv.elapsed());
  if (transfer_time == 0)
    transfer_time = 1;
  quint32 old_data_packet_size = cur_data_packet_size;
  cur_data_packet_size = (acked_packets_total_len*BUFFERED_PACKET_OPTIMAL_TRANSFER_TIME)/transfer_time;
  if (cur_data_packet_size < BUFFERED_PACKET_MIN_SIZE)
    cur_data_packet_size = BUFFERED_PACKET_MIN_SIZE;
  if (params.max_data_packet_size > 0 && sizeof(TunnelConnId)+sizeof(TunnelConnPacketId)+cur_data_packet_size > params.max_data_packet_size)
    cur_data_packet_size = params.max_data_packet_size-sizeof(TunnelConnId)-sizeof(TunnelConnPacketId);
  if (cur_data_packet_size+sizeof(TunnelConnId)+sizeof(TunnelConnPacketId) > MGR_PACKET_MAX_LEN)
    cur_data_packet_size = MGR_PACKET_MAX_LEN-sizeof(TunnelConnId)-sizeof(TunnelConnPacketId);
  if (old_data_packet_size != cur_data_packet_size)
    log(LOG_DBG4, QString(": new calculated optimal data packet size = %1").arg(cur_data_packet_size));

  t_last_buffered_packet_ack_rcv.restart();
  log(LOG_DBG4, QString(": %1 buffered packets acknowledged and removed from buffer (%2 left in buffer)").arg(packet_count).arg(buffered_packets_id_list.count()));
}

//---------------------------------------------------------------------------
void Tunnel::cmd_tun_buffer_resend_from(MgrClientConnection *conn, const QByteArray &data)
{
  if (data.length() < (int)sizeof(TunnelConnPacketId))
  {
    conn->log(LOG_DBG1, QString(": CMD_TUN_BUFFER_RESEND_FROM packet too short"));
    conn->abort();
    return;
  }

  if (forward_packet(conn, CMD_TUN_BUFFER_RESEND_FROM, data))
    return;

  TunnelConnPacketId resend_from_packet_id = *((TunnelConnPacketId *)data.data());
  log(LOG_DBG4, QString(": received CMD_TUN_BUFFER_RESEND_FROM, packet_id=%1").arg(resend_from_packet_id));
  if (resend_from_packet_id == 0 && !buffered_packets_id_list.isEmpty())
    resend_from_packet_id = buffered_packets_id_list.first();

  MgrClientConnection *dest_conn = (params.flags & TunnelParameters::FL_MASTER_TUNSERVER) ? mgrconn_out : mgrconn_in;
  if (!dest_conn || (dest_conn == mgrconn_out && !(state.flags & TunnelState::TF_MGRCONN_OUT_CONNECTED)))
    return;

  if (dest_conn == mgrconn_in)
    state.flags |= TunnelState::TF_MGRCONN_IN_CLEAR_TO_SEND;
  else if (dest_conn == mgrconn_out)
    state.flags |= TunnelState::TF_MGRCONN_OUT_CLEAR_TO_SEND;

  int resend_from_packet_index = buffered_packets_id_list.indexOf(resend_from_packet_id);
  if (resend_from_packet_index < 0)
    return;
  for (int i=resend_from_packet_index; i < buffered_packets_id_list.count(); i++)
    dest_conn->output_buffer.append(buffered_packets_list[i]);
  dest_conn->sendOutputBuffer();
  int resent_packets_count = buffered_packets_id_list.count()-resend_from_packet_index;
  log(LOG_DBG4, QString(": %1 buffered packets resent due to resend request").arg(resent_packets_count));
}

//---------------------------------------------------------------------------
void Tunnel::cmd_tun_buffer_reset()
{
  if (forward_packet(mgrconn_out, CMD_TUN_BUFFER_RESET, QByteArray()))
    return;

  if (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL)
    close_outgoing_connections();

  if (params.flags & TunnelParameters::FL_MASTER_TUNSERVER)
    state.flags |= TunnelState::TF_MGRCONN_OUT_CLEAR_TO_SEND;
  state.flags |= TunnelState::TF_CHAIN_OK;
  chain_heartbeat_timeout();
  emit state_changed();

  if ((!buffered_packets_id_list.isEmpty() && buffered_packets_id_list.first() != 1) ||
           (params.flags & TunnelParameters::FL_PERMANENT_TUNNEL) ||
           params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL)
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
  else if (!buffered_packets_id_list.isEmpty())
  {
    for (int i=0; i < buffered_packets_id_list.count(); i++)
      mgrconn_out->output_buffer.append(buffered_packets_list[i]);
    mgrconn_out->sendOutputBuffer();
    int resent_packets_count = buffered_packets_id_list.count();
    log(LOG_DBG4, QString(": %1 buffered packets resent due to reset request").arg(resent_packets_count));
  }
}

//---------------------------------------------------------------------------
// command to send new connection data which has been received
void Tunnel::cmd_conn_data(TunnelConn::Direction direction, TunnelConnId conn_id, const QByteArray &data)
{
  if (direction == TunnelConn::INCOMING &&
     ((params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE && params.tunservers.isEmpty()) ||
      (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL && (params.flags & TunnelParameters::FL_MASTER_TUNSERVER))))
  {
    if (params.app_protocol == TunnelParameters::UDP)
    {
      TunnelUdpConn *conn = udp_conn_list_by_id.value(conn_id);
      if (conn && conn->udp_sock)
      {
        if (udp_remote_addr_lookup_in_progress)
          conn->output_buffer.append(data);
        else if (!udp_remote_addr.isNull())
        {
          qint64 snd_len = conn->udp_sock->writeDatagram(data, udp_remote_addr, params.remote_port);
          if (in_conn_info_list.contains(conn->id))
            in_conn_info_list[conn->id].bytes_snd += snd_len;
          log(LOG_DBG4, QString(", conn %1: sending datagram to %2:%3, len=%4").arg(conn_id).arg(udp_remote_addr.toString()).arg(params.remote_port).arg(data.length()));
        }
      }
      else
        log(LOG_DBG1, QString(": failed to find outgoing connection ID %1").arg(conn_id));
    }
    else
    {
      TunnelConn *conn = out_conn_list.value(conn_id);
      if (conn)
      {
        conn->output_buffer.append(data);
        conn->sendOutputBuffer();
      }
      else
        log(LOG_DBG1, QString(": failed to find outgoing connection ID %1").arg(conn_id));
    }
  }
  else if (direction == TunnelConn::OUTGOING &&
     ((params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE && (params.flags & TunnelParameters::FL_MASTER_TUNSERVER)) ||
      (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL && params.tunservers.isEmpty())))
  {
    if (params.app_protocol == TunnelParameters::UDP)
    {
      TunnelUdpConn *conn = udp_conn_list_by_id.value(conn_id);
      if (conn && bind_udpSocket)
      {
        qint64 snd_len = bind_udpSocket->writeDatagram(data, conn->remote_addr, conn->remote_port);
        if (in_conn_info_list.contains(conn->id))
          in_conn_info_list[conn->id].bytes_snd += snd_len;
        log(LOG_DBG4, QString(", conn %1: sending datagram to %2:%3, len=%4").arg(conn_id).arg(conn->remote_addr.toString()).arg(conn->remote_port).arg(data.length()));
      }
      else
        log(LOG_DBG1, QString(": failed to find incoming connection ID %1").arg(conn_id));
    }
    else
    {
      TunnelConn *conn = in_conn_list.value(conn_id);
      if (conn)
      {
        conn->output_buffer.append(data);
        conn->sendOutputBuffer();
      }
      else
        log(LOG_DBG1, QString(": failed to find incoming connection ID %1").arg(conn_id));
    }
  }
}

//---------------------------------------------------------------------------
bool Tunnel::queueOutPacket(MgrPacketCmd _cmd, TunnelConnId conn_id, TunnelConnPacketId packet_id, const QByteArray &_data)
{
  QByteArray packet_data;
  packet_data.reserve(sizeof(TunnelConnPacketId)+sizeof(TunnelConnId)+_data.length());
  packet_data.append((const char *)&packet_id, sizeof(TunnelConnPacketId));
  packet_data.append((const char *)&conn_id, sizeof(TunnelConnId));
  packet_data.append(_data);
  if (!(params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
  {
    if (!mgrconn_out || !(state.flags & TunnelState::TF_MGRCONN_OUT_CONNECTED))
      return false;
    return mgrconn_out->sendPacket(_cmd, packet_data);
  }
  MgrPacketLen len = packet_data.length();
  quint32 buf_len = buffered_packets_total_len;
  bool use_compression = true;
  if (mgrconn_out)
  {
    buf_len += mgrconn_out->output_buffer.length();
    use_compression = mgrconn_out->params.flags & MgrClientParameters::FL_USE_COMPRESSION;
  }
  if ((params.max_io_buffer_size > 0 && buf_len+len > params.max_io_buffer_size) ||
      buffered_packets_id_list.count()+1 > BUFFERED_PACKETS_MAX_COUNT)
  {
    log(LOG_DBG1, QString("mgrconn_out buffer overflow - closing/restarting tunnel"));
    buffered_packets_id_list.clear();
    buffered_packets_list.clear();
    buffered_packets_total_len = 0;
    QTimer::singleShot(0, this, SLOT(restart()));
    return false;
  }
  QByteArray packet_buffer;
  bool compressed = false;
  if (use_compression && len >= MGR_PACKET_MIN_LEN_FOR_COMPRESSION)
  {
    QByteArray compressed_packet_data = qCompress(packet_data);
    if (compressed_packet_data.length() < packet_data.length())
    {
      MgrPacketLen cmd = _cmd | MGR_PACKET_FLAG_COMPRESSED;
      len = compressed_packet_data.length();
      packet_buffer.reserve(sizeof(MgrPacketCmd)+sizeof(MgrPacketLen)+compressed_packet_data.length());
      packet_buffer.append(QByteArray((const char *)&cmd, sizeof(MgrPacketCmd)));
      packet_buffer.append(QByteArray((const char *)&len, sizeof(MgrPacketLen)));
      packet_buffer.append(compressed_packet_data);
      compressed = true;
    }
  }
  if (!compressed)
  {
    packet_buffer.reserve(sizeof(MgrPacketCmd)+sizeof(MgrPacketLen)+packet_data.length());
    packet_buffer.append(QByteArray((const char *)&_cmd, sizeof(MgrPacketCmd)));
    packet_buffer.append(QByteArray((const char *)&len, sizeof(MgrPacketLen)));
    packet_buffer.append(packet_data);
  }
  log(LOG_DBG4, QString(": queueing mgrconn_out packet cmd=%1, id=%2, conn_id=%3, len=%4").arg(mgrPacket_cmdString(_cmd)).arg(packet_id).arg(conn_id).arg(len));
  buffered_packets_id_list.append(packet_id);
  buffered_packets_list.append(packet_buffer);
  buffered_packets_total_len += packet_buffer.length();

  if (mgrconn_out && (state.flags & TunnelState::TF_MGRCONN_OUT_CONNECTED) && (state.flags & TunnelState::TF_MGRCONN_OUT_CLEAR_TO_SEND))
  {
    mgrconn_out->output_buffer.append(packet_buffer);
    mgrconn_out->sendOutputBuffer();
  }
  return true;
}

//---------------------------------------------------------------------------
void Tunnel::connection_dataReceived()
{
  TunnelConn *conn = qobject_cast<TunnelConn *>(sender());

  int pos = 0;
  QTime t;
  t.start();
  while (pos < conn->input_buffer.length())
  {
    int data_len = conn->input_buffer.length()-pos;
    if (data_len > (int)cur_data_packet_size)
      data_len = cur_data_packet_size;

    MgrPacketCmd _cmd = (conn->direction == TunnelConn::INCOMING) ? CMD_TUN_CONN_IN_DATA : CMD_TUN_CONN_OUT_DATA;
    if ((conn->direction == TunnelConn::INCOMING && params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE) ||
        (conn->direction == TunnelConn::OUTGOING && params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL))
      queueOutPacket(_cmd, conn->id, next_packet_id(), conn->input_buffer.mid(pos, data_len));
    else if ((conn->direction == TunnelConn::OUTGOING && params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE) ||
             (conn->direction == TunnelConn::INCOMING && params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL))
      queueInPacket(_cmd, conn->id, next_packet_id(), conn->input_buffer.mid(pos, data_len));

    state.stats.data_bytes_rcv += data_len;
    if (in_conn_info_list.contains(conn->id))
      in_conn_info_list[conn->id].bytes_rcv += data_len;
    pos += data_len;
  }
  conn->input_buffer.clear();
}

//---------------------------------------------------------------------------
void Tunnel::connection_bytesWritten(qint64 bytes)
{
  if (params.app_protocol != TunnelParameters::UDP)
  {
    TunnelConn *conn = qobject_cast<TunnelConn *>(sender());
    if (conn && in_conn_info_list.contains(conn->id))
      in_conn_info_list[conn->id].bytes_snd += bytes;
  }
  state.stats.data_bytes_snd += bytes;
}

//---------------------------------------------------------------------------
bool Tunnel::queueInPacket(MgrPacketCmd _cmd, TunnelConnId conn_id, TunnelConnPacketId packet_id, const QByteArray &_data)
{
  QByteArray packet_data;
  packet_data.reserve(sizeof(TunnelConnPacketId)+sizeof(TunnelConnId)+_data.length());
  packet_data.append((const char *)&packet_id, sizeof(TunnelConnPacketId));
  packet_data.append((const char *)&conn_id, sizeof(TunnelConnId));
  packet_data.append(_data);
  if (!params.tunservers.isEmpty())
  {
    if (!mgrconn_in)
      return false;
    return mgrconn_in->sendPacket(_cmd, packet_data);
  }
  MgrPacketLen len = packet_data.length();
  quint32 buf_len = buffered_packets_total_len;
  bool use_compression = true;
  if (mgrconn_in)
  {
    buf_len += mgrconn_in->output_buffer.length();
    use_compression = mgrconn_in->params.flags & MgrClientParameters::FL_USE_COMPRESSION;
  }
  if ((params.max_io_buffer_size > 0 && buf_len+len > params.max_io_buffer_size) ||
       buffered_packets_id_list.count()+1 > BUFFERED_PACKETS_MAX_COUNT)
  {
    log(LOG_DBG1, QString("mgrconn_in buffer overflow - closing/restarting tunnel"));
    buffered_packets_id_list.clear();
    buffered_packets_list.clear();
    buffered_packets_total_len = 0;
    QTimer::singleShot(0, this, SLOT(restart()));
    return false;
  }
  QByteArray packet_buffer;
  bool compressed = false;
  if (use_compression && len >= MGR_PACKET_MIN_LEN_FOR_COMPRESSION)
  {
    QByteArray compressed_packet_data = qCompress(packet_data);
    if (compressed_packet_data.length() < packet_data.length())
    {
      MgrPacketLen cmd = _cmd | MGR_PACKET_FLAG_COMPRESSED;
      len = compressed_packet_data.length();
      packet_buffer.reserve(sizeof(MgrPacketCmd)+sizeof(MgrPacketLen)+compressed_packet_data.length());
      packet_buffer.append(QByteArray((const char *)&cmd, sizeof(MgrPacketCmd)));
      packet_buffer.append(QByteArray((const char *)&len, sizeof(MgrPacketLen)));
      packet_buffer.append(compressed_packet_data);
      compressed = true;
    }
  }
  if (!compressed)
  {
    packet_buffer.reserve(sizeof(MgrPacketCmd)+sizeof(MgrPacketLen)+packet_data.length());
    packet_buffer.append(QByteArray((const char *)&_cmd, sizeof(MgrPacketCmd)));
    packet_buffer.append(QByteArray((const char *)&len, sizeof(MgrPacketLen)));
    packet_buffer.append(packet_data);
  }
  log(LOG_DBG4, QString(": queueing mgrconn_in packet cmd=%1, id=%2, conn_id=%3, len=%4").arg(mgrPacket_cmdString(_cmd)).arg(packet_id).arg(conn_id).arg(len));
  buffered_packets_id_list.append(packet_id);
  buffered_packets_list.append(packet_buffer);
  buffered_packets_total_len += packet_buffer.length();

  if (mgrconn_in && (state.flags & TunnelState::TF_MGRCONN_IN_CLEAR_TO_SEND))
  {
    mgrconn_in->output_buffer.append(packet_buffer);
    mgrconn_in->sendOutputBuffer();
  }
  return true;
}

//---------------------------------------------------------------------------
void Tunnel::connection_udpIncomingRead()
{
  while (bind_udpSocket->hasPendingDatagrams())
  {
    QByteArray datagram;
    datagram.resize(bind_udpSocket->pendingDatagramSize());
    QHostAddress sender;
    quint16 senderPort;

    if (bind_udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort) < 0)
    {
      log(LOG_HIGH, QString(": readDatagram() error: ").arg(bind_udpSocket->errorString()));
      continue;
    }
    log(LOG_DBG4, QString(": received datagram from %1:%2, size=%3").arg(sender.toString()).arg(senderPort).arg(datagram.size()));
    state.stats.data_bytes_rcv += datagram.size();

    QString hash_key = sender.toString()+QString(":%1").arg(senderPort);
    TunnelUdpConn *conn = udp_conn_list_by_addr.value(hash_key);
    if (!conn)
    {
      if ((quint32)udp_conn_list_by_id.count() >= params.max_incoming_connections)
      {
        QHashIterator<TunnelConnId, TunnelUdpConn *> i_udp_conn(udp_conn_list_by_id);
        TunnelConnId id_less_recent_used = 0;
        QTime t_less_recent_used;
        while (i_udp_conn.hasNext())
        {
          i_udp_conn.next();
          QTime t = i_udp_conn.value()->t_last_rcv;
          if (!t.isValid() || qAbs(i_udp_conn.value()->t_last_snd.elapsed()) < qAbs(t.elapsed()))
            t = i_udp_conn.value()->t_last_snd;
          if (!t_less_recent_used.isValid() || qAbs(t.elapsed()) > qAbs(t_less_recent_used.elapsed()))
          {
            id_less_recent_used = i_udp_conn.key();
            t_less_recent_used = t;
          }
        }
        if (id_less_recent_used > 0)
        {
          TunnelUdpConn *less_recent_used_conn = udp_conn_list_by_id.take(id_less_recent_used);
          QString less_recent_used_hash_key = less_recent_used_conn->remote_addr.toString()+QString(":%1").arg(less_recent_used_conn->remote_port);
          udp_conn_list_by_addr.remove(less_recent_used_hash_key);
          delete less_recent_used_conn;
          state.stats.conn_cur_count--;
        }
      }
      conn = new TunnelUdpConn;
      TunnelConnInInfo conn_info;
      conn_info.t_connected = QDateTime::currentDateTime().toUTC();
      conn->id = unique_conn_id++;
      while (conn->id == 0 || udp_conn_list_by_id.contains(conn->id))
        conn->id = unique_conn_id++;
      conn->remote_addr = conn_info.peer_address = sender;
      conn->remote_port = conn_info.peer_port = senderPort;
      udp_conn_list_by_id.insert(conn->id, conn);
      udp_conn_list_by_addr.insert(hash_key, conn);

      in_conn_info_list.insert(conn->id, conn_info);
      if (in_conn_info_list.count() > (int)params.max_incoming_connections_info || conn->id % 10 == 0)
        cleanup_in_conn_info_list();

      if (params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE)
        queueOutPacket(CMD_TUN_CONN_OUT_NEW, conn->id, next_packet_id());
      else if (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL)
        queueInPacket(CMD_TUN_CONN_OUT_NEW, conn->id, next_packet_id());

      state.stats.conn_total_count++;
      state.stats.conn_cur_count++;

      // if we need to initiate tunnel on demand
      if (params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE && !(params.flags & TunnelParameters::FL_PERMANENT_TUNNEL))
      {
        if (mgrconn_out->state() == QAbstractSocket::UnconnectedState)
        {
          state.flags &= ~TunnelState::TF_IDLE;
          mgrconn_out->beginConnection();
          emit state_changed();
        }
        else if (mgrconn_out->state() == QAbstractSocket::ClosingState)
          close_incoming_connections();
        else if (state.flags & TunnelState::TF_IDLE)
        {
          state.flags &= ~TunnelState::TF_IDLE;
          if (mgrconn_out->timer_idle->isActive())
          {
            mgrconn_out->timer_idle->stop();
            mgrconn_out->log(LOG_DBG1, QString(": stopped idle timer"));
          }
          emit state_changed();
        }
      }
    }
    conn->t_last_rcv.restart();
//    conn->bytes_rcv += datagram.size();
    if (in_conn_info_list.contains(conn->id))
      in_conn_info_list[conn->id].bytes_rcv += datagram.size();

    if (params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE)
      queueOutPacket(CMD_TUN_CONN_IN_DATA, conn->id, next_packet_id(), datagram);
    else if (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL)
      queueInPacket(CMD_TUN_CONN_IN_DATA, conn->id, next_packet_id(), datagram);
  }
}

//---------------------------------------------------------------------------
void Tunnel::connection_udpOutgoingRead()
{
  QUdpSocket *udp_sock = qobject_cast<QUdpSocket *>(sender());
  TunnelUdpConn *conn = udp_conn_list_by_addr.value(QString::number(udp_sock->localPort()));
  if (!conn)
    return;
  while (udp_sock->hasPendingDatagrams())
  {
    QByteArray datagram;
    datagram.resize(udp_sock->pendingDatagramSize());
    QHostAddress sender;
    quint16 senderPort;

    if (udp_sock->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort) < 0)
    {
      log(LOG_HIGH, QString(": readDatagram() error: ").arg(udp_sock->errorString()));
      continue;
    }
    log(LOG_DBG4, QString(", conn %1: received datagram from %2:%3, size=%4").arg(conn->id).arg(sender.toString()).arg(senderPort).arg(datagram.size()));
    state.stats.data_bytes_rcv += datagram.size();
    conn->t_last_rcv.restart();
//    conn->bytes_rcv += datagram.size();

    if (params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE)
      queueInPacket(CMD_TUN_CONN_OUT_DATA, conn->id, next_packet_id(), datagram);
    else if (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL)
      queueOutPacket(CMD_TUN_CONN_OUT_DATA, conn->id, next_packet_id(), datagram);
  }
}

