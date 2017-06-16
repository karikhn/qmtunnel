/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef MGRSERVERPARAMETERS_H
#define MGRSERVERPARAMETERS_H

#include "mgrclient-parameters.h"
#include <QHostAddress>
#include <QMetaType>
#include <QStringList>
#include <QSslCertificate>
#include "tunnel-parameters.h"

#define USER_PASSWORD_SPECIAL_CLEAR              "-----:clear password:-----"
#define USER_PASSWORD_SPECIAL_SECRET             "-----:secret password:-----"

class UserGroup
{
public:
  quint32 id;                         // Unique group id
  QString name;                       // Group name
  bool enabled;
  QStringList tunnel_allow_list;      // LOCAL_TO_REMOTE format: L#local-bind-address:local-bind-port#tunserver[:port]#tunserver[:port]#...#remote-host:remote-port
                                      // REMOTE_TO_LOCAL format: R#remote-host:remote-port#tunserver[:port]#tunserver[:port]#...#remote-bind-address:remote-bind-port
                                      // example: L#127.0.0.1:8080#tunserver1:9999#yandex.ru:80

  quint64 access_flags;               // Access rights flags
  enum
  {
    AF_CONFIG_SET             = 0x00000001  // can view/modify configuration
   ,AF_TUNNEL_CREATE          = 0x00000002  // can create new tunnels
   ,AF_TUNNEL_GROUP_ADMIN     = 0x00000004  // can view/modify all tunnels owned by user group
   ,AF_TUNNEL_SUPER_ADMIN     = 0x00000008  // can view/modify all tunnels owned by any user
  };

  UserGroup()
  {
    id = 0;
    enabled = true;
    access_flags = 0;
    tunnel_allow_list << QString("L#*:*#*#*:*")
                      << QString("R#*:*#*#*:*");
  }

  UserGroup(const UserGroup &src)
  {
    id = src.id;
    name = src.name;
    enabled = src.enabled;
    access_flags = src.access_flags;
    tunnel_allow_list = src.tunnel_allow_list;
  }

  void parseJSON(cJSON *json);
  void printJSON(cJSON *json) const;

  bool operator ==(const UserGroup &src) const { return isEquivalTo(src); }
  bool operator !=(const UserGroup &src) const { return !isEquivalTo(src); }

private:

  bool isEquivalTo(const UserGroup &src) const
  {
    return
        id == src.id &&
        name == src.name &&
        enabled == src.enabled &&
        access_flags == src.access_flags;
  }
};

class User
{
public:
  quint32 id;                         // Unique user id
  QString name;                       // User name
  quint32 group_id;                   // User group id which this user belongs to
  bool enabled;

  QSslCertificate cert;               // X.509 SSL certificate
  QString password;                   // password

  quint64 flags;                      // User-specific flags
  enum
  {
//    UF_             = 0x00000001  //
  };

  User()
  {
    id = 0;
    group_id = 0;
    enabled = true;
    flags = 0;
  }

  User(const User &src)
  {
    id = src.id;
    name = src.name;
    group_id = src.group_id;
    enabled = src.enabled;
    cert = src.cert;
    password = src.password;
    flags = src.flags;
  }

  void parseJSON(cJSON *json);
  void printJSON(cJSON *json, bool print_password=true) const;

  bool operator ==(const User &src) const { return isEquivalTo(src); }
  bool operator !=(const User &src) const { return !isEquivalTo(src); }

private:

  bool isEquivalTo(const User &src) const
  {
    return
        id == src.id &&
        name == src.name &&
        enabled == src.enabled &&
        group_id == src.group_id &&
        flags == src.flags &&
        cert == src.cert;
  }
};

class MgrServerParameters
{
public:
  QHostAddress listen_interface;      // IP-address of qmtunnel manager listening interface
  quint16 listen_port;                // TCP port number

  QString ssl_cert_filename;          // OpenSSL certificate filename
  QString private_key_filename;       // OpenSSL private key filename

  quint32 max_incoming_mgrconn;       // maximum number of incoming management connections (GUI + tunservers)
  quint32 flags;                      // Additional flags, such as:
  enum
  {
    FL_ENABLE_HTTP                            = 0x00000001   // enable HTTP protocol for management connections
  };

  quint32 unique_conn_id;
  quint32 unique_user_group_id;
  quint32 unique_user_id;
  TunnelId unique_tunnel_id;

  QList<UserGroup> user_groups;
  QList<User> users;

  quint32 config_version;

  // result code for tunnel operations
  enum
  {
    RES_CODE_OK = 0
   ,RES_CODE_PERMISSION_DENIED = 1
   ,RES_CODE_PARSING_ERROR = 2
   ,RES_CODE_LISTEN_ERROR = 3
   ,RES_CODE_SSL_CERT_ERROR = 4
   ,RES_CODE_PRIVATE_KEY_ERROR = 5
  };

  MgrServerParameters()
  {
    listen_interface = QHostAddress::Any;
    listen_port = DEFAULT_MGR_PORT;
    max_incoming_mgrconn = 100;
    flags = 0;
    unique_conn_id = 1;
    unique_user_group_id = 1;
    unique_user_id = 1;
    unique_tunnel_id = 1;
    config_version = 1;
  }

  MgrServerParameters(const MgrServerParameters &src)
  {
    listen_interface = src.listen_interface;
    listen_port = src.listen_port;
    ssl_cert_filename = src.ssl_cert_filename;
    private_key_filename = src.private_key_filename;
    max_incoming_mgrconn = src.max_incoming_mgrconn;
    flags = src.flags;
    unique_conn_id = src.unique_conn_id;
    unique_user_group_id = src.unique_user_group_id;
    unique_user_id = src.unique_user_id;
    unique_tunnel_id = src.unique_tunnel_id;
    user_groups = src.user_groups;
    users = src.users;
    config_version = src.config_version;
  }

  enum
  {
    JF_PASSWORDS         = 0x0001
  };
  void parseJSON(cJSON *json, quint32 j_flags=0xffff);
  void printJSON(cJSON *json, quint32 j_flags=0xffff) const;

  bool operator ==(const MgrServerParameters &src) const { return isEquivalTo(src); }
  bool operator !=(const MgrServerParameters &src) const { return !isEquivalTo(src); }

private:

  bool isEquivalTo(const MgrServerParameters &src) const
  {
    return
        listen_interface == src.listen_interface &&
        listen_port == src.listen_port &&
        ssl_cert_filename == src.ssl_cert_filename &&
        private_key_filename == src.private_key_filename &&
        max_incoming_mgrconn == src.max_incoming_mgrconn &&
        flags == src.flags &&
        user_groups == src.user_groups &&
        users == src.users &&
        unique_conn_id == src.unique_conn_id &&
        unique_user_group_id == src.unique_user_group_id &&
        unique_user_id == src.unique_user_id;
  }
};

Q_DECLARE_METATYPE(MgrServerParameters*);

#endif // MGRSERVERPARAMETERS_H
