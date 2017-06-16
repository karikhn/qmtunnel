/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef MGRCLIENT_CONN_H
#define MGRCLIENT_CONN_H

#include <QSslSocket>
#include <QTimer>
#include <QTime>
#include "mgrclient-parameters.h"
#include "mgr_packet.h"
#include "prc_log.h"

#define PHASE_INIT_TIMEOUT                          3000
#define PHASE_INIT_ENCRYPT_CMD             "ENCRYPT\r\n"
#define PHASE_INIT_DECRYPT_CMD             "DECRYPT\r\n"

#define PHASE_SSL_HANDSHAKE_TIMEOUT                 3000

#define PHASE_AUTH_TIMEOUT                          3000

class MgrClientConnection: public QSslSocket
{
  Q_OBJECT
public:
  MgrClientParameters params;       // qmtunnel host management connection parameters
  QString private_key_passphrase;

  enum ConnDirection
  {
    INCOMING=0, OUTGOING=1
  } direction;

  MgrClientConnection(ConnDirection dir, QObject *parent=NULL): QSslSocket(parent)
  {
    direction = dir;
    connect(this, SIGNAL(encrypted()), this, SLOT(socket_encrypted()));
    connect(this, SIGNAL(connected()), this, SLOT(socket_connected()));
    connect(this, SIGNAL(disconnected()), this, SLOT(socket_disconnected()));
    connect(this, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(socket_error(QAbstractSocket::SocketError)));
    connect(this, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(socket_sslError(QList<QSslError>)));
    connect(this, SIGNAL(bytesWritten(qint64)), this, SLOT(socket_bytesWritten(qint64)));
    connect(this, SIGNAL(encryptedBytesWritten(qint64)), this, SLOT(socket_encryptedBytesWritten(qint64)));
    connect(this, SIGNAL(readyRead()), this, SLOT(socket_readyRead()));

    this->setPeerVerifyMode(QSslSocket::QueryPeer);

    // timer for heartbeat send
    timer_heartbeat = new QTimer(this);
    timer_heartbeat->setSingleShot(false);
    connect(timer_heartbeat, SIGNAL(timeout()), this, SLOT(socket_heartbeat()));

    // timer for heartbeat check
    timer_heartbeat_check = new QTimer(this);
    timer_heartbeat_check->setSingleShot(false);
    connect(timer_heartbeat_check, SIGNAL(timeout()), this, SLOT(socket_heartbeat_check()));

    // timer for phase timeout
    timer_phase = new QTimer(this);
    timer_phase->setSingleShot(true);
    connect(timer_phase, SIGNAL(timeout()), this, SLOT(socket_phase_timeout()));

    // timer for idle timeout (this one should start/restart in application!)
    timer_idle = new QTimer(this);
    timer_idle->setSingleShot(true);
    connect(timer_idle, SIGNAL(timeout()), this, SLOT(socket_idle_timeout()));

    if (dir == OUTGOING)
    {
      // timer for connect timeout
      timer_connect = new QTimer(this);
      timer_connect->setSingleShot(true);
      connect(timer_connect, SIGNAL(timeout()), this, SLOT(socket_connect_timeout()));

      // timer for reconnect
      timer_reconnect = new QTimer(this);
      timer_reconnect->setSingleShot(true);
      connect(timer_reconnect, SIGNAL(timeout()), this, SLOT(beginConnection()));
    }
    else
    {
      timer_connect = NULL;
      timer_reconnect = NULL;
    }

    bytes_rcv = 0;
    bytes_snd = 0;
    bytes_rcv_encrypted = 0;
    bytes_snd_encrypted = 0;
    latency = 0;
    phase = PHASE_NONE;
    protocol_version = MGR_PACKET_VERSION;
    max_bytes_to_read_at_once = (MGR_PACKET_MAX_LEN+sizeof(MgrPacketCmd)+sizeof(MgrPacketLen))*2;
    max_processing_time_ms = 50;
    closing_by_cmd_close = false;
  }
  ~MgrClientConnection()
  {
    this->log(LOG_DBG4, QString(": destroyed"));
    if (this->state() != QSslSocket::UnconnectedState)
      this->abort();
  }

  QByteArray input_buffer;
  QByteArray output_buffer;

  QTime t_connected;
  QTime t_last_rcv;
  QTime t_last_snd;
  QTime t_last_check;
  QTime t_last_heartbeat_req;

  quint64 bytes_rcv;
  quint64 bytes_snd;
  quint64 bytes_rcv_encrypted;
  quint64 bytes_snd_encrypted;
  quint32 latency;

  QTimer *timer_connect;
  QTimer *timer_heartbeat;
  QTimer *timer_heartbeat_check;
  QTimer *timer_reconnect;
  QTimer *timer_phase;
  QTimer *timer_idle;
  bool closing_by_cmd_close;

