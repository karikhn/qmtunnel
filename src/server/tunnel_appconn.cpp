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
bool Tunnel::bind_start()
{
  if (params.app_protocol == TunnelParameters::TCP && !bind_tcpServer)
  {
    QHostAddress bind_address = QHostAddress(params.bind_address);
    if (bind_address.isNull())
    {
      state.last_error_code = TunnelState::RES_CODE_BIND_ERROR;
      state.last_error_str = tr("Invalid bind address '%1' on %2").arg(params.bind_address).arg(SysUtil::machine_name);
      emit state_changed();
      return false;
    }
    bind_tcpServer = new QTcpServer;
    if (params.max_incoming_connections > 0)
      bind_tcpServer->setMaxPendingConnections(params.max_incoming_connections);
    connect(bind_tcpServer, SIGNAL(newConnection()), this, SLOT(new_incoming_conn()));
    if (!bind_tcpServer->listen(bind_address, params.bind_port))
    {
      state.last_error_code = TunnelState::RES_CODE_BIND_ERROR;
      state.last_error_str = tr("Failed to bind to %1:%2 on %3: ").arg(params.bind_address).arg(params.bind_port).arg(SysUtil::machine_name)+bind_tcpServer->errorString();
      this->log(LOG_DBG1, QString(": ")+state.last_error_str);
      delete bind_tcpServer;
      bind_tcpServer = NULL;
      emit state_changed();
      return false;
    }
    this->log(LOG_DBG1, QString(": binding TCP server on %1:%2 started").arg(bind_tcpServer->serverAddress().toString()).arg(bind_tcpServer->serverPort()));
  }
  else if (params.app_protocol == TunnelParameters::UDP && !bind_udpSocket)
  {
    QHostAddress bind_address = QHostAddress(params.bind_address);
    if (bind_address.isNull())
    {
      state.last_error_code = TunnelState::RES_CODE_BIND_ERROR;
      state.last_error_str = tr("Invalid bind address '%1' on %2").arg(params.bind_address).arg(SysUtil::machine_name);
      this->log(LOG_DBG1, QString(": ")+state.last_error_str);
      emit state_changed();
      return false;
    }
    bind_udpSocket = new QUdpSocket;
    connect(bind_udpSocket, SIGNAL(bytesWritten(qint64)), this, SLOT(connection_bytesWritten(qint64)));
    connect(bind_udpSocket, SIGNAL(readyRead()), this, SLOT(connection_udpIncomingRead()));
    if (!bind_udpSocket->bind(bind_address, params.bind_port, QUdpSocket::DontShareAddress | QUdpSocket::ReuseAddressHint))
    {
      state.last_error_code = TunnelState::RES_CODE_BIND_ERROR;
      state.last_error_str = tr("Failed to bind to %1:%2 on %3: ").arg(params.bind_address).arg(params.bind_port).arg(SysUtil::machine_name)+bind_udpSocket->errorString();
      this->log(LOG_DBG1, QString(": ")+state.last_error_str);
      delete bind_udpSocket;
      bind_udpSocket = NULL;
      emit state_changed();
      return false;
    }
    this->log(LOG_DBG1, QString(": binding UDP socket on %1:%2 opened").arg(bind_udpSocket->localAddress().toString()).arg(bind_udpSocket->localPort()));
  }
  else if (params.app_protocol == TunnelParameters::PIPE && !bind_pipeServer)
  {
    if (params.bind_address.isEmpty())
    {
      state.last_error_code = TunnelState::RES_CODE_BIND_ERROR;
      state.last_error_str = tr("Invalid bind address '%1' on %2").arg(params.bind_address).arg(SysUtil::machine_name);
      this->log(LOG_DBG1, QString(": ")+state.last_error_str);
      emit state_changed();
      return false;
    }
    bind_pipeServer = new QLocalServer;
    if (params.max_incoming_connections > 0)
      bind_pipeServer->setMaxPendingConnections(params.max_incoming_connections);
    connect(bind_pipeServer, SIGNAL(newConnection()), this, SLOT(new_incoming_conn()));
    if (!bind_pipeServer->listen(params.bind_address))
    {
      state.last_error_code = TunnelState::RES_CODE_BIND_ERROR;
      state.last_error_str = tr("Failed to bind to '%1' on %2: ").arg(params.bind_address).arg(SysUtil::machine_name)+bind_pipeServer->errorString();
      this->log(LOG_DBG1, QString(": ")+state.last_error_str);
      delete bind_pipeServer;
      bind_pipeServer = NULL;
      emit state_changed();
      return false;
    }
    this->log(LOG_DBG1, QString(": binding named pipe '%1' opened").arg(bind_pipeServer->serverName()));
  }
  state.flags |= TunnelState::TF_BINDING;
  emit state_changed();
  return true;
}

