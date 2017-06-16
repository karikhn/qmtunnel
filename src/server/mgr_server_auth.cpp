/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "mgr_server.h"
#include "../lib/prc_log.h"
#include "../lib/sys_util.h"

//---------------------------------------------------------------------------
void MgrServer::authReqPacketReceived(MgrClientConnection *socket, const QByteArray &req_data)
{
  if (req_data.length() < (int)sizeof(MgrPacket_AuthReq))
  {
    socket->log(LOG_DBG1, QString(": MgrPacket_AuthReq packet too short"));
    socket->abort();
    return;
  }
  MgrPacket_AuthReq *req = (MgrPacket_AuthReq *)req_data.data();
  if (req_data.length() < (int)sizeof(MgrPacket_AuthReq)+req->hostname_len+req->username_len+req->password_len)
  {
    socket->log(LOG_DBG1, QString(": MgrPacket_AuthReq packet too short"));
    socket->abort();
    return;
  }

  socket->protocol_version = req->protocol_version;
  socket->params.flags = req->flags;
  socket->params.ping_interval = req->ping_interval;
  socket->params.name = socket->peer_hostname = QString::fromUtf8(req_data.mid(sizeof(MgrPacket_AuthReq),req->hostname_len));
  socket->params.auth_username = QString::fromUtf8(req_data.mid(sizeof(MgrPacket_AuthReq)+req->hostname_len,req->username_len));
  socket->params.auth_password = QString::fromUtf8(req_data.mid(sizeof(MgrPacket_AuthReq)+req->hostname_len+req->username_len,req->password_len));

  MgrPacket_AuthRep rep;
  rep.server_hostname_len = SysUtil::machine_name.toUtf8().length();
  if (req->protocol_version > MGR_PACKET_VERSION)
    rep.auth_result = MgrPacket_AuthRep::RES_CODE_UNSUPPORTED_PROTOCOL_VERSION;
  else
  {
    rep.auth_result = MgrPacket_AuthRep::RES_CODE_AUTH_FAILED;

    for (int i=0; i < params.users.count(); i++)
    {
      if (params.users[i].enabled && (!params.users[i].cert.isNull() || !params.users[i].password.isEmpty()) &&
          ((params.users[i].cert.isNull() || params.users[i].cert == socket->peerCertificate()) &&
           (params.users[i].password.isEmpty() || (params.users[i].password == socket->params.auth_password && params.users[i].name == socket->params.auth_username))))
      {
        UserGroup *user_group = userGroupById(params.users[i].group_id);
        if (!user_group)
          continue;
        rep.auth_result = MgrPacket_AuthRep::RES_CODE_OK;
        rep.access_flags = user_group->access_flags;
        socket->params.auth_username = params.users[i].name;
        mgrconn_state_list_in[socket]->user_id = params.users[i].id;
        break;
      }
    }
  }

  QByteArray rep_data((const char *)&rep, sizeof(MgrPacket_AuthRep));
  if (SysUtil::machine_name.length() > 0)
    rep_data.append(SysUtil::machine_name.toUtf8());

  socket->sendPacket(CMD_AUTH_REP, rep_data);
  if (rep.auth_result != MgrPacket_AuthRep::RES_CODE_OK)
  {
    socket->log(LOG_DBG1, QString(": user %1 login failed from %2").arg(socket->params.auth_username).arg(socket->peer_hostname));
    socket->disconnectFromHost();
  }
  else
  {
    socket->socket_startOperational();
    socket->log(LOG_DBG1, QString(": user %1 logged in from %2").arg(socket->params.auth_username).arg(socket->peer_hostname));
  }
}

//---------------------------------------------------------------------------
void MgrServer::initUserGroups()
{
  hash_user_groups.clear();
  for (int i=0; i < params.user_groups.count(); i++)
    hash_user_groups.insert(params.user_groups[i].id, &params.user_groups[i]);

  hash_users.clear();
  for (int i=0; i < params.users.count(); i++)
    hash_users.insert(params.users[i].id, &params.users[i]);
}

//---------------------------------------------------------------------------
UserGroup *MgrServer::userGroupById(quint32 group_id) const
{
  return hash_user_groups.value(group_id, NULL);
}

//---------------------------------------------------------------------------
User *MgrServer::userById(quint32 user_id) const
{
  return hash_users.value(user_id, NULL);
}
