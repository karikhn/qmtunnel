/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef TUNNEL_H
#define TUNNEL_H

#include <QObject>
#include "../lib/tunnel-parameters.h"
#include "../lib/tunnel-state.h"
#include "../lib/mgrclient-conn.h"
#include "../lib/prc_log.h"
#include <QTcpServer>
#include <QLocalServer>
#include <QUdpSocket>
#include <QHostInfo>
#include <QTimer>
#include "tunnel_conn.h"

#define BUFFERED_PACKET_MIN_SIZE                          1024
#define BUFFERED_PACKETS_MAX_COUNT                       10000
#define BUFFERED_PACKETS_MIN_COUNT_BEFORE_ACK               10
#define BUFFERED_PACKETS_MIN_SIZE_BEFORE_ACK         1024*1024
#define BUFFERED_PACKETS_TIMEOUT_BEFORE_ACK               1000
#define BUFFERED_PACKET_OPTIMAL_TRANSFER_TIME             1000

typedef quint16 TunnelConnPacketId;
typedef quint32 TunnelConnPacketCount;

class Tunnel: public QObject
{
  Q_OBJECT
public:
  friend class TunnelConn;

  TunnelParameters params;                        // tunnel parameters
  TunnelState state;                              // tunnel state
  MgrClientConnection *mgrconn_in;                // incoming management connection (previous tunserver in chain or GUI)
  MgrClientConnection *mgrconn_out;               // outgoing management connection (next tunserver in chain)

  QTcpServer *bind_tcpServer;
  QUdpSocket *bind_udpSocket;
  QLocalServer *bind_pipeServer;

  QHash<TunnelConnId, TunnelConn *> in_conn_list;
  QHash<TunnelConnId, TunnelConn *> out_conn_list;
  QHash<TunnelConnId, TunnelConnInInfo> in_conn_info_list;

  QHash<TunnelConnId, TunnelUdpConn *> udp_conn_list_by_id;
  QHash<QString, TunnelUdpConn *> udp_conn_list_by_addr;            // key is ipaddr:port
  QHostAddress udp_remote_addr;
  int udp_remote_addr_lookup_id;
  bool udp_remote_addr_lookup_in_progress;
  quint16 next_udp_port;

  TunnelConnId unique_conn_id;
  QTimer *timer_failure_tolerance;

  QTime t_last_buffered_packet_ack_rcv;

  QTimer *timer_chain_heartbeat;
  QTime t_last_chain_heartbeat_req_sent;
  bool chain_heartbeat_rep_received;

  QTime t_last_state_sent;
  bool state_dispatch_queued;

  bool to_be_deleted;
  bool restart_after_stop;

  Tunnel(QObject *parent=NULL): QObject(parent)
  {
    mgrconn_out = NULL;
    mgrconn_in = NULL;
    udp_remote_addr_lookup_id = 0;
    udp_remote_addr_lookup_in_progress = false;

    restart_after_stop = false;
    state_dispatch_queued = false;

    seq_packet_id = 1;
    expected_packet_id = 1;
    last_rcv_packet_id = 0;
    next_udp_port = 0;

    to_be_deleted = false;

    chain_heartbeat_rep_received = false;

    bind_tcpServer = NULL;
    bind_udpSocket = NULL;
    bind_pipeServer = NULL;

    timer_failure_tolerance = new QTimer(this);
    timer_failure_tolerance->setSingleShot(true);
    connect(timer_failure_tolerance, SIGNAL(timeout()), this, SLOT(failure_tolerance_timedout()));

    timer_buffered_packets_ack = new QTimer(this);
    timer_buffered_packets_ack->setSingleShot(true);
    timer_buffered_packets_ack->setInterval(BUFFERED_PACKETS_TIMEOUT_BEFORE_ACK);
    connect(timer_buffered_packets_ack, SIGNAL(timeout()), this, SLOT(buffered_packets_send_ack()));

    timer_chain_heartbeat = new QTimer(this);
    connect(timer_chain_heartbeat, SIGNAL(timeout()), this, SLOT(chain_heartbeat_timeout()));

    unique_conn_id = 1;
    buffered_packets_total_len = 0;
    buffered_packets_rcv_count = 0;
    buffered_packets_rcv_total_len = 0;
    cur_data_packet_size = 4*1024;
  }
  ~Tunnel()
  {
    to_be_deleted = true;
    stop();
    log(LOG_DBG4, QString(": destroyed"));
  }

  void log(LogPriority prio, const QString &text);