  quint32 max_bytes_to_read_at_once;      // limit amount of data to read from socket at once in readyRead() (0 = no limit)
  quint32 max_processing_time_ms;         // limit time used for processing of received packets in readyRead() (0 = no limit)

  quint8 protocol_version;                // version of MgrPacket/MgrClientConnection protocol
  QString peer_hostname;                  // peer (server or client) hostname reported by itself

  QString log_prefix;

  enum ConnPhase { PHASE_NONE=0,             // Connection is not established yet
                   PHASE_INIT=1,             // Connection established, sending HTTP request and processing HTTP replies
                   PHASE_SSL_HANDSHAKE=2,    // SSL handshake is in progress
                   PHASE_AUTH=3,             // Authorization is in progress
                   PHASE_OPERATIONAL=4,      // Successfully authorized and fully operational
                 } phase;

  enum MgrConnState { MGR_NONE=0          // there was no connection attempt (manual connection?), idle
                     ,MGR_CONNECTING      // trying to connect right now
                     ,MGR_AUTH            // authentication in progress
                     ,MGR_CONNECTED       // connection established and authenticated
                     ,MGR_ERROR           // connection or authentication error
                      };

  void init();
  void socket_startOperational();
  void setParameters(const MgrClientParameters *new_params);
  bool sendPacket(MgrPacketCmd _cmd, const QByteArray &_data=QByteArray());
  void sendOutputBuffer();

  void log(LogPriority prio, const QString &text);

signals:
  void state_changed(quint16 mgr_conn_state);
  void connection_error(QAbstractSocket::SocketError error);
  void socket_finished();
  void init_inputParsing();
  void packetReceived(MgrPacketCmd cmd, const QByteArray &data);
  void latency_changed(quint32 latency_ms);
  void password_required();
  void passphrase_required();
  void output_buffer_empty();

  void stat_bytesReceived(quint64 bytes);
  void stat_bytesSent(quint64 bytes);
  void stat_bytesSentEncrypted(quint64 encrypted_bytes);

private:

  QByteArray socket_read(quint64 max_len);

  void socket_send_auth();

private slots:
  void socket_encrypted();
  void socket_disconnected();
  void socket_error(QAbstractSocket::SocketError error);
  void socket_sslError(const QList<QSslError> &errors);
  void socket_bytesWritten(qint64 bytes);
  void socket_encryptedBytesWritten(qint64 bytes);
  void socket_readyRead();
  void socket_connect_timeout();
  void socket_initiate_reconnect();
  void socket_phase_timeout();
  void socket_heartbeat();
  void socket_heartbeat_check();
  void socket_idle_timeout();

  void parseInputBuffer();

public slots:
  void beginConnection();

  void socket_connected();
};

struct __attribute__ ((__packed__)) MgrPacket_AuthReq
{
  quint8 protocol_version;           // version of MgrPacket/MgrClientConnection protocol
  quint8 hostname_len;               // length of client hostname (to follow)
  quint8 username_len;               // length of client username (to follow)
  quint8 password_len;               // length of client password (to follow)

  quint32 ping_interval;              // Interval between heartbeat packets (ms)

  quint32 flags;                      // Additional flags, such as:
  /*
  enum
  {
    FL_USE_COMPRESSION                            = 0x00000001      // compress data when it's not too small
   ,FL_TCP_KEEP_ALIVE                             = 0x00000002      // enable tcp keepalive option
   ,FL_TCP_NO_DELAY                               = 0x00000004      // disable Nagle's algorithm (enable TCP_NODELAY option)
   ,FL_ENABLE_HEARTBEATS                          = 0x00000008      // enable heartbeats (to measure latency and keep connection alive) - STRONGLY RECOMMENDED
  };
  */
  MgrPacket_AuthReq()
  {
    protocol_version = MGR_PACKET_VERSION;
    hostname_len = 0;
    username_len = 0;
    password_len = 0;
    ping_interval = 0;
    flags = 0;
  }

};

struct __attribute__ ((__packed__)) MgrPacket_AuthRep
{
  enum
  {
    RES_CODE_OK = 0,
    RES_CODE_UNSUPPORTED_PROTOCOL_VERSION = 1,
    RES_CODE_AUTH_FAILED = 2
  };
  quint8 auth_result;
  quint8 server_hostname_len;        // length of server hostname (to follow)
  quint64 access_flags;              // Access rights flags

  MgrPacket_AuthRep()
  {
    auth_result = RES_CODE_AUTH_FAILED;
    server_hostname_len = 0;
    access_flags = 0;
  }

};


#endif // MGRCLIENT_CONN_H