//---------------------------------------------------------------------------
void Tunnel::close_incoming_connections()
{
  if (!(params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE && (params.flags & TunnelParameters::FL_MASTER_TUNSERVER)) &&
      !(params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL && (params.tunservers.isEmpty())))
    return;

  QHashIterator<TunnelConnId, TunnelConn *> i_in_conn(in_conn_list);
  while (i_in_conn.hasNext())
  {
    i_in_conn.next();
    i_in_conn.value()->close();
  }
  in_conn_list.clear();
  QHashIterator<TunnelConnId, TunnelUdpConn *> i_udp_conn(udp_conn_list_by_id);
  while (i_udp_conn.hasNext())
  {
    i_udp_conn.next();
    delete i_udp_conn.value();
  }
  udp_conn_list_by_addr.clear();
  udp_conn_list_by_id.clear();
}

//---------------------------------------------------------------------------
void Tunnel::close_outgoing_connections()
{
  if (!(params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE && (params.tunservers.isEmpty())) &&
      !(params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL && (params.flags & TunnelParameters::FL_MASTER_TUNSERVER)))
    return;

  QHashIterator<TunnelConnId, TunnelConn *> i(out_conn_list);
  while (i.hasNext())
  {
    i.next();
    i.value()->close();
  }
  out_conn_list.clear();
  QHashIterator<TunnelConnId, TunnelUdpConn *> i_udp_conn(udp_conn_list_by_id);
  while (i_udp_conn.hasNext())
  {
    i_udp_conn.next();
    delete i_udp_conn.value();
  }
  udp_conn_list_by_addr.clear();
  udp_conn_list_by_id.clear();
}

//---------------------------------------------------------------------------
void Tunnel::incoming_connection_finished(int error_code, const QString &error_str)
{
  TunnelConn *in_conn = qobject_cast<TunnelConn *>(sender());

  if (!in_conn->closing_by_cmd)
  {
    MgrPacket_StandartReply pkt;
    pkt.res_code = error_code;
    pkt.error_len = error_str.toUtf8().length();
    QByteArray pkt_data;
    pkt_data.append((const char *)&pkt, sizeof(MgrPacket_StandartReply));
    pkt_data.append(error_str.toUtf8());
    if (params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE)
      queueOutPacket(CMD_TUN_CONN_OUT_DROP, in_conn->id, next_packet_id(), pkt_data);
    else if (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL)
      queueInPacket(CMD_TUN_CONN_OUT_DROP, in_conn->id, next_packet_id(), pkt_data);
  }

  state.stats.conn_cur_count--;

  in_conn_list.remove(in_conn->id);
  if (in_conn_info_list.contains(in_conn->id))
  {
    in_conn_info_list[in_conn->id].t_disconnected = QDateTime::currentDateTime().toUTC();
    if (in_conn_info_list[in_conn->id].errorstring.isEmpty())
      in_conn_info_list[in_conn->id].errorstring = error_str.toUtf8();
  }
  in_conn->deleteLater();

  if (in_conn_list.isEmpty() && udp_conn_list_by_id.isEmpty() &&
      !(params.flags & TunnelParameters::FL_PERMANENT_TUNNEL) &&
      params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE &&
      (params.flags & TunnelParameters::FL_MASTER_TUNSERVER))
  {
    if (mgrconn_out && (state.flags & TunnelState::TF_MGRCONN_OUT_CONNECTED))
    {
      state.flags |= TunnelState::TF_IDLE;
      mgrconn_out->timer_idle->setInterval(params.idle_timeout);
      mgrconn_out->log(LOG_DBG1, QString(": starting idle timer (%1 ms)").arg(params.idle_timeout));
      mgrconn_out->timer_idle->start();
      emit state_changed();
    }
  }
}

