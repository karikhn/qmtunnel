/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef TUNNELPARAMETERS_H
#define TUNNELPARAMETERS_H

#include <QString>
#include <QStringList>
#include "../lib/cJSON.h"
#include "../lib/mgrclient-parameters.h"
#include "../lib/mgr_packet.h"

typedef quint16 TunnelId;

class TunnelParameters
{
public:
  TunnelId orig_id;                     // Original id (came from previous server or owner) for reference
  TunnelId id;                          // Unique id
  TunnelId next_id;                     // Next tunserver's tunnel id (came from next server in chain) for reference
  QString name;                        // User-defined tunnel name
  enum FwdDirection { LOCAL_TO_REMOTE=1, REMOTE_TO_LOCAL=2 } fwd_direction;  // Forwarding direction
  enum AppProtocol { TCP=1, UDP=2, PIPE=3 } app_protocol;  // Connection protocol

  QList<MgrClientParameters> tunservers;  // list (chain) of tunservers used

  QString bind_address;                // local bind address, for pipe - pipe name
  quint16 bind_port;                   // local bind port number, for pipe - ignored

  quint16 udp_port_range_from;         // local UDP (outgoing) bind port range
  quint16 udp_port_range_till;         // local UDP (outgoing) bind port range

  QString remote_host;                 // remote host, for pipe - pipe name
  quint16 remote_port;                 // remote host port number, for pipe - ignored

  quint32 idle_timeout;                // idle timeout (ms)
  quint32 owner_user_id;               // owner user id
  quint32 owner_group_id;              // owner group id

  quint32 max_incoming_connections;    // maximum number of incoming TCP or PIPE connections
  quint32 max_bytes_to_read_at_once;   // limit amount of data to read from socket at once in readyRead() (0 = no limit)
  quint32 max_io_buffer_size;          // Maximum read/write buffer size in bytes (0=unlimited)
  quint32 read_buffer_size;            // Maximum read buffer size in bytes (0=unlimited)
  quint32 write_buffer_size;           // Maximum write buffer size in bytes (0=unlimited)
  quint32 max_data_packet_size;        // Maximum size of packet with data (0=maximum packet size)
  quint32 failure_tolerance_timeout;   // Keep data buffered in case of short-time failure of tunserver-tunserver mgrconn (ms)
  quint32 connect_timeout;             // Connect timeout for outgoing application connections (ms)
  quint32 heartbeat_interval;          // Tunnel heartbeat (latency check) interval (ms) (0 - no heartbeat and latency check)

  quint32 max_state_dispatch_frequency;// Maximum frequency (ms) of sending out tunnel state updates to subscribers
  quint32 max_incoming_connections_info;// Maximum number of items in incoming connections info list
  quint32 incoming_connections_info_timeout;// Maximum number of seconds to keep closed incoming connection info

  quint32 flags;                       // Additional flags, such as:
  enum
  {
    FL_SAVE_IN_CONFIG_PERMANENTLY      = 0x00000001      // save this tunnel in configuration so it would not be lost after server restart
   ,FL_PERMANENT_TUNNEL                = 0x00000002      // always try to keep tunnel up and running
   ,FL_MASTER_TUNSERVER                = 0x00000004      // this tunserver is the first one in chain (master/owner)
   ,FL_TCP_NO_DELAY                    = 0x00000010
   ,FL_TCP_KEEP_ALIVE                  = 0x00000020
   ,FL_CONNIN_REVERSE_DNS              = 0x00000040      // (DNS reverse) lookup peer name for incoming connections (not implemented yet)
  };

  TunnelParameters()
  {
    id = 0;
    orig_id = 0;
    next_id = 0;
    max_incoming_connections = 100;
    max_bytes_to_read_at_once = (MGR_PACKET_MAX_LEN-sizeof(quint16))*2;
    connect_timeout = 10*1000;
    failure_tolerance_timeout = 1000;
    max_io_buffer_size = 1024*1024*32;
    read_buffer_size = 1024*1024*8;
    write_buffer_size = 1024*1024*8;
    max_data_packet_size = 0;
    owner_user_id = 0;
    owner_group_id = 0;
    fwd_direction = LOCAL_TO_REMOTE;
    app_protocol = TCP;
    bind_port = 8080;
    remote_port = 80;
    heartbeat_interval = 60*1000;
    udp_port_range_from = 33001;
    udp_port_range_till = 63000;
    idle_timeout = 300000;
    flags = FL_SAVE_IN_CONFIG_PERMANENTLY
          | FL_MASTER_TUNSERVER;
    max_state_dispatch_frequency = 250;
    max_incoming_connections_info = 200;
    incoming_connections_info_timeout = 60*60;
  }

