/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include <QHostAddress>
#include "tunnel_conn.h"

//---------------------------------------------------------------------------
void TunnelConn::init_incoming()
{
  if (tcp_sock)
  {
    log(LOG_DBG3, QString(": new incoming TCP connection from %1:%2").arg(tcp_sock->peerAddress().toString()).arg(tcp_sock->peerPort()));
    connect(tcp_sock, SIGNAL(disconnected()), this, SLOT(socket_disconnected()));
    connect(tcp_sock, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(tcp_socket_error(QAbstractSocket::SocketError)));
    connect(tcp_sock, SIGNAL(bytesWritten(qint64)), this, SLOT(socket_bytesWritten(qint64)));
    connect(tcp_sock, SIGNAL(readyRead()), this, SLOT(socket_readyRead()));
  }
  else if (pipe_sock)
  {
    log(LOG_DBG3, QString(": new incoming PIPE connection on '%1'").arg(pipe_sock->fullServerName()));
    connect(pipe_sock, SIGNAL(disconnected()), this, SLOT(socket_disconnected()));
    connect(pipe_sock, SIGNAL(error(QLocalSocket::LocalSocketError)), this, SLOT(local_socket_error(QLocalSocket::LocalSocketError)));
    connect(pipe_sock, SIGNAL(bytesWritten(qint64)), this, SLOT(socket_bytesWritten(qint64)));
    connect(pipe_sock, SIGNAL(readyRead()), this, SLOT(socket_readyRead()));
  }
}

//---------------------------------------------------------------------------
void TunnelConn::init_outgoing(const QString &remote_host, quint16 remote_port, quint32 connect_timeout)
{
  if (tcp_sock)
  {
    log(LOG_DBG3, QString(": establishing outgoing TCP connection to %1:%2").arg(remote_host).arg(remote_port));
    connect(tcp_sock, SIGNAL(connected()), this, SLOT(socket_connected()));
    connect(tcp_sock, SIGNAL(disconnected()), this, SLOT(socket_disconnected()));
    connect(tcp_sock, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(tcp_socket_error(QAbstractSocket::SocketError)));
    connect(tcp_sock, SIGNAL(bytesWritten(qint64)), this, SLOT(socket_bytesWritten(qint64)));
    connect(tcp_sock, SIGNAL(readyRead()), this, SLOT(socket_readyRead()));
    timer_connect->setInterval(connect_timeout);
    timer_connect->start();
    tcp_sock->connectToHost(remote_host, remote_port);
  }
  else if (pipe_sock)
  {
    log(LOG_DBG3, QString(": establishing outgoing PIPE connection to %1").arg(remote_host));
    connect(pipe_sock, SIGNAL(connected()), this, SLOT(socket_connected()));
    connect(pipe_sock, SIGNAL(disconnected()), this, SLOT(socket_disconnected()));
    connect(pipe_sock, SIGNAL(error(QLocalSocket::LocalSocketError)), this, SLOT(local_socket_error(QLocalSocket::LocalSocketError)));
    connect(pipe_sock, SIGNAL(bytesWritten(qint64)), this, SLOT(socket_bytesWritten(qint64)));
    connect(pipe_sock, SIGNAL(readyRead()), this, SLOT(socket_readyRead()));
    timer_connect->setInterval(connect_timeout);
    timer_connect->start();
    pipe_sock->connectToServer(remote_host);
  }
}

//---------------------------------------------------------------------------
void TunnelConn::socket_connected()
{
  if (timer_connect->isActive())
    timer_connect->stop();
  emit connection_established();
  log(LOG_DBG3, QString(": connected"));
  if (tcp_sock)
  {
    tcp_sock->setSocketOption(QTcpSocket::KeepAliveOption, (tun_params.flags & TunnelParameters::FL_TCP_KEEP_ALIVE) ? 1 : 0);
    tcp_sock->setSocketOption(QTcpSocket::LowDelayOption, (tun_params.flags & TunnelParameters::FL_TCP_NO_DELAY) ? 1 : 0);
    tcp_sock->setReadBufferSize(tun_params.read_buffer_size);
  }
  else if (pipe_sock)
    pipe_sock->setReadBufferSize(tun_params.read_buffer_size);
  if (!output_buffer.isEmpty())
    sendOutputBuffer();
}

//---------------------------------------------------------------------------
void TunnelConn::socket_connect_timeout()
{
  log(LOG_DBG3, QString(": connect timeout"));
  this->close(true);
}

//---------------------------------------------------------------------------
void TunnelConn::socket_disconnected()
{
  if (timer_connect->isActive())
    timer_connect->stop();
  log(LOG_DBG3, QString(": disconnected"));
  this->close();
}

//---------------------------------------------------------------------------
void TunnelConn::tcp_socket_error(QAbstractSocket::SocketError)
{
  if (timer_connect->isActive())
    timer_connect->stop();
  log(LOG_DBG3, QString(" error: ")+tcp_sock->errorString());
  this->close();
}

//---------------------------------------------------------------------------
void TunnelConn::local_socket_error(QLocalSocket::LocalSocketError)
{
  if (timer_connect->isActive())
    timer_connect->stop();
  log(LOG_DBG3, QString(" error: ")+pipe_sock->errorString());
  this->close();
}

