/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "../lib/sys_util.h"
#include "../lib/prc_log.h"
#include "../lib/mgrserver-parameters.h"
#include "../lib/ssl_helper.h"
#include <QFile>
#include <QNetworkInterface>
#include <QSslCertificate>
#include <QSslKey>
#include "mgr_server.h"

// these ones are only for main (GUI) thread
MgrServerParameters gui_mgrServerParams;
bool mgr_server_params_gui_modified=false;
bool mgr_server_params_modified_on_server_while_editing=false;

QString qmtunnel_config_filepath("./qmtunnel-server.conf");

//---------------------------------------------------------------------------
void MgrServer::config_init()
{
  params = MgrServerParameters();

  UserGroup userGroup;
  userGroup.name = tr("Administrators");
  userGroup.access_flags = 0xffffffff;
  userGroup.id = params.unique_user_group_id++;
  params.user_groups.append(userGroup);

  User user;
  user.name = tr("admin");
  user.group_id = userGroup.id;
  user.id = params.unique_user_id++;
  params.users.append(user);

  userGroup.name = tr("Tunnel servers");
  userGroup.access_flags = UserGroup::AF_TUNNEL_CREATE;
  userGroup.id = params.unique_user_group_id++;
  params.user_groups.append(userGroup);
}

//---------------------------------------------------------------------------
bool MgrServer::config_load()
{
  QFile f_config(qmtunnel_config_filepath);
  if (!f_config.exists())
  {
    config_init();
    return true;
  }
  if (!f_config.open(QIODevice::ReadOnly))
  {
    prc_log(LOG_CRITICAL, QString("Unable to open config file %1 for read").arg(qmtunnel_config_filepath));
    return false;
  }
  QByteArray buffer = f_config.readAll();
  f_config.close();

  const char *ep = NULL;
  cJSON *j_config = cJSON_ParseWithOpts(buffer.constData(), &ep, 0);
  if (!j_config)
  {
    prc_log(LOG_CRITICAL, QString("Unable to parse config file %1: ").arg(qmtunnel_config_filepath)+QString::fromUtf8(ep).left(256));
    return false;
  }

  params.parseJSON(j_config);

  cJSON *j = cJSON_GetObjectItem(j_config, "unique_tunnel_id");
  if (j && j->type == cJSON_Number)
    unique_tunnel_id = j->valueint;

  config_load_tunnels(j_config);

  cJSON_Delete(j_config);
  return true;
}

//---------------------------------------------------------------------------
bool MgrServer::config_save(MgrServerParameters *server_params)
{
  QFile f_config(qmtunnel_config_filepath);
  if (!f_config.open(QIODevice::WriteOnly))
  {
    prc_log(LOG_CRITICAL, QString("Unable to open config file %1 for write").arg(qmtunnel_config_filepath));
    return false;
  }

  cJSON *j_config = cJSON_CreateObject();
  server_params->printJSON(j_config);
  cJSON_AddNumberToObject(j_config, "unique_tunnel_id", unique_tunnel_id);
  config_save_tunnels(j_config);

  char *buffer = cJSON_PrintBuffered(j_config, 1024*64, 1);
  cJSON_Delete(j_config);
  qint64 n_bytes = f_config.write(buffer);
  free(buffer);
  f_config.close();
  if (n_bytes <= 0)
  {
    prc_log(LOG_CRITICAL, QString("Unable to write to config file %1").arg(qmtunnel_config_filepath));
    return false;
  }
  return true;
}

//---------------------------------------------------------------------------
void MgrServer::config_get(MgrClientConnection *socket)
{
  User *user = userById(mgrconn_state_list_in[socket]->user_id);
  if (!user)
  {
    return;
  }
  UserGroup *userGroup = userGroupById(user->group_id);
  if (!userGroup)
  {
    return;
  }
  if (!(userGroup->access_flags & UserGroup::AF_CONFIG_SET))
  {
    return;
  }

  mgrconn_state_list_in[socket]->flags |= MgrClientState::FL_CONFIG;

  cJSON *j_config = cJSON_CreateObject();
  params.printJSON(j_config, 0);

  cJSON *j_network_interfaces = cJSON_CreateArray();
  cJSON_AddItemToObject(j_config, "networkInterfaces", j_network_interfaces);
  QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
  for (int i=0; i < interfaces.count(); i++)
  {
    for (int a=0; a < interfaces.at(i).addressEntries().count(); a++)
    {
      cJSON *j_item = cJSON_CreateObject();
      cJSON_AddItemToArray(j_network_interfaces, j_item);
      cJSON_AddStringToObject(j_item, "name", interfaces.at(i).name().toUtf8());
      cJSON_AddStringToObject(j_item, "address", interfaces.at(i).addressEntries().at(a).ip().toString().toUtf8());
    }
  }

  char *buffer = cJSON_PrintBuffered(j_config, 1024*64, 1);
  cJSON_Delete(j_config);
  QByteArray data(buffer);
  free(buffer);
  socket->sendPacket(CMD_CONFIG_GET, data);
}

