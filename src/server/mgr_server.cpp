/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "mgr_server.h"
#include "../lib/prc_log.h"
#include "mainwindow.h"

MgrServerThread *mgrServerThread = NULL;
extern bool daemon_mode;
MgrServer *mgrServer = NULL;

//---------------------------------------------------------------------------
void mgrServerThread_init()
{
  qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");
  qRegisterMetaType<QLocalSocket::LocalSocketError>("QLocalSocket::LocalSocketError");
  qRegisterMetaType<MgrPacketCmd>("MgrPacketCmd");

  mgrServer = new MgrServer;
  mgrServer->config_load();
  gui_mgrServerParams = mgrServer->params;

  if (!daemon_mode)
  {
    mgrServerThread = new MgrServerThread;
    mgrServerThread->start();
    for (int i=0; i < mgrServer->tunnels.count(); i++)
      mgrServer->tunnels[i]->moveToThread(mgrServerThread);
    mgrServer->moveToThread(mgrServerThread);
  }
  else
  {
    mgrServer->start();
  }
}

//---------------------------------------------------------------------------
void mgrServerThread_free()
{
  if (mgrServerThread)
  {
    delete mgrServerThread;
    mgrServerThread = NULL;
  }
  if (mgrServer)
  {
    mgrServer->stop();
    delete mgrServer;
    mgrServer = NULL;
  }
}

//---------------------------------------------------------------------------
void MgrServer::resetConnections()
{
  for (int i=mgrconn_list_in.count()-1; i >= 0; i--)
  {
    if (mgrconn_list_in[i]->state() != QSslSocket::UnconnectedState)
    {
      mgrconn_list_in[i]->abort();
    }
  }
  while (!mgrconn_list_in.isEmpty())
  {
    MgrClientConnection *socket = mgrconn_list_in.takeFirst();
    delete mgrconn_state_list_in.take(socket);
    delete socket;
  }
}

//---------------------------------------------------------------------------
void MgrServer::packetReceived(MgrPacketCmd cmd, const QByteArray &data)
{
  MgrClientConnection *socket = qobject_cast<MgrClientConnection *>(sender());
  if (!socket)
    return;

  switch (cmd)
  {
    case CMD_AUTH_REQ:
      authReqPacketReceived(socket, data);
      break;
    case CMD_CONFIG_GET:
      config_get(socket);
      break;
    case CMD_CONFIG_SET:
      config_set(socket, data);
      break;
    case CMD_TUN_CREATE:
    case CMD_TUN_CONFIG_GET:
    case CMD_TUN_CONFIG_SET:
    case CMD_TUN_REMOVE:
    case CMD_TUN_CHAIN_HEARTBEAT_REQ:
    case CMD_TUN_CHAIN_HEARTBEAT_REP:
    case CMD_TUN_STATE_GET:
    case CMD_TUN_CONNSTATE_GET:
    case CMD_TUN_START:
    case CMD_TUN_STOP:
    case CMD_TUN_BUFFER_ACK:
    case CMD_TUN_CHAIN_BROKEN:
    case CMD_TUN_CHAIN_CHECK:
    case CMD_TUN_BUFFER_RESEND_FROM:
    case CMD_TUN_CONN_OUT_NEW:
    case CMD_TUN_CONN_OUT_DROP:
    case CMD_TUN_CONN_OUT_CONNECTED:
    case CMD_TUN_CONN_IN_DROP:
    case CMD_TUN_CONN_IN_DATA:
    case CMD_TUN_CONN_OUT_DATA:
      cmd_tun(socket, cmd, data);
      break;
    default:
      socket->log(LOG_DBG1, QString(": Unknown packet cmd %1 - dropping connection").arg(cmd));
      socket->abort();
      break;
  }
}

//---------------------------------------------------------------------------
void send_standart_reply(MgrClientConnection *socket, MgrPacketCmd cmd, const QByteArray &obj_id, quint16 res_code, const QString &error_str)
{
  QByteArray error_buf = error_str.toUtf8();
  MgrPacket_StandartReply error_packet;
  error_packet.res_code = res_code;
  error_packet.obj_id_len = obj_id.length();
  error_packet.error_len = error_buf.length();
  QByteArray buffer;
  buffer.reserve(sizeof(MgrPacket_StandartReply)+obj_id.length()+error_buf.length());
  buffer = QByteArray((const char *)&error_packet, sizeof(MgrPacket_StandartReply));
  if (!obj_id.isEmpty())
    buffer.append(obj_id);
  if (!error_str.isEmpty())
    buffer.append(error_buf);
  socket->sendPacket(cmd, buffer);
}

//---------------------------------------------------------------------------
void MgrServer::start()
{
  if (params.max_incoming_mgrconn > 0)
    setMaxPendingConnections(params.max_incoming_mgrconn);
  if (!QTcpServer::listen(params.listen_interface, params.listen_port))
  {
    prc_log(LOG_CRITICAL, QString("Failed to start MgrServer on %1:%2: ").arg(params.listen_interface.toString()).arg(params.listen_port)+errorString());
    emit listenError(serverError(), errorString());
    return;
  }
  initUserGroups();
  prc_log(LOG_LOW, QString("MgrServer on %1:%2 started").arg(params.listen_interface.toString()).arg(params.listen_port));
  tunnels_start();
  emit server_started();
}

//---------------------------------------------------------------------------
void MgrServer::stop()
{
  if (QTcpServer::isListening())
  {
    QTcpServer::close();
    prc_log(LOG_LOW, QString("MgrServer on %1:%2 stopped").arg(params.listen_interface.toString()).arg(params.listen_port));
  }
  tunnels_clear();
  resetConnections();
  emit server_stopped();
}

//---------------------------------------------------------------------------
void MgrServer::quit()
{
  stop();
  if (mgrServerThread)
    mgrServerThread->quit();
}

//---------------------------------------------------------------------------
void MgrServer::socket_finished()
{
  MgrClientConnection *socket = qobject_cast<MgrClientConnection *>(sender());
  if (!socket)
    return;
  tunnel_owner_mgrconn_removed(socket);
  mgrconn_list_in.removeOne(socket);
  delete mgrconn_state_list_in.take(socket);
  socket->deleteLater();
}

//---------------------------------------------------------------------------
void MgrServerThread::run()
{
  prc_log(LOG_DBG2, QString("MgrServer thread started"));
//  prc_log(LOG_DBG3, QString("MgrServer thread 0x%1").arg((long long)QThread::currentThread(), 0, 16));
  // if running in GUI mode, we want to wait a bit while GUI is starting
  if (!daemon_mode)
  {
    while (!mainWindow)
      msleep(10);
  }
  while (mgrServer->thread() != QThread::currentThread())
    msleep(10);
  mgrServer->start();
  this->exec();
  mgrServer->stop();
  prc_log(LOG_DBG2, QString("MgrServer thread stopped"));
}
