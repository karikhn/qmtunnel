/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "tunnel-parameters.h"

//---------------------------------------------------------------------------
void TunnelParameters::parseJSON(cJSON *json)
{
  *this = TunnelParameters();
  cJSON *j;

  j = cJSON_GetObjectItem(json, "id");
  if (j && j->type == cJSON_Number)
    id = j->valueint;

  j = cJSON_GetObjectItem(json, "orig_id");
  if (j && j->type == cJSON_Number)
    orig_id = j->valueint;

  j = cJSON_GetObjectItem(json, "next_id");
  if (j && j->type == cJSON_Number)
    next_id = j->valueint;

  j = cJSON_GetObjectItem(json, "owner_user_id");
  if (j && j->type == cJSON_Number)
    owner_user_id = j->valueint;

  j = cJSON_GetObjectItem(json, "owner_group_id");
  if (j && j->type == cJSON_Number)
    owner_group_id = j->valueint;

  j = cJSON_GetObjectItem(json, "name");
  if (j && j->type == cJSON_String)
    name = QString::fromUtf8(j->valuestring);

  j = cJSON_GetObjectItem(json, "fwd_direction");
  if (j && j->type == cJSON_Number)
    fwd_direction = (FwdDirection)j->valueint;

  j = cJSON_GetObjectItem(json, "app_protocol");
  if (j && j->type == cJSON_Number)
    app_protocol = (AppProtocol)j->valueint;

  cJSON *j_ar = cJSON_GetObjectItem(json, "tunservers");
  if (j_ar && j_ar->type == cJSON_Array)
  {
    tunservers.clear();
    int n = cJSON_GetArraySize(j_ar);
    for (int i=0; i < n; i++)
    {
      cJSON *j_item = cJSON_GetArrayItem(j_ar, i);
      MgrClientParameters tunserver;
      tunserver.parseJSON(j_item);
      tunservers.append(tunserver);
    }
  }

  j = cJSON_GetObjectItem(json, "bind_address");
  if (j && j->type == cJSON_String)
    bind_address = QString::fromUtf8(j->valuestring);

  j = cJSON_GetObjectItem(json, "bind_port");
  if (j && j->type == cJSON_Number)
    bind_port = j->valueint;

  j = cJSON_GetObjectItem(json, "remote_host");
  if (j && j->type == cJSON_String)
    remote_host = QString::fromUtf8(j->valuestring);

  j = cJSON_GetObjectItem(json, "remote_port");
  if (j && j->type == cJSON_Number)
    remote_port = j->valueint;

  j = cJSON_GetObjectItem(json, "udp_port_range_from");
  if (j && j->type == cJSON_Number)
    udp_port_range_from = j->valueint;

  j = cJSON_GetObjectItem(json, "udp_port_range_till");
  if (j && j->type == cJSON_Number)
    udp_port_range_till = j->valueint;

  j = cJSON_GetObjectItem(json, "idle_timeout");
  if (j && j->type == cJSON_Number)
    idle_timeout = j->valueint;

  j = cJSON_GetObjectItem(json, "max_incoming_connections");
  if (j && j->type == cJSON_Number)
    max_incoming_connections = j->valueint;

  j = cJSON_GetObjectItem(json, "max_bytes_to_read_at_once");
  if (j && j->type == cJSON_Number)
    max_bytes_to_read_at_once = j->valueint;

  j = cJSON_GetObjectItem(json, "max_io_buffer_size");
  if (j && j->type == cJSON_Number)
    max_io_buffer_size = j->valueint;

  j = cJSON_GetObjectItem(json, "read_buffer_size");
  if (j && j->type == cJSON_Number)
    read_buffer_size = j->valueint;

  j = cJSON_GetObjectItem(json, "write_buffer_size");
  if (j && j->type == cJSON_Number)
    write_buffer_size = j->valueint;

  j = cJSON_GetObjectItem(json, "max_data_packet_size");
  if (j && j->type == cJSON_Number)
    max_data_packet_size = j->valueint;

  j = cJSON_GetObjectItem(json, "connect_timeout");
  if (j && j->type == cJSON_Number)
    connect_timeout = j->valueint;

  j = cJSON_GetObjectItem(json, "failure_tolerance_timeout");
  if (j && j->type == cJSON_Number)
    failure_tolerance_timeout = j->valueint;

  j = cJSON_GetObjectItem(json, "max_state_dispatch_frequency");
  if (j && j->type == cJSON_Number)
    max_state_dispatch_frequency = j->valueint;

  j = cJSON_GetObjectItem(json, "max_incoming_connections_info");
  if (j && j->type == cJSON_Number)
    max_incoming_connections_info = j->valueint;

  j = cJSON_GetObjectItem(json, "incoming_connections_info_timeout");
  if (j && j->type == cJSON_Number)
    incoming_connections_info_timeout = j->valueint;

  j = cJSON_GetObjectItem(json, "flags");
  if (j && j->type == cJSON_String)
    flags = QByteArray(j->valuestring).toULongLong(0, 16);
}