//---------------------------------------------------------------------------
void MgrServer::gui_config_set(cJSON *json)
{
  MgrServerParameters new_params;
  new_params.parseJSON(json);
  // as we don't send passwords in config, we need to rewrite them if new ones are not set
  for (int new_user_index=0; new_user_index < new_params.users.count(); new_user_index++)
  {
    for (int cur_user_index=0; cur_user_index < params.users.count(); cur_user_index++)
    {
      if (params.users[cur_user_index].id == new_params.users[new_user_index].id)
      {
        if (new_params.users[new_user_index].password.isEmpty())
          new_params.users[new_user_index].password = params.users[cur_user_index].password;
        else if (new_params.users[new_user_index].password == QString(USER_PASSWORD_SPECIAL_CLEAR))
          new_params.users[new_user_index].password.clear();
        break;
      }
    }
  }
  cJSON_Delete(json);
  if (new_params.listen_interface != params.listen_interface ||
      new_params.listen_port != params.listen_port ||
      new_params.ssl_cert_filename != params.ssl_cert_filename ||
      new_params.private_key_filename != params.private_key_filename)
  {
    prc_log(LOG_DBG2, QString("Restarting MgrServer due to config change"));
    new_params.config_version = ++params.config_version;
    if (!config_save(&new_params))
      return;
    stop();
    params = new_params;
    start();
  }
  else
  {
    new_params.config_version = ++params.config_version;
    if (!config_save(&new_params))
      return;
    params = new_params;
    initUserGroups();
    // send configuration to subscribed clients
    QHashIterator<MgrClientConnection *, MgrClientState *> iterator(mgrconn_state_list_in);
    while (iterator.hasNext())
    {
      iterator.next();
      if (iterator.value()->flags & MgrClientState::FL_CONFIG)
      {
        config_get(iterator.key());
      }
    }
  }
  if (receivers(SIGNAL(config_changed(cJSON *))) > 0)
  {
    cJSON *j_config = cJSON_CreateObject();
    params.printJSON(j_config);
    emit config_changed(j_config);
  }
}

//---------------------------------------------------------------------------
bool config_check_ssl_cert(const QString &filename, quint16 &res_code, QString &config_error)
{
  if (filename.isEmpty())
  {
    config_error = MgrServer::tr("No SSL certificate file specified");
    res_code = MgrServerParameters::RES_CODE_SSL_CERT_ERROR;
    return false;
  }
  QFile f(filename);
  if (!f.exists())
  {
    config_error = MgrServer::tr("File %1 does not exist").arg(f.fileName());
    res_code = MgrServerParameters::RES_CODE_SSL_CERT_ERROR;
    return false;
  }
  if (!f.open(QIODevice::ReadOnly))
  {
    config_error = MgrServer::tr("Unable to open file %1").arg(f.fileName());
    res_code = MgrServerParameters::RES_CODE_SSL_CERT_ERROR;
    return false;
  }
  f.close();
  QSslCertificate crt = loadSSLCertFromFile(f.fileName());
  if (crt.isNull())
  {
    config_error = MgrServer::tr("File %1 does not contain any valid SSL certificate").arg(f.fileName());
    res_code = MgrServerParameters::RES_CODE_SSL_CERT_ERROR;
    return false;
  }
  return true;
}

