/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef MGR_PACKET_H
#define MGR_PACKET_H

#include <QtGlobal>
#include <QString>

enum
{
  CMD_HEARTBEAT_REQ=1,
  CMD_HEARTBEAT_REP=2,
  CMD_CLOSE=98,
  CMD_MAX_INTERNAL=99,

  CMD_AUTH_REQ=101,
  CMD_AUTH_REP=102,

  CMD_CONFIG_GET=201,
  CMD_CONFIG_SET=202,
  CMD_CONFIG_SET_ERROR=203,

  CMD_TUN_CREATE=301,
  CMD_TUN_CREATE_REPLY=302,
  CMD_TUN_CONFIG_GET=303,
  CMD_TUN_CONFIG_SET=304,
  CMD_TUN_REMOVE=305,
  CMD_TUN_STATE_GET=306,
  CMD_TUN_CONNSTATE_GET=307,

  CMD_TUN_START=311,
  CMD_TUN_STOP=312,

  CMD_TUN_CONN_OUT_NEW=321,
  CMD_TUN_CONN_OUT_DROP=322,
  CMD_TUN_CONN_OUT_CONNECTED=323,
  CMD_TUN_CONN_IN_DROP=324,

  CMD_TUN_CONN_IN_DATA=331,
  CMD_TUN_CONN_OUT_DATA=332,

  CMD_TUN_BUFFER_ACK=341,
  CMD_TUN_BUFFER_RESEND_FROM=342,
  CMD_TUN_BUFFER_RESET=343,

  CMD_TUN_CHAIN_BROKEN=351,
  CMD_TUN_CHAIN_CHECK=352,
  CMD_TUN_CHAIN_RESTORED=353,

  CMD_TUN_CHAIN_HEARTBEAT_REQ=361,
  CMD_TUN_CHAIN_HEARTBEAT_REP=362,

  CMD_MAX
};

typedef quint16 MgrPacketCmd;
typedef quint32 MgrPacketLen;

#define MGR_PACKET_FLAG_COMPRESSED             0x8000
#define MGR_PACKET_MIN_LEN_FOR_COMPRESSION        512

#define MGR_PACKET_HEADER_LEN      (int)(sizeof(MgrPacketCmd)+sizeof(MgrPacketLen))

#define MGR_PACKET_MAX_LEN         1024*512

#define MGR_PACKET_VERSION                1


struct __attribute__ ((__packed__)) MgrPacket_StandartReply
{
  quint16 res_code;
  quint16 obj_id_len;       // length of extra info (object id) associated with error
  quint16 error_len;        // length of error string

  MgrPacket_StandartReply()
  {
    res_code = 0;
    obj_id_len = 0;
    error_len = 0;
  }

};

QString mgrPacket_cmdString(MgrPacketCmd cmd);


#endif // MGR_PACKET_H
