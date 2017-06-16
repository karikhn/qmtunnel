/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef CLIENTCONNPARAMETERS_H
#define CLIENTCONNPARAMETERS_H

#include <QString>
#include "../lib/cJSON.h"

class ClientConnParameters
{
public:
  quint32 id;                         // Unique connection id
  QString name;                       // User-defined connection name
  QString host;                       // hostname or IP-address of qmtunnel host
  quint16 port;                       // TCP port number (qmtunnel protocol port)

  enum ConnDirection
  {
    INCOMING=0, OUTGOING=1
  } direction;
  enum ConnType { CONN_PERMANENT=1, CONN_DEMAND=2 } conn_type;  // Connection type for outgoing connections

  quint32 conn_timeout;               // Connection timeout (ms)
  quint32 idle_timeout;               // Connection idle timeout (ms)
  quint32 rcv_timeout;                // If no data received during this timeout (ms), consider connection dead
  quint32 reconnect_interval;         // Reconnect interval (ms)
  quint32 reconnect_interval_increment;      // If connect fails - increase reconnect_interval (ms)
  double reconnect_interval_multiplicator;   // If connect fails - multiply reconnect_interval (ms)
  quint32 reconnect_interval_max;            // If connect fails - maximum value of reconnect_interval (ms)

  quint32 io_buffer_size;             // Maximum read/write buffer size in bytes (0=unlimited)

  quint32 flags;                      // Additional flags, such as:
  enum
  {
    FL_TCP_KEEP_ALIVE                             = 0x00000002      // enable tcp keepalive option
   ,FL_TCP_NO_DELAY                               = 0x00000004      // disable Nagle's algorithm (enable TCP_NODELAY option)
  };

  ClientConnParameters()
  {
    id = 0;
    port = 0;
    direction = INCOMING;
    conn_type = CONN_DEMAND;
    conn_timeout = 5000;
    idle_timeout = 0;
    rcv_timeout = 5*60*1000;
    reconnect_interval = 1000;
    reconnect_interval_increment = 0;
    reconnect_interval_multiplicator = 1;
    reconnect_interval_max = 60*60*24*1000;
    io_buffer_size = 1024*1024*64;
    flags = FL_TCP_KEEP_ALIVE;
  }

  ClientConnParameters(const ClientConnParameters &src)
  {
    id = src.id;
    name = src.name;
    host = src.host;
    port = src.port;
    direction = src.direction;
    conn_type = src.conn_type;
    conn_timeout = src.conn_timeout;
    idle_timeout = src.idle_timeout;
    rcv_timeout = src.rcv_timeout;
    reconnect_interval = src.reconnect_interval;
    reconnect_interval_increment = src.reconnect_interval_increment;
    reconnect_interval_multiplicator = src.reconnect_interval_multiplicator;
    reconnect_interval_max = src.reconnect_interval_max;
    flags = src.flags;
    io_buffer_size = src.io_buffer_size;
  }

  void parseJSON(cJSON *json);
  void printJSON(cJSON *json) const;
};

#endif // CLIENTCONNPARAMETERS_H
