/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef MGRCLIENT_PARAMETERS_H
#define MGRCLIENT_PARAMETERS_H

#include <QString>
#include <QSsl>
#include <QSslCertificate>
#include <QSslKey>
#include <QMetaType>
#include "../lib/cJSON.h"

#define DEFAULT_MGR_PORT           9200

class MgrClientParameters
{
public:
  quint32 id;                         // Unique item id
  QString name;                       // User-defined host name
  QString host;                       // hostname or IP-address of qmtunnel host
  quint16 port;                       // TCP port number (qmtunnel protocol port)
  bool enabled;

  enum ConnType { CONN_AUTO=1, CONN_DEMAND=2 } conn_type;  // Connection type

  QString ssl_cert_filename;          // OpenSSL certificate filename
  QString private_key_filename;       // OpenSSL private key filename
  QString server_ssl_cert_filename;   // OpenSSL server certificate filename
  QSslCertificate ssl_cert;           // OpenSSL certificate
  QSslKey private_key;                // OpenSSL private key
  QSslCertificate server_ssl_cert;    // OpenSSL server certificate

  QString auth_username;              // Username for authorization
  QString auth_password;              // Password for authorization

  quint32 conn_timeout;               // Connection timeout (ms)
  quint32 idle_timeout;               // Connection idle timeout (ms)
  quint32 ping_interval;              // Interval between heartbeat packets (ms)
  quint32 rcv_timeout;                // If no data received during this timeout (ms), consider connection dead
  quint32 reconnect_interval;         // Reconnect interval (ms)
  quint32 reconnect_interval_increment;      // If connect fails - increase reconnect_interval (ms)
  double reconnect_interval_multiplicator;   // If connect fails - multiply reconnect_interval (ms)
  quint32 reconnect_interval_max;            // If connect fails - maximum value of reconnect_interval (ms)

  quint32 max_io_buffer_size;           // Maximum read/write buffer size in bytes (0=unlimited)
  quint32 read_buffer_size;             // Maximum read buffer size in bytes (0=unlimited)
  quint32 write_buffer_size;            // Maximum write buffer size in bytes (0=unlimited)

  QSsl::SslProtocol ssl_protocol;     // SSL/TLS protocol to use
  QString allowed_ciphers;            // Colon-separated allowed ciphers ordered by preference (most prefered first), if empty - use defaults

  quint32 flags;                      // Additional flags, such as:
  enum
  {
    FL_USE_COMPRESSION                            = 0x00000001      // compress data when it's not too small
   ,FL_TCP_KEEP_ALIVE                             = 0x00000002      // enable tcp keepalive option
   ,FL_TCP_NO_DELAY                               = 0x00000004      // disable Nagle's algorithm (enable TCP_NODELAY option)
   ,FL_ENABLE_HEARTBEATS                          = 0x00000008      // enable heartbeats (to measure latency and keep connection alive) - STRONGLY RECOMMENDED
   ,FL_DISABLE_ENCRYPTION                         = 0x00000020      // completely disable encryption (SSL/TLS)
  };

  MgrClientParameters()
  {
    id = 0;
    port = DEFAULT_MGR_PORT;
    conn_type = CONN_AUTO;
    conn_timeout = 10*1000;
    idle_timeout = 600*1000;
    ping_interval = 10*1000;
    rcv_timeout = 15*1000;
    max_io_buffer_size = 1024*1024*32;
    read_buffer_size = 1024*1024*8;
    write_buffer_size = 1024*1024*8;
    flags = FL_USE_COMPRESSION
          | FL_ENABLE_HEARTBEATS;
    reconnect_interval = 100;
    reconnect_interval_increment = 0;
    reconnect_interval_multiplicator = 1.5;
    reconnect_interval_max = 10*1000;
    ssl_protocol = QSsl::SecureProtocols;
    enabled = true;
  }

  MgrClientParameters(const MgrClientParameters &src)
  {
    id = src.id;
    name = src.name;
    host = src.host;
    port = src.port;
    conn_type = src.conn_type;
    ssl_cert_filename = src.ssl_cert_filename;
    private_key_filename = src.private_key_filename;
    server_ssl_cert_filename = src.server_ssl_cert_filename;
    ssl_cert = src.ssl_cert;
    private_key = src.private_key;
    server_ssl_cert = src.server_ssl_cert;
    auth_username = src.auth_username;
    auth_password = src.auth_password;
    conn_timeout = src.conn_timeout;
    idle_timeout = src.idle_timeout;
    reconnect_interval = src.reconnect_interval;
    ping_interval = src.ping_interval;
    rcv_timeout = src.rcv_timeout;
    flags = src.flags;
    max_io_buffer_size = src.max_io_buffer_size;
    read_buffer_size = src.read_buffer_size;
    write_buffer_size = src.write_buffer_size;
    reconnect_interval_increment = src.reconnect_interval_increment;
    reconnect_interval_multiplicator = src.reconnect_interval_multiplicator;
    reconnect_interval_max = src.reconnect_interval_max;
    ssl_protocol = src.ssl_protocol;
    allowed_ciphers = src.allowed_ciphers;
    enabled = src.enabled;
  }

  bool parseJSON(cJSON *json);
  void printJSON(cJSON *json) const;

  bool operator ==(const MgrClientParameters &src) const { return isEquivalTo(src); }
  bool operator !=(const MgrClientParameters &src) const { return !isEquivalTo(src); }

private:

  bool isEquivalTo(const MgrClientParameters &src) const
  {
    return
        id == src.id &&
        name == src.name &&
        host == src.host &&
        port == src.port &&
        conn_type == src.conn_type &&
        ssl_cert_filename == src.ssl_cert_filename &&
        private_key_filename == src.private_key_filename &&
        server_ssl_cert_filename == src.server_ssl_cert_filename &&
        ssl_cert == src.ssl_cert &&
        private_key == src.private_key &&
        server_ssl_cert == src.server_ssl_cert &&
        auth_username == src.auth_username &&
        auth_password == src.auth_password &&
        conn_timeout == src.conn_timeout &&
        idle_timeout == src.idle_timeout &&
        reconnect_interval == src.reconnect_interval &&
        ping_interval == src.ping_interval &&
        rcv_timeout == src.rcv_timeout &&
        flags == src.flags &&
        max_io_buffer_size == src.max_io_buffer_size &&
        read_buffer_size == src.read_buffer_size &&
        write_buffer_size == src.write_buffer_size &&
        reconnect_interval_increment == src.reconnect_interval_increment &&
        reconnect_interval_multiplicator == src.reconnect_interval_multiplicator &&
        reconnect_interval_max == src.reconnect_interval_max &&
        ssl_protocol == src.ssl_protocol &&
        allowed_ciphers == src.allowed_ciphers &&
        enabled == src.enabled;
  }
};

Q_DECLARE_METATYPE(MgrClientParameters*);

#endif // MGRCLIENT_PARAMETERS_H