//---------------------------------------------------------------------------
void Tunnel::outgoing_connection_finished(int error_code, const QString &error_str)
{
  TunnelConn *out_conn = qobject_cast<TunnelConn *>(sender());

  if (!out_conn->closing_by_cmd)
  {
    MgrPacket_StandartReply pkt;
    pkt.res_code = error_code;
    pkt.error_len = error_str.toUtf8().length();
    QByteArray pkt_data;
    pkt_data.append((const char *)&pkt, sizeof(MgrPacket_StandartReply));
    pkt_data.append(error_str.toUtf8());
    if (params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE)
      queueInPacket(CMD_TUN_CONN_IN_DROP, out_conn->id, next_packet_id(), pkt_data);
    else if (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL)
      queueOutPacket(CMD_TUN_CONN_IN_DROP, out_conn->id, next_packet_id(), pkt_data);
  }

  state.stats.conn_cur_count--;

  out_conn_list.remove(out_conn->id);
  out_conn->deleteLater();
}

//---------------------------------------------------------------------------
void Tunnel::outgoing_connection_established()
{
  TunnelConn *out_conn = qobject_cast<TunnelConn *>(sender());
  quint16 out_port = 0;
  if (out_conn->tcp_sock)
    out_port = out_conn->tcp_sock->localPort();

  if (out_conn->direction == TunnelConn::OUTGOING)
  {
    if (params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE)
      queueInPacket(CMD_TUN_CONN_OUT_CONNECTED, out_conn->id, next_packet_id(), QByteArray((const char *)&out_port, sizeof(quint16)));
    else if (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL)
      queueOutPacket(CMD_TUN_CONN_OUT_CONNECTED, out_conn->id, next_packet_id(), QByteArray((const char *)&out_port, sizeof(quint16)));
  }
}

//---------------------------------------------------------------------------
void Tunnel::bind_stop()
{
  if (bind_tcpServer)
  {
    bind_tcpServer->close();
    this->log(LOG_DBG1, QString(": binding TCP server on %1:%2 closed").arg(params.bind_address).arg(params.bind_port));
    bind_tcpServer->deleteLater();
    bind_tcpServer = NULL;
  }
  else if (bind_udpSocket)
  {
    bind_udpSocket->abort();
    this->log(LOG_DBG1, QString(": binding UDP socket on %1:%2 closed").arg(params.bind_address).arg(params.bind_port));
    bind_udpSocket->deleteLater();
    bind_udpSocket = NULL;
  }
  else if (bind_pipeServer)
  {
    bind_pipeServer->close();
    this->log(LOG_DBG1, QString(": binding named pipe '%1' closed").arg(params.bind_address));
    bind_pipeServer->deleteLater();
    bind_pipeServer = NULL;
  }
  close_incoming_connections();
  if (state.flags & TunnelState::TF_BINDING)
  {
    state.flags &= ~TunnelState::TF_BINDING;
  }
  emit state_changed();
}