  TunnelParameters(const TunnelParameters &src)
  {
    id = src.id;
    orig_id = src.orig_id;
    next_id = src.next_id;
    max_incoming_connections = src.max_incoming_connections;
    max_bytes_to_read_at_once = src.max_bytes_to_read_at_once;
    connect_timeout = src.connect_timeout;
    max_io_buffer_size = src.max_io_buffer_size;
    read_buffer_size = src.read_buffer_size;
    write_buffer_size = src.write_buffer_size;
    max_data_packet_size = src.max_data_packet_size;
    failure_tolerance_timeout = src.failure_tolerance_timeout;
    name = src.name;
    owner_user_id = src.owner_user_id;
    owner_group_id = src.owner_group_id;
    fwd_direction = src.fwd_direction;
    app_protocol = src.app_protocol;
    tunservers = src.tunservers;
    bind_address = src.bind_address;
    bind_port = src.bind_port;
    remote_host = src.remote_host;
    remote_port = src.remote_port;
    udp_port_range_from = src.udp_port_range_from;
    udp_port_range_till = src.udp_port_range_till;
    idle_timeout = src.idle_timeout;
    flags = src.flags;
    max_state_dispatch_frequency = src.max_state_dispatch_frequency;
    max_incoming_connections_info = src.max_incoming_connections_info;
    incoming_connections_info_timeout = src.incoming_connections_info_timeout;
  }

  void parseJSON(cJSON *json);
  void printJSON(cJSON *json) const;
  bool operator ==(const TunnelParameters &src) const { return isEquivalTo(src); }
  bool operator !=(const TunnelParameters &src) const { return !isEquivalTo(src); }

  bool needRestart(const TunnelParameters &old_params) const
  {
    return !isEquivalTo(old_params);
  }

  QString tunnelRule() const
  {
    QString tunnel_rule;
    if (fwd_direction == TunnelParameters::LOCAL_TO_REMOTE)
    {
      tunnel_rule += QString("L#");
      tunnel_rule += bind_address;
      if (app_protocol != TunnelParameters::PIPE)
        tunnel_rule += QString(":%1").arg(bind_port);
      tunnel_rule += QString("#");
      for (int i=0; i < tunservers.count(); i++)
      {
        tunnel_rule += tunservers[i].host;
        if (tunservers[i].port != DEFAULT_MGR_PORT)
          tunnel_rule += QString(":%1").arg(tunservers[i].port);
        tunnel_rule += QString("#");
      }
      tunnel_rule += remote_host;
      if (app_protocol != TunnelParameters::PIPE)
        tunnel_rule += QString(":%1").arg(remote_port);
    }
    else if (fwd_direction == TunnelParameters::REMOTE_TO_LOCAL)
    {
      tunnel_rule += QString("R#");
      tunnel_rule += remote_host;
      if (app_protocol != TunnelParameters::PIPE)
        tunnel_rule += QString(":%1").arg(remote_port);
      tunnel_rule += QString("#");
      for (int i=0; i < tunservers.count(); i++)
      {
        tunnel_rule += tunservers[i].host;
        if (tunservers[i].port != DEFAULT_MGR_PORT)
          tunnel_rule += QString(":%1").arg(tunservers[i].port);
        tunnel_rule += QString("#");
      }
      tunnel_rule += bind_address;
      if (app_protocol != TunnelParameters::PIPE)
        tunnel_rule += QString(":%1").arg(bind_port);
    }
    return tunnel_rule;
  }

