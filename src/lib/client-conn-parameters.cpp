/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "client-conn-parameters.h"

//---------------------------------------------------------------------------
void ClientConnParameters::parseJSON(cJSON *json)
{
  *this = ClientConnParameters();
  cJSON *j;

  j = cJSON_GetObjectItem(json, "id");
  if (j && j->type == cJSON_Number)
    id = j->valueint;

  j = cJSON_GetObjectItem(json, "name");
  if (j && j->type == cJSON_String)
    name = QString::fromUtf8(j->valuestring);
  else
    name = QString("qmtunnel host %1").arg(id);

  j = cJSON_GetObjectItem(json, "host");
  if (j && j->type == cJSON_String)
    host = QString::fromUtf8(j->valuestring);

  j = cJSON_GetObjectItem(json, "port");
  if (j && j->type == cJSON_Number)
    port = j->valueint;

  j = cJSON_GetObjectItem(json, "direction");
  if (j && j->type == cJSON_Number)
    direction = (ConnDirection)j->valueint;

  j = cJSON_GetObjectItem(json, "conn_type");
  if (j && j->type == cJSON_Number)
    conn_type = (ConnType)j->valueint;

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

  j = cJSON_GetObjectItem(json, "reconnect_interval_increment");
  if (j && j->type == cJSON_Number)
    reconnect_interval_multiplicator = j->valuedouble;

  j = cJSON_GetObjectItem(json, "reconnect_interval_max");
  if (j && j->type == cJSON_Number)
    reconnect_interval_max = j->valueint;

  j = cJSON_GetObjectItem(json, "rcv_timeout");
  if (j && j->type == cJSON_Number)
    rcv_timeout = j->valueint;

  j = cJSON_GetObjectItem(json, "io_buffer_size");
  if (j && j->type == cJSON_Number)
    io_buffer_size = j->valueint;

  j = cJSON_GetObjectItem(json, "flags");
  if (j && j->type == cJSON_String)
    flags = QByteArray(j->valuestring).toUInt(0, 16);
}

//---------------------------------------------------------------------------
void ClientConnParameters::printJSON(cJSON *json) const
{
  cJSON_AddNumberToObject(json, "id", id);
  cJSON_AddStringToObject(json, "name", name.toUtf8());
  cJSON_AddStringToObject(json, "host", host.toUtf8());
  cJSON_AddNumberToObject(json, "port", port);
  cJSON_AddNumberToObject(json, "direction", (int)direction);
  cJSON_AddNumberToObject(json, "conn_type", (int)conn_type);
  if (conn_timeout > 0)
    cJSON_AddNumberToObject(json, "conn_timeout", conn_timeout);
  if (idle_timeout > 0)
    cJSON_AddNumberToObject(json, "idle_timeout", idle_timeout);
  if (reconnect_interval > 0)
    cJSON_AddNumberToObject(json, "reconnect_interval", reconnect_interval);
  if (reconnect_interval_increment > 0)
    cJSON_AddNumberToObject(json, "reconnect_interval_increment", reconnect_interval_increment);
  if (reconnect_interval_multiplicator > 1)
    cJSON_AddNumberToObject(json, "reconnect_interval_multiplicator", reconnect_interval_multiplicator);
  cJSON_AddNumberToObject(json, "reconnect_interval_max", reconnect_interval_max);
  if (rcv_timeout > 0)
    cJSON_AddNumberToObject(json, "rcv_timeout", rcv_timeout);
  if (io_buffer_size > 0)
    cJSON_AddNumberToObject(json, "io_buffer_size", io_buffer_size);
  cJSON_AddStringToObject(json, "flags", QByteArray::number(flags, 16));
}
