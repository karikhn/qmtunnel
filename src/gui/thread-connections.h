/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef THREAD_CONNECTIONS_H
#define THREAD_CONNECTIONS_H

#include <QThread>
#include "../lib/mgrclient-conn.h"
#include "../lib/tunnel-parameters.h"
#include "../lib/tunnel-state.h"

// we need to create some abstract object in order to exchange signals/slots between main and comm threads
// this way CommThreadObject's slots will be executed in CommThread thread
class CommThreadObject: public QObject
{
  Q_OBJECT
public:
  QList<MgrClientConnection *> sockets;

  CommThreadObject(QObject *parent=NULL): QObject(parent)
  {
  }
  ~CommThreadObject()
  {
    resetConnections();
  }

  void resetConnections();

private slots:
  void gui_mgrConnChanged(quint32 conn_id, const MgrClientParameters &params);
  void gui_profileLoaded(cJSON *j_profile);
  void gui_closing();
  void slot_mgrconn_state_changed(quint16 mgr_conn_state);
  void slot_connection_error(QAbstractSocket::SocketError error);
  void slot_latency_changed(quint32 latency_ms);
  void slot_password_required();
  void slot_passphrase_required();
  void gui_mgrconn_set_password(quint32 conn_id, const QString &password);
  void gui_mgrconn_set_passphrase(quint32 conn_id, const QString &passphrase);
  void gui_mgrserver_config_get(quint32 conn_id);
  void gui_mgrserver_config_set(quint32 conn_id, cJSON *j_config);

  void gui_mgrConnDemand(quint32 conn_id);
  void gui_mgrConnDemand_disconnect(quint32 conn_id);
  void gui_mgrConnDemand_idleStart(quint32 conn_id);
  void gui_mgrConnDemand_idleStop(quint32 conn_id);

  void gui_tunnel_create(quint32 conn_id, cJSON *j_tun_params);
  void gui_tunnel_config_get(quint32 conn_id, TunnelId tun_id);
  void gui_tunnel_config_set(quint32 conn_id, cJSON *j_tun_params);
  void gui_tunnel_remove(quint32 conn_id, TunnelId tun_id);
  void gui_tunnel_state_get(quint32 conn_id, TunnelId tun_id);
  void gui_tunnel_connstate_get(quint32 conn_id, TunnelId tun_id, bool include_disconnected);
  void gui_tunnel_start(quint32 conn_id, TunnelId tun_id);
  void gui_tunnel_stop(quint32 conn_id, TunnelId tun_id);

  void packetReceived(MgrPacketCmd cmd, const QByteArray &data);

signals:
  void mgrconn_state_changed(quint32 conn_id, quint16 mgr_conn_state);
  void mgrconn_authorized(quint32 conn_id, quint64 access_flags);
  void mgrconn_socket_error(quint32 conn_id, QAbstractSocket::SocketError error);
  void mgrconn_latency_changed(quint32 conn_id, quint32 latency_ms);
  void mgrconn_config_received(quint32 conn_id, cJSON *j_config);
  void mgrconn_config_error(quint32 conn_id, quint16 res_code, const QString &config_error);
  void mgrconn_password_required(quint32 conn_id);
  void mgrconn_passphrase_required(quint32 conn_id);

  void mgrconn_tunnel_create_reply(quint32 conn_id, TunnelId orig_tun_id, TunnelId tun_id, quint16 res_code, const QString &error_str);
  void mgrconn_tunnel_config_set_reply(quint32 conn_id, TunnelId tun_id, quint16 res_code, const QString &error_str);
  void tunnel_config_received(quint32 conn_id, cJSON *j_config);
  void mgrconn_tunnel_remove(quint32 conn_id, TunnelId tun_id, quint16 res_code, const QString &error_str);
  void tunnel_state_received(quint32 conn_id, TunnelId tun_id, TunnelState state);
  void tunnel_connstate_received(quint32 conn_id, const QByteArray &data);

private:
  MgrClientConnection *connectionById(quint32 conn_id);
  MgrClientConnection *create_new_connection(const MgrClientParameters *params);

  void authRepPacketReceived(MgrClientConnection *socket, const QByteArray &req_data);
  void standartReplyPacketReceived(MgrClientConnection *socket, MgrPacketCmd cmd, const QByteArray &req_data);
  void cmd_tun_create_reply_received(MgrClientConnection *socket, const QByteArray &req_data);
  void load_profile_json_items(cJSON *j_items);

};

class CommThread: public QThread
{
  Q_OBJECT
public:

  CommThreadObject *obj;

  CommThread(): QThread()
  {
    obj = new CommThreadObject;
  }
  ~CommThread()
  {
    this->wait();
    delete obj;
  }

protected:
  void run();

};

extern CommThread *commThread;

void commThread_init();
void commThread_free();

#endif // THREAD_CONNECTIONS_H
