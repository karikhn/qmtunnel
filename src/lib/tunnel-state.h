/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef TUNNELSTATE_H
#define TUNNELSTATE_H

#include <QString>
#include <QTime>
#include <QHostAddress>
#include "../lib/cJSON.h"
#include "../lib/mgrclient-parameters.h"
#include "../lib/tunnel-parameters.h"

typedef quint16 TunnelConnId;

class TunnelConnInInfo
{
public:
  quint64 bytes_rcv;
  quint64 bytes_snd;
  QDateTime t_connected;
  QDateTime t_disconnected;
  quint16 peer_port;
  quint16 tun_out_port;
  QHostAddress peer_address;
  QByteArray peer_name;
  quint8 flags;
  enum {
    FL_OUT_CONNECTED      = 0x01
  };
  QByteArray errorstring;

  TunnelConnInInfo()
  {
    bytes_rcv = 0;
    bytes_snd = 0;
    peer_port = 0;
    tun_out_port = 0;
    flags = 0;
  }

  TunnelConnInInfo(const TunnelConnInInfo &src)
  {
    bytes_rcv = src.bytes_rcv;
    bytes_snd = src.bytes_snd;
    t_connected = src.t_connected;
    t_disconnected = src.t_disconnected;
    peer_port = src.peer_port;
    tun_out_port = src.tun_out_port;
    peer_address = src.peer_address;
    peer_name = src.peer_name;
    flags = src.flags;
    errorstring = src.errorstring;
  }

  int parseFromBuffer(const char *buffer, int buf_size);
  QByteArray printToBuffer() const;

};

struct __attribute__ ((__packed__)) TunnelStatistics
{
public:
  quint64 bytes_rcv;                   // number of total bytes received
  quint64 bytes_snd;                   // number of total bytes sent
  quint64 bytes_snd_encrypted;         // number of total encrypted bytes sent
  quint64 data_bytes_rcv;              // number of total tunnel connection data (application) bytes received
  quint64 data_bytes_snd;              // number of total tunnel connection data (application) bytes sent
  quint32 conn_total_count;            // number of total tunnel incoming connections
  quint32 conn_cur_count;              // number of current tunnel incoming connections
  quint32 conn_failed_count;           // number of failed tunnel outgoing connections
  quint32 chain_error_count;           // number of total registered tunnel chain disconnections/errors

  TunnelStatistics()
  {
    bytes_rcv = 0;
    bytes_snd = 0;
    bytes_snd_encrypted = 0;
    data_bytes_rcv = 0;
    data_bytes_snd = 0;
    conn_total_count = 0;
    conn_cur_count = 0;
    conn_failed_count = 0;
    chain_error_count = 0;
  }

  TunnelStatistics(const TunnelStatistics &src)
  {
    bytes_rcv = src.bytes_rcv;
    bytes_snd = src.bytes_snd;
    bytes_snd_encrypted = src.bytes_snd_encrypted;
    data_bytes_rcv = src.data_bytes_rcv;
    data_bytes_snd = src.data_bytes_snd;
    conn_total_count = src.conn_total_count;
    conn_cur_count = src.conn_cur_count;
    conn_failed_count = src.conn_failed_count;
    chain_error_count = src.chain_error_count;
  }
};

//---------------------------------------------------------------------------
class TunnelState
{
public:
  quint16 last_error_code;
  enum
  {
    RES_CODE_OK = 0
   ,RES_CODE_NAME_ERROR = 1
   ,RES_CODE_BIND_ERROR = 2
   ,RES_CODE_REMOTE_HOST_ERROR = 3
   ,RES_CODE_MGRCONN_FAILED = 4
   ,RES_CODE_BUFFER_OVERFLOW = 5
   ,RES_CODE_TUNNEL_NOT_FOUND = 6
   ,RES_CODE_PERMISSION_DENIED = 7
  };
  QString last_error_str;

  qint32 latency_ms;

  TunnelStatistics stats;

  quint64 flags;
  enum
  {
    TF_CHECK_PASSED               = 0x00000001  // configuration and first setup test passed ok
   ,TF_MGRCONN_IN_CONNECTED       = 0x00000002  // connected to previous tunserver in chain
   ,TF_MGRCONN_OUT_CONNECTED      = 0x00000004  // connected to next tunserver in chain
   ,TF_SAVED_IN_CONFIG            = 0x00000008  // tunnel's most recent configuration has been saved in config file
   ,TF_BINDING                    = 0x00000010  // bind is running
   ,TF_MGRCONN_OUT_CLEAR_TO_SEND  = 0x00000020  // can send connection&data packets to mgrconn_out
   ,TF_MGRCONN_IN_CLEAR_TO_SEND   = 0x00000040  // can send connection&data packets to mgrconn_in
   ,TF_CHAIN_OK                   = 0x00000080  // tunnel chain is established
   ,TF_STOPPING                   = 0x00000100  // tunnel is stopping
   ,TF_IDLE                       = 0x00000200  // tunnel is idle (e.g. binding but chain not established)
   ,TF_STARTED                    = 0x00000800  // tunnel is started
  };

  TunnelState()
  {
    flags = 0;
    latency_ms = -1;
    last_error_code = RES_CODE_OK;
  }

  TunnelState(const TunnelState &src)
  {
    flags = src.flags;
    latency_ms = src.latency_ms;
    last_error_str = src.last_error_str;
    last_error_code = src.last_error_code;
    stats = src.stats;
  }

  bool parseFromBuffer(const char *buffer, unsigned int buf_size);
  QByteArray printToBuffer(bool include_stat=true) const;
};


struct __attribute__ ((__packed__)) MgrPacket_TunnelCreateReply
{
  TunnelId orig_tunnel_id;           // original tunnel id that came in CMD_TUN_CREATE tun_params.id
  TunnelId tunnel_id;                // created tunnel id unique within current (sender of this packet) tunserver
  quint16 res_code;                  // result code TunnelState::RES_CODE...
  quint16 error_len;                 // length of error string if any

  MgrPacket_TunnelCreateReply()
  {
    orig_tunnel_id = 0;
    tunnel_id = 0;
    res_code = 0;
    error_len = 0;
  }

};

Q_DECLARE_METATYPE(TunnelState*);

#endif // TUNNELSTATE_H