//---------------------------------------------------------------------------
void Tunnel::new_incoming_conn()
{
  if (!bind_tcpServer && !bind_pipeServer)
    return;

  if ((quint32)in_conn_list.count() >= params.max_incoming_connections)
  {
    if (bind_tcpServer)
    {
      QTcpSocket *sock = bind_tcpServer->nextPendingConnection();
      this->log(LOG_LOW, QString(": maximum number of incoming TCP connections reached (%1) - dropping new connection from %2:%3").arg(params.max_incoming_connections).arg(sock->peerAddress().toString()).arg(sock->peerPort()));
      sock->abort();
      sock->deleteLater();
    }
    else if (bind_pipeServer)
    {
      QLocalSocket *sock = bind_pipeServer->nextPendingConnection();
      this->log(LOG_LOW, QString(": maximum number of incoming PIPE connections reached (%1) - dropping new connection from '%2'").arg(params.max_incoming_connections).arg(sock->fullServerName()));
      sock->abort();
      sock->deleteLater();
    }
    return;
  }


  TunnelConn *new_conn = new TunnelConn(TunnelConn::INCOMING, params);
  TunnelConnInInfo conn_info;
  conn_info.t_connected = QDateTime::currentDateTime().toUTC();
  new_conn->id = unique_conn_id++;
  while (in_conn_list.contains(new_conn->id) || new_conn->id == 0)
    new_conn->id = unique_conn_id++;
  connect(new_conn, SIGNAL(finished(int,QString)), this, SLOT(incoming_connection_finished(int,QString)));
  connect(new_conn, SIGNAL(dataReceived()), this, SLOT(connection_dataReceived()));
  connect(new_conn, SIGNAL(connection_bytesWritten(qint64)), this, SLOT(connection_bytesWritten(qint64)));
  if (bind_tcpServer)
  {
    QTcpSocket *sock = bind_tcpServer->nextPendingConnection();
    conn_info.peer_address = sock->peerAddress();
    conn_info.peer_port = sock->peerPort();
    new_conn->tcp_sock = sock;
  }
  else if (bind_pipeServer)
  {
    QLocalSocket *sock = bind_pipeServer->nextPendingConnection();
    new_conn->pipe_sock = sock;
  }
  in_conn_list.insert(new_conn->id, new_conn);
  in_conn_info_list.insert(new_conn->id, conn_info);

  if (in_conn_info_list.count() > (int)params.max_incoming_connections_info || new_conn->id % 10 == 0)
    cleanup_in_conn_info_list();

  new_conn->init_incoming();

  if (params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE)
    queueOutPacket(CMD_TUN_CONN_OUT_NEW, new_conn->id, next_packet_id());
  else if (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL)
    queueInPacket(CMD_TUN_CONN_OUT_NEW, new_conn->id, next_packet_id());

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

//---------------------------------------------------------------------------
// command to open new outgoing connection received (new incoming connection)
void Tunnel::cmd_conn_out_new(TunnelConnId conn_id)
{
  if ((params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE && params.tunservers.isEmpty()) ||
      (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL && (params.flags & TunnelParameters::FL_MASTER_TUNSERVER)))
  {
    if (out_conn_list.contains(conn_id))
    {
      //! error
      return;
    }
    if (params.app_protocol == TunnelParameters::UDP)
    {
      if (udp_remote_addr.isNull() && !udp_remote_addr_lookup_in_progress)
      {
        udp_remote_addr_lookup_in_progress = true;
        this->log(LOG_DBG1, QString(": starting host '%1' lookup").arg(params.remote_host));
        udp_remote_addr_lookup_id = QHostInfo::lookupHost(params.remote_host, this, SLOT(udp_hostLookupFinished(QHostInfo)));
      }
      TunnelUdpConn *new_conn = new TunnelUdpConn;
      new_conn->id = conn_id;
      new_conn->udp_sock = new QUdpSocket;
      connect(new_conn->udp_sock, SIGNAL(bytesWritten(qint64)), this, SLOT(connection_bytesWritten(qint64)));
      connect(new_conn->udp_sock, SIGNAL(readyRead()), this, SLOT(connection_udpOutgoingRead()));
      quint16 start_udp_port = next_udp_port;
      while (!new_conn->udp_sock->bind(QHostAddress::Any, next_udp_port, QUdpSocket::DontShareAddress | QUdpSocket::ReuseAddressHint))
      {
        next_udp_port++;
        if (next_udp_port > params.udp_port_range_till && params.udp_port_range_from <= params.udp_port_range_till)
          next_udp_port = params.udp_port_range_from;
        if (next_udp_port == start_udp_port || next_udp_port == 0 || next_udp_port > params.udp_port_range_till)
        {
          state.last_error_code = TunnelState::RES_CODE_BIND_ERROR;
          state.last_error_str = tr("Failed to bind to port range %1-%2 on %3: ").arg(params.udp_port_range_from).arg(params.udp_port_range_till).arg(SysUtil::machine_name)+new_conn->udp_sock->errorString();
          this->log(LOG_DBG1, QString(": ")+state.last_error_str);
          delete new_conn;
          emit state_changed();
          return;
        }
      }
      this->log(LOG_DBG3, QString(", conn %1: binding UDP socket on port %2 opened").arg(new_conn->id).arg(new_conn->udp_sock->localPort()));

      if (params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE)
        queueInPacket(CMD_TUN_CONN_OUT_CONNECTED, new_conn->id, next_packet_id(), QByteArray((const char *)&next_udp_port, sizeof(quint16)));
      else if (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL)
        queueOutPacket(CMD_TUN_CONN_OUT_CONNECTED, new_conn->id, next_packet_id(), QByteArray((const char *)&next_udp_port, sizeof(quint16)));

      udp_conn_list_by_id.insert(new_conn->id, new_conn);
      udp_conn_list_by_addr.insert(QString::number(new_conn->udp_sock->localPort()), new_conn);
    }
    else
    {
      TunnelConn *new_conn = new TunnelConn(TunnelConn::OUTGOING, params);
      connect(new_conn, SIGNAL(finished(int,QString)), this, SLOT(outgoing_connection_finished(int,QString)));
      connect(new_conn, SIGNAL(connection_established()), this, SLOT(outgoing_connection_established()));
      connect(new_conn, SIGNAL(connection_bytesWritten(qint64)), this, SLOT(connection_bytesWritten(qint64)));
      connect(new_conn, SIGNAL(dataReceived()), this, SLOT(connection_dataReceived()));
      new_conn->id = conn_id;
      if (params.app_protocol == TunnelParameters::TCP)
      {
        new_conn->tcp_sock = new QTcpSocket;
      }
      else if (params.app_protocol == TunnelParameters::PIPE)
      {
        new_conn->pipe_sock = new QLocalSocket;
      }
      out_conn_list.insert(new_conn->id, new_conn);
      new_conn->init_outgoing(params.remote_host, params.remote_port, params.connect_timeout);
    }
    state.stats.conn_cur_count++;
    state.stats.conn_total_count++;
  }
}

//---------------------------------------------------------------------------
// command to close outgoing connection received (incoming connection closed)
void Tunnel::cmd_conn_out_drop(TunnelConnId conn_id, const QByteArray &data)
{
  if ((params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE && params.tunservers.isEmpty()) ||
      (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL && (params.flags & TunnelParameters::FL_MASTER_TUNSERVER)))
  {
    TunnelConn *conn = out_conn_list.value(conn_id);
    int error_code = 0;
    QString error_str;
    if (data.length() >= (int)sizeof(MgrPacket_StandartReply))
    {
      MgrPacket_StandartReply *pkt = (MgrPacket_StandartReply *)data.constData();
      error_code = (int)pkt->res_code-1;
      if (pkt->error_len > 0)
        error_str = QString::fromUtf8(data.mid(sizeof(MgrPacket_StandartReply), pkt->error_len));
    }
    if (conn)
    {
      if (error_code >= 0 && error_code != QAbstractSocket::RemoteHostClosedError)
        state.stats.conn_failed_count++;
      conn->closing_by_cmd = true;
      conn->close();
    }
  }
}

//---------------------------------------------------------------------------
// command to drop incoming connection received (outgoing connection closed)
void Tunnel::cmd_conn_in_drop(TunnelConnId conn_id, const QByteArray &data)
{
  if ((params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE && (params.flags & TunnelParameters::FL_MASTER_TUNSERVER)) ||
      (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL && params.tunservers.isEmpty()))
  {
    TunnelConn *conn = in_conn_list.value(conn_id);
    int error_code = 0;
    QString error_str;
    if (data.length() >= (int)sizeof(MgrPacket_StandartReply))
    {
      MgrPacket_StandartReply *pkt = (MgrPacket_StandartReply *)data.constData();
      error_code = (int)pkt->res_code-1;
      if (pkt->error_len > 0)
      {
        error_str = QString::fromUtf8(data.mid(sizeof(MgrPacket_StandartReply), pkt->error_len));
        if (conn && in_conn_info_list.contains(conn_id))
        {
          in_conn_info_list[conn_id].t_disconnected = QDateTime::currentDateTime().toUTC();
          if (in_conn_info_list[conn_id].errorstring.isEmpty())
            in_conn_info_list[conn_id].errorstring = error_str.toUtf8();
        }
      }
    }
    if (conn)
    {
      if (error_code >= 0 && error_code != QAbstractSocket::RemoteHostClosedError)
        state.stats.conn_failed_count++;
      conn->closing_by_cmd = true;
      conn->close();
    }
  }
}

//---------------------------------------------------------------------------
// notification about outgoing connection established received
void Tunnel::cmd_conn_out_connected(TunnelConnId conn_id, const QByteArray &data)
{
  if ((params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE && (params.flags & TunnelParameters::FL_MASTER_TUNSERVER)) ||
      (params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL && params.tunservers.isEmpty()))
  {
    if (in_conn_info_list.contains(conn_id))
    {
      if (data.length() >= (int)sizeof(quint16))
        in_conn_info_list[conn_id].tun_out_port = *((quint16 *)data.data());
      in_conn_info_list[conn_id].flags |= TunnelConnInInfo::FL_OUT_CONNECTED;
    }
  }
}

//---------------------------------------------------------------------------
void Tunnel::cleanup_in_conn_info_list()
{
  if (!(params.fwd_direction == TunnelParameters::LOCAL_TO_REMOTE && (params.flags & TunnelParameters::FL_MASTER_TUNSERVER)) &&
      !(params.fwd_direction == TunnelParameters::REMOTE_TO_LOCAL && (params.tunservers.isEmpty())))
    return;

  // remove old disconnected connections info
  QDateTime now = QDateTime::currentDateTime().toUTC();
  int old_conn_count = 0;
  int oldest_conn = 0;
  QMutableHashIterator<TunnelConnId, TunnelConnInInfo> i_in_conn_info(in_conn_info_list);
  while (i_in_conn_info.hasNext())
  {
    i_in_conn_info.next();
    QDateTime t_disconnected = i_in_conn_info.value().t_disconnected;
    if (params.app_protocol == TunnelParameters::UDP)
      t_disconnected = now;
    if (t_disconnected.isValid())
    {
      if (i_in_conn_info.value().t_connected.secsTo(t_disconnected) > (int)params.incoming_connections_info_timeout)
        i_in_conn_info.remove();
      else
      {
        old_conn_count++;
        if (oldest_conn == 0 || oldest_conn < i_in_conn_info.value().t_connected.secsTo(t_disconnected))
          oldest_conn = i_in_conn_info.value().t_connected.secsTo(t_disconnected);
      }
    }
  }

  if (params.max_incoming_connections_info > 0)
  {
    // first try to remove oldest connections info
    while (in_conn_info_list.count() > (int)params.max_incoming_connections_info && old_conn_count > 0)
    {
      // try to remove half of oldest connections info on each step
      int _timeout = oldest_conn/2;
      old_conn_count = 0;
      oldest_conn = 0;
      QMutableHashIterator<TunnelConnId, TunnelConnInInfo> i_in_conn_info(in_conn_info_list);
      while (i_in_conn_info.hasNext())
      {
        i_in_conn_info.next();
        QDateTime t_disconnected = i_in_conn_info.value().t_disconnected;
        if (params.app_protocol == TunnelParameters::UDP)
          t_disconnected = now;
        if (t_disconnected.isValid())
        {
          if (_timeout < 1 || i_in_conn_info.value().t_connected.secsTo(t_disconnected) > _timeout)
            i_in_conn_info.remove();
          else
          {
            old_conn_count++;
            if (oldest_conn == 0 || oldest_conn < i_in_conn_info.value().t_connected.secsTo(t_disconnected))
              oldest_conn = i_in_conn_info.value().t_connected.secsTo(t_disconnected);
          }
        }
      }
    }

    // if that doesn't help - just remove active connections info randomly
    while (in_conn_info_list.count() > (int)params.max_incoming_connections_info)
    {
      QMutableHashIterator<TunnelConnId, TunnelConnInInfo> i_in_conn_info(in_conn_info_list);
      if (i_in_conn_info.hasNext())
      {
        i_in_conn_info.next();
        i_in_conn_info.remove();
      }
    }
  }
}

//---------------------------------------------------------------------------
QByteArray Tunnel::in_connPrintToBuffer(bool include_disconnected)
{
  QByteArray buffer;
  buffer.reserve(in_conn_info_list.count()*128);
  QHashIterator<TunnelConnId, TunnelConnInInfo> i(in_conn_info_list);
  while (i.hasNext())
  {
    i.next();
    if (!include_disconnected && i.value().t_disconnected.isValid())
      continue;

    buffer.append((const char *)&i.key(), sizeof(TunnelConnId));
    buffer.append(i.value().printToBuffer());
  }
  return buffer;
}

//---------------------------------------------------------------------------
bool tcp_check_listen_port(const QHostAddress &listen_addr, quint16 listen_port, QString &config_error)
{
  QTcpServer *test_server = new QTcpServer;
  if (!test_server->listen(listen_addr, listen_port))
    config_error = QObject::tr("Port %1 listening error on interface %2 (host %3): ").arg(listen_port).arg(listen_addr.toString()).arg(SysUtil::machine_name)+test_server->errorString();
  else
    test_server->close();
  delete test_server;
  return config_error.isEmpty();
}

//---------------------------------------------------------------------------
bool udp_check_port(const QHostAddress &listen_addr, quint16 listen_port, QString &config_error)
{
  QUdpSocket *test_sock = new QUdpSocket;
  if (!test_sock->bind(listen_addr, listen_port, QUdpSocket::DontShareAddress | QUdpSocket::ReuseAddressHint))
    config_error = QObject::tr("Unable to bind to port %1 on interface %2 (host %3): ").arg(listen_port).arg(listen_addr.toString()).arg(SysUtil::machine_name)+test_sock->errorString();
  else
    test_sock->abort();
  delete test_sock;
  return config_error.isEmpty();
}

//---------------------------------------------------------------------------
bool named_pipe_check(const QString &path, QString &config_error)
{
  QLocalServer *test_server = new QLocalServer;
  if (!test_server->listen(path))
    config_error = QObject::tr("Unable to open named pipe %1 (host %2): ").arg(path).arg(SysUtil::machine_name)+test_server->errorString();
  else
    test_server->close();
  delete test_server;
  return config_error.isEmpty();
}