  bool matchesRule(const QString &rule) const
  {
    QStringList subrules = rule.split("#");
    if (subrules.isEmpty())
      return true;

    // forward direction
    QString subrule = subrules.first().trimmed();
    if (!subrule.isEmpty())
    {
      if ((subrule == QString("L") && fwd_direction != LOCAL_TO_REMOTE) ||
          (subrule == QString("R") && fwd_direction != REMOTE_TO_LOCAL))
        return false;
    }
    subrules.removeFirst();
    if (subrules.isEmpty())
      return true;

    // L+bind/R+remote
    subrule = subrules.first().trimmed();
    if (!subrule.isEmpty())
    {
      if (app_protocol == PIPE)
      {
        if (!QRegExp(subrule, Qt::CaseInsensitive, QRegExp::Wildcard).exactMatch(fwd_direction == LOCAL_TO_REMOTE ? bind_address : remote_host))
          return false;
      }
      else
      {
        QString sub_host = subrule;
        QString sub_port;
        if (subrule.contains(":"))
        {
          QRegExp rx("(|\\*|.+):([0-9\\*]*)");
          if (!rx.exactMatch(subrule))
            return false;
          sub_host = rx.cap(1);
          sub_port = rx.cap(2);
        }
        if (!sub_host.isEmpty() && !QRegExp(sub_host, Qt::CaseInsensitive, QRegExp::Wildcard).exactMatch(fwd_direction == LOCAL_TO_REMOTE ? bind_address : remote_host))
          return false;
        if (!sub_port.isEmpty() && !QRegExp(sub_port, Qt::CaseInsensitive, QRegExp::Wildcard).exactMatch(QString::number(fwd_direction == LOCAL_TO_REMOTE ? bind_port : remote_port)))
          return false;
      }
    }
    subrules.removeFirst();
    if (subrules.isEmpty())
      return true;

    // tunservers chain
    int chain_index = 0;
    while (subrules.count() > 1)
    {
      subrule = subrules.first().trimmed();
      if (chain_index >= tunservers.count())
      {
        if (subrule == QString("*") || subrule.isEmpty())
        {
          chain_index++;
          subrules.removeFirst();
          continue;
        }
        else
          return false;
      }

      QString sub_host = subrule;
      QString sub_port;
      if (subrule.contains(":"))
      {
        QRegExp rx("(|\\*|.+):([0-9\\*]*)");
        if (!rx.exactMatch(subrule))
          return false;
        sub_host = rx.cap(1);
        sub_port = rx.cap(2);
      }
      if (!sub_host.isEmpty() && !QRegExp(sub_host, Qt::CaseInsensitive, QRegExp::Wildcard).exactMatch(tunservers.at(chain_index).host))
        return false;
      if (!sub_port.isEmpty() && !QRegExp(sub_port, Qt::CaseInsensitive, QRegExp::Wildcard).exactMatch(QString::number(tunservers.at(chain_index).port)))
        return false;

      chain_index++;
      subrules.removeFirst();
    }

    // L+remote/R+bind
    subrule = subrules.first().trimmed();
    if (!subrule.isEmpty())
    {
      if (app_protocol == PIPE)
      {
        if (!QRegExp(subrule, Qt::CaseInsensitive, QRegExp::Wildcard).exactMatch(fwd_direction == LOCAL_TO_REMOTE ? remote_host : bind_address))
          return false;
      }
      else
      {
        QString sub_host = subrule;
        QString sub_port;
        if (subrule.contains(":"))
        {
          QRegExp rx("(|\\*|.+):([0-9\\*]*)");
          if (!rx.exactMatch(subrule))
            return false;
          sub_host = rx.cap(1);
          sub_port = rx.cap(2);
        }
        if (!sub_host.isEmpty() && !QRegExp(sub_host, Qt::CaseInsensitive, QRegExp::Wildcard).exactMatch(fwd_direction == LOCAL_TO_REMOTE ? remote_host : bind_address))
          return false;
        if (!sub_port.isEmpty() && !QRegExp(sub_port, Qt::CaseInsensitive, QRegExp::Wildcard).exactMatch(QString::number(fwd_direction == LOCAL_TO_REMOTE ? remote_port : bind_port)))
          return false;
      }
    }

    return true;
  }

private:

  bool isEquivalTo(const TunnelParameters &src) const
  {
    return
        id == src.id &&
        orig_id == src.orig_id &&
        next_id == src.next_id &&
        max_incoming_connections == src.max_incoming_connections &&
        max_bytes_to_read_at_once == src.max_bytes_to_read_at_once &&
        connect_timeout == src.connect_timeout &&
        max_io_buffer_size == src.max_io_buffer_size &&
        read_buffer_size == src.read_buffer_size &&
        write_buffer_size == src.write_buffer_size &&
        max_data_packet_size == src.max_data_packet_size &&
        failure_tolerance_timeout == src.failure_tolerance_timeout &&
        name == src.name &&
        owner_user_id == src.owner_user_id &&
        owner_group_id == src.owner_group_id &&
        fwd_direction == src.fwd_direction &&
        app_protocol == src.app_protocol &&
        tunservers == src.tunservers &&
        bind_address == src.bind_address &&
        bind_port == src.bind_port &&
        remote_host == src.remote_host &&
        remote_port == src.remote_port &&
        idle_timeout == src.idle_timeout &&
        flags == src.flags;
  }
};

Q_DECLARE_METATYPE(TunnelParameters*);

#endif // TUNNELPARAMETERS_H