//-----------------------------------------------------------------------------
void TunnelConn::sendOutputBuffer()
{
  quint64 bytes_to_write=0;
  if (tcp_sock)
    bytes_to_write = tcp_sock->bytesToWrite();
  else if (pipe_sock)
    bytes_to_write = pipe_sock->bytesToWrite();
  int max_new_bytes_to_write = output_buffer.length();
  if (tun_params.write_buffer_size > 0 && bytes_to_write+max_new_bytes_to_write > tun_params.write_buffer_size)
    max_new_bytes_to_write = tun_params.write_buffer_size-bytes_to_write;
  if (max_new_bytes_to_write <= 0)
    return;
  int send_len=0;
  if (tcp_sock)
    send_len = tcp_sock->write(output_buffer, max_new_bytes_to_write);
  else if (pipe_sock)
    send_len = pipe_sock->write(output_buffer, max_new_bytes_to_write);
  if (send_len > 0)
  {
    output_buffer.remove(0, send_len);
    t_last_snd.restart();
  }
}

//---------------------------------------------------------------------------
void TunnelConn::socket_bytesWritten(qint64 bytes)
{
  t_last_snd.restart();
//  bytes_snd += bytes;
  emit connection_bytesWritten(bytes);
  quint64 bytes_to_write=0;
  if (tcp_sock)
    bytes_to_write = tcp_sock->bytesToWrite();
  else if (pipe_sock)
    bytes_to_write = pipe_sock->bytesToWrite();
  if (bytes_to_write > 0 && output_buffer.length() > 0)
    log(LOG_DBG4, QString(": %1 bytes written (%2 more in internal buffer, %3 in output buffer)").arg(bytes).arg(bytes_to_write).arg(output_buffer.length()));
  else if (bytes_to_write > 0)
    log(LOG_DBG4, QString(": %1 bytes written (%2 more in internal buffer)").arg(bytes).arg(bytes_to_write));
  else if (output_buffer.length() > 0)
    log(LOG_DBG4, QString(": %1 bytes written (%3 more in output buffer)").arg(bytes).arg(output_buffer.length()));
  else
    log(LOG_DBG4, QString(": %1 bytes written").arg(bytes));
  if (!output_buffer.isEmpty())
    sendOutputBuffer();
}

//---------------------------------------------------------------------------
void TunnelConn::socket_readyRead()
{
  t_last_rcv.restart();
  int bytes_avail = 0;
  if (tcp_sock)
    bytes_avail = tcp_sock->bytesAvailable();
  if (pipe_sock)
    bytes_avail = pipe_sock->bytesAvailable();

  if (tun_params.read_buffer_size > 0 && (quint32)input_buffer.length() >= tun_params.read_buffer_size)
  {
    // input buffer overflows, slow down and reschedule
    QTimer::singleShot(5, this, SLOT(socket_readyRead()));
    return;
  }

  int bytes_to_read = tun_params.max_bytes_to_read_at_once > 0 ? tun_params.max_bytes_to_read_at_once : bytes_avail;
  if (tun_params.read_buffer_size > 0 && (quint32)input_buffer.length()+bytes_to_read > tun_params.read_buffer_size)
    bytes_to_read = tun_params.read_buffer_size-input_buffer.length();

  QByteArray data;
  if (tcp_sock)
    data = tcp_sock->read(bytes_to_read);
  else if (pipe_sock)
    data = pipe_sock->read(bytes_to_read);
  input_buffer.append(data);

//  bytes_rcv += data.length();
  bytes_avail -= data.length();

  if (prc_log_level >= LOG_DBG4)
  {
    if (bytes_avail > 0)
      log(LOG_DBG4, QString(": %1 bytes received (%2 more available)").arg(data.length()).arg(bytes_avail));
    else
      log(LOG_DBG4, QString(": %1 bytes received").arg(data.length()));
  }

  emit dataReceived();

  if (bytes_avail > 0)
    QTimer::singleShot(0, this, SLOT(socket_readyRead()));
}

//---------------------------------------------------------------------------
void TunnelConn::close(bool connect_timeout)
{
  if (closing)
    return;
  closing = true;
  if (timer_connect->isActive())
    timer_connect->stop();
  if (tcp_sock)
  {
    emit finished((connect_timeout ? QAbstractSocket::SocketTimeoutError : tcp_sock->error())+1,
                  connect_timeout ? QString("Connect timeout") : tcp_sock->errorString());
    if (tcp_sock->state() != QAbstractSocket::UnconnectedState)
    {
      log(LOG_DBG3, QString(": closing"));
      tcp_sock->abort();
    }
    tcp_sock->deleteLater();
    tcp_sock = NULL;
  }
  if (pipe_sock)
  {
    emit finished((connect_timeout ? QLocalSocket::SocketTimeoutError : pipe_sock->error())+1,
                  connect_timeout ? QString("Connect timeout") : pipe_sock->errorString());
    if (pipe_sock->state() != QLocalSocket::UnconnectedState)
    {
      log(LOG_DBG3, QString(": closing"));
      pipe_sock->abort();
    }
    pipe_sock->deleteLater();
    pipe_sock = NULL;
  }
  input_buffer.clear();
  output_buffer.clear();
}

//---------------------------------------------------------------------------
void TunnelConn::log(LogPriority prio, const QString &text)
{
  prc_log(prio, QString("Tunnel '%1', conn %2").arg(tun_params.name).arg(this->id)+text);
}