//---------------------------------------------------------------------------
bool config_check_private_key(const QString &filename, const QString &cert_filename, quint16 &res_code, QString &config_error)
{
  if (filename.isEmpty())
  {
    config_error = MgrServer::tr("No private key file specified");
    res_code = MgrServerParameters::RES_CODE_PRIVATE_KEY_ERROR;
    return false;
  }
  QFile f(filename);
  if (!f.exists())
  {
    config_error = MgrServer::tr("File %1 does not exist").arg(f.fileName());
    res_code = MgrServerParameters::RES_CODE_PRIVATE_KEY_ERROR;
    return false;
  }
  if (!f.open(QIODevice::ReadOnly))
  {
    config_error = MgrServer::tr("Unable to open file %1").arg(f.fileName());
    res_code = MgrServerParameters::RES_CODE_PRIVATE_KEY_ERROR;
    return false;
  }
  QByteArray buffer = f.read(1024*1024);
  f.close();
  if (buffer.isEmpty() || buffer.length() > 1024*1024)
  {
    config_error = MgrServer::tr("Invalid private key file %1: empty or too long").arg(f.fileName());
    res_code = MgrServerParameters::RES_CODE_PRIVATE_KEY_ERROR;
    return false;
  }
  // QSslKey cannot load encrypted key if no passphrase provided
  if (buffer.contains("ENCRYPTED"))
  {
    config_error = MgrServer::tr("Private key file %1 must NOT be encrypted with passphrase").arg(f.fileName());
    res_code = MgrServerParameters::RES_CODE_PRIVATE_KEY_ERROR;
    return false;
  }
  QSslKey key = QSslKey(buffer, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
  if (key.isNull())
  {
    config_error = MgrServer::tr("File %1 does not contain a valid private key").arg(f.fileName());
    res_code = MgrServerParameters::RES_CODE_PRIVATE_KEY_ERROR;
    return false;
  }
  QSslCertificate crt = loadSSLCertFromFile(cert_filename);
  if (!isCorrectKeyCertificatePair(key, crt))
  {
    config_error = MgrServer::tr("Private key file %1 does not correspond with certificate file %2").arg(f.fileName()).arg(cert_filename);
    res_code = MgrServerParameters::RES_CODE_PRIVATE_KEY_ERROR;
    return false;
  }
  return true;
}

//---------------------------------------------------------------------------
bool config_check(MgrServerParameters *old_params, MgrServerParameters *new_params, quint16 &res_code, QString &config_error)
{
  // check server port
  if (new_params->listen_port != old_params->listen_port)
  {
    if (!tcp_check_listen_port(new_params->listen_interface, new_params->listen_port, config_error))
    {
      res_code = MgrServerParameters::RES_CODE_LISTEN_ERROR;
      return false;
    }
  }

  if (!new_params->private_key_filename.isEmpty() || !new_params->ssl_cert_filename.isEmpty())
  {
    // check server certificate
    if (!config_check_ssl_cert(new_params->ssl_cert_filename, res_code, config_error))
      return false;

    // check server private key
    if (!config_check_private_key(new_params->private_key_filename, new_params->ssl_cert_filename, res_code, config_error))
      return false;
  }

  return true;
}

//---------------------------------------------------------------------------
void MgrServer::config_set(MgrClientConnection *socket, const QByteArray &data)
{
  User *user = userById(mgrconn_state_list_in[socket]->user_id);
  if (!user)
  {
    send_standart_reply(socket, CMD_CONFIG_SET_ERROR, QByteArray(), MgrServerParameters::RES_CODE_PERMISSION_DENIED, tr("Permission denied"));
    return;
  }
  UserGroup *userGroup = userGroupById(user->group_id);
  if (!userGroup)
  {
    send_standart_reply(socket, CMD_CONFIG_SET_ERROR, QByteArray(), MgrServerParameters::RES_CODE_PERMISSION_DENIED, tr("Permission denied"));
    return;
  }
  if (!(userGroup->access_flags & UserGroup::AF_CONFIG_SET))
  {
    send_standart_reply(socket, CMD_CONFIG_SET_ERROR, QByteArray(), MgrServerParameters::RES_CODE_PERMISSION_DENIED, tr("Permission denied"));
    return;
  }

  cJSON *j_config = cJSON_Parse(data);
  if (!j_config)
  {
    send_standart_reply(socket, CMD_CONFIG_SET_ERROR, QByteArray(), MgrServerParameters::RES_CODE_PARSING_ERROR, tr("Configuration parsing error"));
    return;
  }
  MgrServerParameters new_params;
  new_params.parseJSON(j_config);
  quint16 res_code = MgrServerParameters::RES_CODE_OK;
  QString config_error;
  if (config_check(&params, &new_params, res_code, config_error))
    gui_config_set(j_config);
  else
  {
    cJSON_Delete(j_config);
    send_standart_reply(socket, CMD_CONFIG_SET_ERROR, QByteArray(), res_code, config_error);
  }
}