//---------------------------------------------------------------------------
void TunnelParameters::printJSON(cJSON *json) const
{
  cJSON_AddNumberToObject(json, "id", id);
  if (orig_id > 0)
    cJSON_AddNumberToObject(json, "orig_id", orig_id);
  if (next_id > 0)
    cJSON_AddNumberToObject(json, "next_id", next_id);
  if (owner_user_id > 0)
    cJSON_AddNumberToObject(json, "owner_user_id", orig_id);
  if (owner_group_id > 0)
    cJSON_AddNumberToObject(json, "owner_group_id", orig_id);
  cJSON_AddStringToObject(json, "name", name.toUtf8());
  cJSON_AddNumberToObject(json, "fwd_direction", (int)fwd_direction);
  cJSON_AddNumberToObject(json, "app_protocol", (int)app_protocol);
  cJSON *j_ar = cJSON_CreateArray();
  cJSON_AddItemToObject(json, "tunservers", j_ar);
  for (int i=0; i < tunservers.count(); i++)
  {
    cJSON *j_item = cJSON_CreateObject();
    cJSON_AddItemToArray(j_ar, j_item);
    tunservers[i].printJSON(j_item);
  }
  if (!bind_address.isEmpty())
    cJSON_AddStringToObject(json, "bind_address", bind_address.toUtf8());
  if (bind_port > 0 && app_protocol != PIPE)
    cJSON_AddNumberToObject(json, "bind_port", bind_port);
  if (!remote_host.isEmpty())
    cJSON_AddStringToObject(json, "remote_host", remote_host.toUtf8());
  if (remote_port > 0 && app_protocol != PIPE)
    cJSON_AddNumberToObject(json, "remote_port", remote_port);
  if (app_protocol == UDP)
  {
    cJSON_AddNumberToObject(json, "udp_port_range_from", udp_port_range_from);
    cJSON_AddNumberToObject(json, "udp_port_range_till", udp_port_range_till);
  }
  if (!(flags & FL_PERMANENT_TUNNEL))
    cJSON_AddNumberToObject(json, "idle_timeout", idle_timeout);
  if (max_incoming_connections > 0)
    cJSON_AddNumberToObject(json, "max_incoming_connections", max_incoming_connections);
  if (max_bytes_to_read_at_once > 0)
    cJSON_AddNumberToObject(json, "max_bytes_to_read_at_once", max_bytes_to_read_at_once);
  if (max_io_buffer_size > 0)
    cJSON_AddNumberToObject(json, "max_io_buffer_size", max_io_buffer_size);
  if (read_buffer_size > 0)
    cJSON_AddNumberToObject(json, "read_buffer_size", read_buffer_size);
  if (write_buffer_size > 0)
    cJSON_AddNumberToObject(json, "write_buffer_size", write_buffer_size);
  if (max_data_packet_size > 0)
    cJSON_AddNumberToObject(json, "max_data_packet_size", max_data_packet_size);
  if (connect_timeout > 0)
    cJSON_AddNumberToObject(json, "connect_timeout", connect_timeout);
  if (failure_tolerance_timeout > 0)
    cJSON_AddNumberToObject(json, "failure_tolerance_timeout", failure_tolerance_timeout);
  if (max_state_dispatch_frequency > 0)
    cJSON_AddNumberToObject(json, "max_state_dispatch_frequency", max_state_dispatch_frequency);
  cJSON_AddNumberToObject(json, "max_incoming_connections_info", max_incoming_connections_info);
  cJSON_AddNumberToObject(json, "incoming_connections_info_timeout", incoming_connections_info_timeout);
  cJSON_AddStringToObject(json, "flags", QByteArray::number(flags, 16));
}
