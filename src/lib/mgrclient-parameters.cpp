/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "mgrclient-parameters.h"

//---------------------------------------------------------------------------
bool MgrClientParameters::parseJSON(cJSON *json)
{
  *this = MgrClientParameters();
  cJSON *j;

  j = cJSON_GetObjectItem(json, "id");
  if (j && j->type == cJSON_Number)
    id = j->valueint;

  j = cJSON_GetObjectItem(json, "name");
  if (j && j->type == cJSON_String)
    name = QString::fromUtf8(j->valuestring);
  else
    return false;

  j = cJSON_GetObjectItem(json, "host");
  if (j && j->type == cJSON_String)
    host = QString::fromUtf8(j->valuestring);
  else
    return false;

  j = cJSON_GetObjectItem(json, "port");
  if (j && j->type == cJSON_Number)
    port = j->valueint;

  j = cJSON_GetObjectItem(json, "conn_type");
  if (j && j->type == cJSON_Number)
    conn_type = (ConnType)j->valueint;

  j = cJSON_GetObjectItem(json, "ssl_cert_filename");
  if (j && j->type == cJSON_String)
    ssl_cert_filename = QString::fromUtf8(j->valuestring);

  j = cJSON_GetObjectItem(json, "private_key_filename");
  if (j && j->type == cJSON_String)
    private_key_filename = QString::fromUtf8(j->valuestring);

  j = cJSON_GetObjectItem(json, "server_ssl_cert_filename");
  if (j && j->type == cJSON_String)
    server_ssl_cert_filename = QString::fromUtf8(j->valuestring);

  j = cJSON_GetObjectItem(json, "auth_username");
  if (j && j->type == cJSON_String)
    auth_username = QString::fromUtf8(j->valuestring);

  j = cJSON_GetObjectItem(json, "auth_password");
  if (j && j->type == cJSON_String)
    auth_password = QString::fromUtf8(j->valuestring);

  j = cJSON_GetObjectItem(json, "conn_timeout");
  if (j && j->type == cJSON_Number)
    conn_timeout = j->valueint;

  j = cJSON_GetObjectItem(json, "idle_timeout");
  if (j && j->type == cJSON_Number)
    idle_timeout = j->valueint;

  j = cJSON_GetObjectItem(json, "reconnect_interval");
  if (j && j->type == cJSON_Number)
    reconnect_interval = j->valueint;

  j = cJSON_GetObjectItem(json, "reconnect_interval_increment");
  if (j && j->type == cJSON_Number)
    reconnect_interval_increment = j->valueint;

  j = cJSON_GetObjectItem(json, "reconnect_interval_multiplicator");
  if (j && j->type == cJSON_Number)
    reconnect_interval_multiplicator = j->valuedouble;

  j = cJSON_GetObjectItem(json, "reconnect_interval_max");
  if (j && j->type == cJSON_Number)
    reconnect_interval_max = j->valueint;

  j = cJSON_GetObjectItem(json, "ping_interval");
  if (j && j->type == cJSON_Number)
    ping_interval = j->valueint;

  j = cJSON_GetObjectItem(json, "rcv_timeout");
  if (j && j->type == cJSON_Number)
    rcv_timeout = j->valueint;

  j = cJSON_GetObjectItem(json, "read_buffer_size");
  if (j && j->type == cJSON_Number)
    read_buffer_size = j->valueint;

  j = cJSON_GetObjectItem(json, "write_buffer_size");
  if (j && j->type == cJSON_Number)
    write_buffer_size = j->valueint;

  j = cJSON_GetObjectItem(json, "max_io_buffer_size");
  if (j && j->type == cJSON_Number)
    max_io_buffer_size = j->valueint;

  j = cJSON_GetObjectItem(json, "flags");
  if (j && j->type == cJSON_String)
    flags = QByteArray(j->valuestring).toUInt(0, 16);

  j = cJSON_GetObjectItem(json, "ssl_protocol");
  if (j && j->type == cJSON_Number)
    ssl_protocol = (QSsl::SslProtocol)j->valueint;

  j = cJSON_GetObjectItem(json, "allowed_ciphers");
  if (j && j->type == cJSON_String)
    allowed_ciphers = QString::fromUtf8(j->valuestring);

  j = cJSON_GetObjectItem(json, "enabled");
  if (j && j->type == cJSON_False)
    enabled = false;

  return true;
}

//---------------------------------------------------------------------------
void MgrClientParameters::printJSON(cJSON *json) const
{
  if (id > 0)
    cJSON_AddNumberToObject(json, "id", id);
  cJSON_AddStringToObject(json, "name", name.toUtf8());
  cJSON_AddStringToObject(json, "host", host.toUtf8());
  cJSON_AddNumberToObject(json, "port", port);
  cJSON_AddNumberToObject(json, "conn_type", (int)conn_type);
  if (!ssl_cert_filename.isEmpty())
    cJSON_AddStringToObject(json, "ssl_cert_filename", ssl_cert_filename.toUtf8());
  if (!private_key_filename.isEmpty())
    cJSON_AddStringToObject(json, "private_key_filename", private_key_filename.toUtf8());
  if (!server_ssl_cert_filename.isEmpty())
    cJSON_AddStringToObject(json, "server_ssl_cert_filename", server_ssl_cert_filename.toUtf8());
  if (!auth_username.isEmpty())
    cJSON_AddStringToObject(json, "auth_username", auth_username.toUtf8());
  if (!auth_password.isEmpty())
    cJSON_AddStringToObject(json, "auth_password", auth_password.toUtf8());
  cJSON_AddNumberToObject(json, "conn_timeout", conn_timeout);
  cJSON_AddNumberToObject(json, "idle_timeout", idle_timeout);
  cJSON_AddNumberToObject(json, "reconnect_interval", reconnect_interval);
  if (reconnect_interval_increment > 0)
    cJSON_AddNumberToObject(json, "reconnect_interval_increment", reconnect_interval_increment);
  if (reconnect_interval_multiplicator > 1)
    cJSON_AddNumberToObject(json, "reconnect_interval_multiplicator", reconnect_interval_multiplicator);
  cJSON_AddNumberToObject(json, "reconnect_interval_max", reconnect_interval_max);
  cJSON_AddNumberToObject(json, "ping_interval", ping_interval);
  cJSON_AddNumberToObject(json, "rcv_timeout", rcv_timeout);
  if (max_io_buffer_size > 0)
    cJSON_AddNumberToObject(json, "max_io_buffer_size", max_io_buffer_size);
  if (read_buffer_size > 0)
    cJSON_AddNumberToObject(json, "read_buffer_size", read_buffer_size);
  if (write_buffer_size > 0)
    cJSON_AddNumberToObject(json, "write_buffer_size", write_buffer_size);
  cJSON_AddStringToObject(json, "flags", QByteArray::number(flags, 16));
  cJSON_AddNumberToObject(json, "ssl_protocol", (int)ssl_protocol);
  if (!allowed_ciphers.isEmpty())
    cJSON_AddStringToObject(json, "allowed_ciphers", allowed_ciphers.toUtf8());
  if (!enabled)
    cJSON_AddFalseToObject(json, "enabled");
}

