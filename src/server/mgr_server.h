/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef MGR_SERVER_H
#define MGR_SERVER_H

#include <QThread>
#include <QTcpServer>
#include "../lib/mgrclient-conn.h"
#include "../lib/mgrserver-parameters.h"
#include <QtGlobal>
#include "mgrclient-state.h"
#include "tunnel.h"

#define SERVER_MAJOR_VERSION     0
#define SERVER_MINOR_VERSION     1

// we need to create some QObject-derived class in order to exchange signals/slots between main and server threads
// this way MgrServer's slots will be executed in MgrServerThread thread
class MgrServer: public QTcpServer
{
  Q_OBJECT
public:
  QList<MgrClientConnection *> mgrconn_list_in;
  QHash<MgrClientConnection *, MgrClientState *> mgrconn_state_list_in;
  QHash<quint32, User *> hash_users;
  QHash<quint32, UserGroup *> hash_user_groups;

  TunnelId unique_tunnel_id;

  QList<Tunnel *> tunnels;
  QHash<quint32, Tunnel *> hash_tunnels;
  QHash<MgrClientConnection *, Tunnel *> hash_tunnel_mgconn_in;

  MgrServerParameters params;

  MgrServer(QObject *parent=NULL): QTcpServer(parent)
  {
    unique_tunnel_id = 1;
  }
  ~MgrServer()
  {
    resetConnections();
  }

  void resetConnections();

  bool config_load();
  void config_init();
  bool config_save(MgrServerParameters *server_params);
  void initUserGroups();
  void config_load_tunnels(cJSON *j_config);
  void config_save_tunnels(cJSON *j_config);

  bool isTunnelConfigChangeAllowed(Tunnel *tunnel, User *user)
  {
    if (!user)
      return false;
    UserGroup *userGroup = userGroupById(user->group_id);
    if (!userGroup)
      return false;

    return tunnel->params.owner_user_id == user->id ||
           (userGroup->access_flags & UserGroup::AF_TUNNEL_SUPER_ADMIN) ||
          ((userGroup->access_flags & UserGroup::AF_TUNNEL_GROUP_ADMIN) && tunnel->params.owner_group_id == userGroup->id);
  }

  bool isTunnelConfigChangeAllowed(Tunnel *tunnel, MgrClientConnection *mgrconn)
  {
    return isTunnelConfigChangeAllowed(tunnel, userById(mgrconn_state_list_in[mgrconn]->user_id));
  }

  bool isTunnelControlAllowed(Tunnel *tunnel, User *user)
  {
    if (!user)
      return false;
    UserGroup *userGroup = userGroupById(user->group_id);
    if (!userGroup)
      return false;

    return tunnel->params.owner_user_id == user->id ||
           (userGroup->access_flags & UserGroup::AF_TUNNEL_SUPER_ADMIN) ||
          ((userGroup->access_flags & UserGroup::AF_TUNNEL_GROUP_ADMIN) && tunnel->params.owner_group_id == userGroup->id);
  }

  bool isTunnelControlAllowed(Tunnel *tunnel, MgrClientConnection *mgrconn)
  {
    return isTunnelControlAllowed(tunnel, userById(mgrconn_state_list_in[mgrconn]->user_id));
  }

  bool isTunnelViewAllowed(Tunnel *tunnel, User *user)
  {
    if (!user)
      return false;
    UserGroup *userGroup = userGroupById(user->group_id);
    if (!userGroup)
      return false;

    return tunnel->params.owner_user_id == user->id ||
           (userGroup->access_flags & UserGroup::AF_TUNNEL_SUPER_ADMIN) ||
          ((userGroup->access_flags & UserGroup::AF_TUNNEL_GROUP_ADMIN) && tunnel->params.owner_group_id == userGroup->id);
  }

  bool isTunnelViewAllowed(Tunnel *tunnel, MgrClientConnection *mgrconn)
  {
    return isTunnelViewAllowed(tunnel, userById(mgrconn_state_list_in[mgrconn]->user_id));
  }


public slots:
  void start();
  void stop();
  void quit();

private slots:
  void gui_config_set(cJSON *json);
  void socket_finished();
  void socket_parseInitBuffer();
  void packetReceived(MgrPacketCmd cmd, const QByteArray &data);

  void tunnel_stopped();
  void tunnel_state_changed();

signals:
  void listenError(QAbstractSocket::SocketError error, const QString &errorString);
  void config_changed(cJSON *json);
  void server_started();
  void server_stopped();

private:
  void authReqPacketReceived(MgrClientConnection *socket, const QByteArray &data);
  void config_get(MgrClientConnection *socket);
  void config_set(MgrClientConnection *socket, const QByteArray &data);

  void send_tunnel_config(MgrClientConnection *socket, Tunnel *tunnel);
  void send_tunnel_state(MgrClientConnection *socket, Tunnel *tunnel, bool include_stat=true);
  void send_tunnel_connstate(MgrClientConnection *socket, Tunnel *tunnel, bool include_disconnected=false);
  void cmd_tunnel_create(MgrClientConnection *socket, const QByteArray &data);
  void cmd_tunnel_config_get(MgrClientConnection *socket);
  void cmd_tunnel_config_set(MgrClientConnection *socket, const QByteArray &req_data);
  void cmd_tun_start(MgrClientConnection *socket, const QByteArray &data);
  void cmd_tun_stop(MgrClientConnection *socket, const QByteArray &data);
  void cmd_tun_remove(MgrClientConnection *socket, const QByteArray &data);
  void cmd_tun_state_get(MgrClientConnection *socket, const QByteArray &data);
  void cmd_tun_connstate_get(MgrClientConnection *socket, const QByteArray &data);
  void tunnel_owner_mgrconn_removed(MgrClientConnection *socket);
  void tunnel_add(Tunnel *tunnel);
  void tunnel_remove(Tunnel *tunnel);
  bool tunnel_checkParams(TunnelParameters *tun_params, quint16 &res_code, QString &error_str);
  void tunnels_start();
  void tunnels_stop();
  void tunnels_clear();
  void cmd_tun(MgrClientConnection *socket, MgrPacketCmd cmd, const QByteArray &req_data);

  UserGroup *userGroupById(quint32 group_id) const;
  User *userById(quint32 user_id) const;

protected:
#if QT_VERSION >= 0x050000
  void incomingConnection(qintptr socketDescriptor);
#else
  void incomingConnection(int socketDescriptor);
#endif
};

class MgrServerThread: public QThread
{
  Q_OBJECT
public:


  MgrServerThread(): QThread()
  {
  }
  ~MgrServerThread()
  {
    this->wait();
  }

protected:
  void run();

};

extern MgrServerThread *mgrServerThread;
extern MgrServer *mgrServer;

void mgrServerThread_init();
void mgrServerThread_free();

extern QString qmtunnel_config_filepath;

extern MgrServerParameters gui_mgrServerParams;
extern bool mgr_server_params_gui_modified;
extern bool mgr_server_params_modified_on_server_while_editing;

bool config_check(MgrServerParameters *old_params, MgrServerParameters *new_params, quint16 &res_code, QString &config_error);

void send_standart_reply(MgrClientConnection *socket, MgrPacketCmd cmd, const QByteArray &obj_id, quint16 res_code, const QString &error_str);

#endif // MGR_SERVER_H
