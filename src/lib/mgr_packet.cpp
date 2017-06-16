/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "mgr_packet.h"

//---------------------------------------------------------------------------
QString mgrPacket_cmdString(MgrPacketCmd cmd)
{
  switch(cmd)
  {
    case CMD_HEARTBEAT_REQ:            return QString("CMD_HEARTBEAT_REQ");
    case CMD_HEARTBEAT_REP:            return QString("CMD_HEARTBEAT_REP");
    case CMD_CLOSE:                    return QString("CMD_CLOSE");

    case CMD_AUTH_REQ:                 return QString("CMD_AUTH_REQ");
    case CMD_AUTH_REP:                 return QString("CMD_AUTH_REP");

    case CMD_CONFIG_GET:               return QString("CMD_CONFIG_GET");
    case CMD_CONFIG_SET:               return QString("CMD_CONFIG_SET");
    case CMD_CONFIG_SET_ERROR:         return QString("CMD_CONFIG_SET_ERROR");

    case CMD_TUN_CREATE:               return QString("CMD_TUN_CREATE");
    case CMD_TUN_CREATE_REPLY:         return QString("CMD_TUN_CREATE_REPLY");
    case CMD_TUN_CONFIG_GET:           return QString("CMD_TUN_CONFIG_GET");
    case CMD_TUN_CONFIG_SET:           return QString("CMD_TUN_CONFIG_SET");
    case CMD_TUN_REMOVE:               return QString("CMD_TUN_REMOVE");
    case CMD_TUN_STATE_GET:            return QString("CMD_TUN_STATE_GET");
    case CMD_TUN_CONNSTATE_GET:        return QString("CMD_TUN_CONNSTATE_GET");

    case CMD_TUN_START:                return QString("CMD_TUN_START");
    case CMD_TUN_STOP:                 return QString("CMD_TUN_STOP");

    case CMD_TUN_CONN_OUT_NEW:         return QString("CMD_TUN_CONN_OUT_NEW");
    case CMD_TUN_CONN_OUT_DROP:        return QString("CMD_TUN_CONN_OUT_DROP");
    case CMD_TUN_CONN_OUT_CONNECTED:   return QString("CMD_TUN_CONN_OUT_CONNECTED");
    case CMD_TUN_CONN_IN_DROP:         return QString("CMD_TUN_CONN_IN_DROP");

    case CMD_TUN_CONN_IN_DATA:         return QString("CMD_TUN_CONN_IN_DATA");
    case CMD_TUN_CONN_OUT_DATA:        return QString("CMD_TUN_CONN_OUT_DATA");

    case CMD_TUN_BUFFER_ACK:           return QString("CMD_TUN_BUFFER_ACK");
    case CMD_TUN_BUFFER_RESEND_FROM:   return QString("CMD_TUN_BUFFER_RESEND_FROM");
    case CMD_TUN_BUFFER_RESET:         return QString("CMD_TUN_BUFFER_RESET");

    case CMD_TUN_CHAIN_BROKEN:         return QString("CMD_TUN_CHAIN_BROKEN");
    case CMD_TUN_CHAIN_CHECK:          return QString("CMD_TUN_CHAIN_CHECK");
    case CMD_TUN_CHAIN_RESTORED:       return QString("CMD_TUN_CHAIN_RESTORED");

    default: return QString::number(cmd);
  }
}
