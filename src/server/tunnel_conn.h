/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef TUNNEL_CONN_H
#define TUNNEL_CONN_H

#include <QTcpSocket>
#include <QLocalSocket>
#include <QUdpSocket>
#include <QTime>
#include <QTimer>
#include "../lib/tunnel-parameters.h"
#include "../lib/tunnel-state.h"
#include "../lib/prc_log.h"

// UDP tunnel connection (application)
class TunnelUdpConn
{
public:
  TunnelConnId id;
  QTime t_created;
  QTime t_last_rcv;
  QTime t_last_snd;

//  quint64 bytes_rcv;
//  quint64 bytes_snd;
  QList<QByteArray> output_buffer;

  // for incoming UDP connection
  QHostAddress remote_addr;
  quint16 remote_port;

  // for outgoing UDP connection
  QUdpSocket *udp_sock;

  TunnelUdpConn()
  {
    id = 0;
    udp_sock = NULL;
//    bytes_rcv = 0;
//    bytes_snd = 0;
    remote_port = 0;
    t_created.start();
  }
  ~TunnelUdpConn()
  {
    if (udp_sock)
      udp_sock->deleteLater();
  }
};

// tunnel incoming connection (application)
class TunnelConn: public QObject
{
  Q_OBJECT
public:
  friend class Tunnel;
  enum Direction
  {
    INCOMING=1,
    OUTGOING=2
  } direction;

  TunnelConnId id;
  QTcpSocket *tcp_sock;
  QLocalSocket *pipe_sock;
  TunnelParameters tun_params;

  QByteArray input_buffer;
  QByteArray output_buffer;

  QTime t_connected;
  QTime t_last_rcv;
  QTime t_last_snd;

  QTimer *timer_connect;

//  quint64 bytes_rcv;
//  quint64 bytes_snd;

  bool closing_by_cmd;

  void init_incoming();
  void close(bool connect_timeout=false);
  void log(LogPriority prio, const QString &text);
  void sendOutputBuffer();
  void init_outgoing(const QString &remote_host, quint16 remote_port, quint32 connect_timeout);

  TunnelConn(Direction _direction, TunnelParameters _tun_params, QObject *parent=NULL): QObject(parent)
  {
    direction = _direction;
    tun_params = _tun_params;
    id = 0;
    tcp_sock = NULL;
    pipe_sock = NULL;
//    bytes_rcv = 0;
//    bytes_snd = 0;
    t_connected.start();
    timer_connect = new QTimer(this);
    timer_connect->setSingleShot(true);
    connect(timer_connect, SIGNAL(timeout()), this, SLOT(socket_connect_timeout()));
    closing_by_cmd = false;
    closing = false;
  }
  ~TunnelConn()
  {
    this->close();
    log(LOG_DBG4, QString(": destroyed"));
  }

signals:
  void finished(int error_code, const QString &error_str);
  void dataReceived();
  void connection_established();
  void connection_bytesWritten(qint64);

private slots:
  void socket_connected();
  void socket_disconnected();
  void tcp_socket_error(QAbstractSocket::SocketError);
  void local_socket_error(QLocalSocket::LocalSocketError);
  void socket_bytesWritten(qint64);
  void socket_readyRead();
  void socket_connect_timeout();

private:
  bool closing;
};


struct __attribute__ ((__packed__)) MgrPacket_TunConnData
{
  quint16 conn_id;

  MgrPacket_TunConnData()
  {
    conn_id = 0;
  }

};

#endif // TUNNEL_CONN_H