  bool queueInPacket(MgrPacketCmd _cmd, TunnelConnId conn_id, TunnelConnPacketId packet_id, const QByteArray &_data=QByteArray());
  bool queueOutPacket(MgrPacketCmd _cmd, TunnelConnId conn_id, TunnelConnPacketId packet_id, const QByteArray &_data=QByteArray());
  TunnelConnPacketId next_packet_id()
  {
    TunnelConnPacketId packet_id = seq_packet_id++;
    if (packet_id == 0)
      packet_id = seq_packet_id++;
    return packet_id;
  }

  void setNewParams(TunnelParameters *new_params);

  void mgrconn_in_disconnected();
  void mgrconn_in_restored();

  void cmd_conn_out_new(TunnelConnId conn_id);
  void cmd_conn_out_drop(TunnelConnId conn_id, const QByteArray &data);
  void cmd_conn_out_connected(TunnelConnId conn_id, const QByteArray &data);
  void cmd_conn_in_drop(TunnelConnId conn_id, const QByteArray &data);
  void cmd_conn_data(TunnelConn::Direction direction, TunnelConnId conn_id, const QByteArray &data);
  void cmd_tun_buffer_ack_received(MgrClientConnection *conn, const QByteArray &data);
  void cmd_tun_buffer_resend_from(MgrClientConnection *conn, const QByteArray &data);
  void cmd_tun_create_reply_received(const QByteArray &data);
  void cmd_tun_buffer_reset();
  void cmd_tun_chain_broken(MgrClientConnection *conn);
  void cmd_tun_chain_check(MgrClientConnection *conn);
  void cmd_tun_chain_restored(MgrClientConnection *conn);
  void cmd_tun_heartbeat(MgrPacketCmd cmd, MgrClientConnection *conn);

  QByteArray in_connPrintToBuffer(bool include_disconnected=false);

  bool buffered_packet_received(TunnelConnPacketId packet_id, quint32 packet_len);

  bool forward_packet(MgrClientConnection *conn, MgrPacketCmd cmd, const QByteArray &data);

public slots:

  void start();
  void stop();
  void restart()
  {
    stop();
    if (!this->to_be_deleted)
      start();
  }

signals:
  void started();
  void stopped();

  void state_changed();

private slots:
  void mgrconn_out_state_changed(quint16 mgr_conn_state);
  void mgrconn_out_connection_error(QAbstractSocket::SocketError error);
  void mgrconn_out_packetReceived(MgrPacketCmd cmd, const QByteArray &data);

  void new_incoming_conn();
  void incoming_connection_finished(int error_code, const QString &error_str);
  void connection_dataReceived();
  void connection_bytesWritten(qint64);
  void connection_udpIncomingRead();
  void connection_udpOutgoingRead();
  void outgoing_connection_established();
  void outgoing_connection_finished(int error_code, const QString &error_str);
  void failure_tolerance_timedout();
  void udp_hostLookupFinished(const QHostInfo &hostInfo);

  void chain_heartbeat_timeout();

  void buffered_packets_send_ack();

  void mgrconn_bytesReceived(quint64 bytes);
  void mgrconn_bytesSent(quint64 bytes);
  void mgrconn_bytesSentEncrypted(quint64 encrypted_bytes);

private:
  void mgrconn_out_authRepPacketReceived(const QByteArray &req_data);

  QList<TunnelConnPacketId> buffered_packets_id_list;
  QList<QByteArray> buffered_packets_list;
  quint32 buffered_packets_total_len;

  quint32 cur_data_packet_size;

  TunnelConnPacketCount buffered_packets_rcv_count;
  quint32 buffered_packets_rcv_total_len;
  TunnelConnPacketId expected_packet_id;
  TunnelConnPacketId last_rcv_packet_id;

  QTimer *timer_buffered_packets_ack;
  QTime t_last_buffered_packet_rcv;
  TunnelConnPacketId seq_packet_id;

  void mgrconn_in_after_disconnected();
  void mgrconn_out_after_disconnected();

  void start_mgrconn_out();
  bool bind_start();
  void bind_stop();
  void close_incoming_connections();
  void close_outgoing_connections();
  void cleanup_in_conn_info_list();

};

bool tcp_check_listen_port(const QHostAddress &listen_addr, quint16 listen_port, QString &config_error);
bool udp_check_port(const QHostAddress &listen_addr, quint16 listen_port, QString &config_error);
bool named_pipe_check(const QString &path, QString &config_error);

#endif // TUNNELS_H
