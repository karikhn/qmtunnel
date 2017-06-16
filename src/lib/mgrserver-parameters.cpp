/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "mgrserver-parameters.h"

//---------------------------------------------------------------------------
void UserGroup::parseJSON(cJSON *json)
{
  *this = UserGroup();
  cJSON *j;

  j = cJSON_GetObjectItem(json, "id");
  if (j && j->type == cJSON_Number)
    id = j->valueint;

  j = cJSON_GetObjectItem(json, "name");
  if (j && j->type == cJSON_String)
    name = QString::fromUtf8(j->valuestring);

  j = cJSON_GetObjectItem(json, "enabled");
  enabled = !j || j->type != cJSON_False;

  j = cJSON_GetObjectItem(json, "access_flags");
  if (j && j->type == cJSON_String)
    access_flags = QByteArray(j->valuestring).toULongLong(0, 16);

  cJSON *j_ar = cJSON_GetObjectItem(json, "tunnel_allow_list");
  if (j_ar && j_ar->type == cJSON_Array)
  {
    tunnel_allow_list.clear();
    for (int i=0; i < cJSON_GetArraySize(j_ar); i++)
    {
      j = cJSON_GetArrayItem(j_ar, i);
      if (j->type == cJSON_String)
        tunnel_allow_list.append(QString::fromUtf8(j->valuestring));
    }
  }
}

//---------------------------------------------------------------------------
void UserGroup::printJSON(cJSON *json) const
{
  cJSON_AddNumberToObject(json, "id", id);
  cJSON_AddStringToObject(json, "name", name.toUtf8());
  if (!enabled)
    cJSON_AddFalseToObject(json, "enabled");
  if (access_flags != 0)
    cJSON_AddStringToObject(json, "access_flags", QByteArray::number(access_flags, 16));
  cJSON *j_ar = cJSON_CreateArray();
  cJSON_AddItemToObject(json, "tunnel_allow_list", j_ar);
  for (int i=0; i < tunnel_allow_list.count(); i++)
    cJSON_AddItemToArray(j_ar, cJSON_CreateString(tunnel_allow_list[i].toUtf8()));
}


//---------------------------------------------------------------------------
void User::parseJSON(cJSON *json)
{
  *this = User();
  cJSON *j;

  j = cJSON_GetObjectItem(json, "id");
  if (j && j->type == cJSON_Number)
    id = j->valueint;

  j = cJSON_GetObjectItem(json, "name");
  if (j && j->type == cJSON_String)
    name = QString::fromUtf8(j->valuestring);

  j = cJSON_GetObjectItem(json, "enabled");
  enabled = !j || j->type != cJSON_False;

  j = cJSON_GetObjectItem(json, "group_id");
  if (j && j->type == cJSON_Number)
    group_id = j->valueint;

  j = cJSON_GetObjectItem(json, "cert");
  if (j && j->type == cJSON_String)
  {
    QList<QSslCertificate> cert_list = QSslCertificate::fromData(j->valuestring);
    if (!cert_list.isEmpty())
      cert = cert_list.first();
  }

  j = cJSON_GetObjectItem(json, "password");
  if (j && j->type == cJSON_String)
    password = QString::fromUtf8(j->valuestring);

  j = cJSON_GetObjectItem(json, "flags");
  if (j && j->type == cJSON_String)
    flags = QByteArray(j->valuestring).toULongLong(0, 16);
}

//---------------------------------------------------------------------------
void User::printJSON(cJSON *json, bool print_password) const
{
  cJSON_AddNumberToObject(json, "id", id);
  cJSON_AddStringToObject(json, "name", name.toUtf8());
  if (!enabled)
    cJSON_AddFalseToObject(json, "enabled");
  cJSON_AddNumberToObject(json, "group_id", group_id);
  if (!cert.isNull())
    cJSON_AddStringToObject(json, "cert", cert.toPem());
  if (!password.isEmpty())
    cJSON_AddStringToObject(json, "password", (print_password ? password.toUtf8() : USER_PASSWORD_SPECIAL_SECRET));
  if (flags != 0)
    cJSON_AddStringToObject(json, "flags", QByteArray::number(flags, 16));
}

//---------------------------------------------------------------------------
void MgrServerParameters::parseJSON(cJSON *json, quint32 /*j_flags*/)
{
  *this = MgrServerParameters();
  cJSON *j;

  j = cJSON_GetObjectItem(json, "listen_interface");
  if (j && j->type == cJSON_String)
    listen_interface = QHostAddress(QString::fromUtf8(j->valuestring));

  j = cJSON_GetObjectItem(json, "listen_port");
  if (j && j->type == cJSON_Number)
    listen_port = j->valueint;

  j = cJSON_GetObjectItem(json, "unique_conn_id");
  if (j && j->type == cJSON_Number)
    unique_conn_id = j->valueint;

  j = cJSON_GetObjectItem(json, "unique_user_group_id");
  if (j && j->type == cJSON_Number)
    unique_user_group_id = j->valueint;

  j = cJSON_GetObjectItem(json, "unique_user_id");
  if (j && j->type == cJSON_Number)
    unique_user_id = j->valueint;

  j = cJSON_GetObjectItem(json, "unique_tunnel_id");
  if (j && j->type == cJSON_Number)
    unique_tunnel_id = j->valueint;

  j = cJSON_GetObjectItem(json, "config_version");
  if (j && j->type == cJSON_Number)
    config_version = j->valueint;

  j = cJSON_GetObjectItem(json, "ssl_cert_filename");
  if (j && j->type == cJSON_String)
    ssl_cert_filename = QString::fromUtf8(j->valuestring);

  j = cJSON_GetObjectItem(json, "private_key_filename");
  if (j && j->type == cJSON_String)
    private_key_filename = QString::fromUtf8(j->valuestring);

  j = cJSON_GetObjectItem(json, "max_incoming_mgrconn");
  if (j && j->type == cJSON_Number)
    max_incoming_mgrconn = j->valueint;

  j = cJSON_GetObjectItem(json, "flags");
  if (j && j->type == cJSON_String)
    flags = QByteArray(j->valuestring).toUInt(0, 16);

  cJSON *j_user_groups = cJSON_GetObjectItem(json, "user_groups");
  if (j_user_groups && j_user_groups->type == cJSON_Array)
  {
    user_groups.clear();
    int n = cJSON_GetArraySize(j_user_groups);
    for (int i=0; i < n; i++)
    {
      j = cJSON_GetArrayItem(j_user_groups, i);
      UserGroup group;
      group.parseJSON(j);
      user_groups.append(group);
    }
  }

  cJSON *j_users = cJSON_GetObjectItem(json, "users");
  if (j_users && j_users->type == cJSON_Array)
  {
    users.clear();
    int n = cJSON_GetArraySize(j_users);
    for (int i=0; i < n; i++)
    {
      j = cJSON_GetArrayItem(j_users, i);
      User user;
      user.parseJSON(j);
      users.append(user);
    }
  }
}

//---------------------------------------------------------------------------
void MgrServerParameters::printJSON(cJSON *json, quint32 j_flags) const
{
  cJSON_AddStringToObject(json, "listen_interface", listen_interface.toString().toUtf8());
  cJSON_AddNumberToObject(json, "listen_port", listen_port);
  if (!ssl_cert_filename.isEmpty())
    cJSON_AddStringToObject(json, "ssl_cert_filename", ssl_cert_filename.toUtf8());
  if (!private_key_filename.isEmpty())
    cJSON_AddStringToObject(json, "private_key_filename", private_key_filename.toUtf8());
  cJSON_AddNumberToObject(json, "max_incoming_mgrconn", max_incoming_mgrconn);
  if (flags != 0)
    cJSON_AddStringToObject(json, "flags", QByteArray::number(flags, 16));
  cJSON_AddNumberToObject(json, "unique_conn_id", unique_conn_id);
  cJSON_AddNumberToObject(json, "unique_user_group_id", unique_user_group_id);
  cJSON_AddNumberToObject(json, "unique_user_id", unique_user_id);
  cJSON_AddNumberToObject(json, "unique_tunnel_id", unique_tunnel_id);
  cJSON_AddNumberToObject(json, "config_version", config_version);

  if (!user_groups.isEmpty())
  {
    cJSON *j_user_groups = cJSON_CreateArray();
    cJSON_AddItemToObject(json, "user_groups", j_user_groups);
    for (int i=0; i < user_groups.count(); i++)
    {
      cJSON *j_user_group = cJSON_CreateObject();
      user_groups[i].printJSON(j_user_group);
      cJSON_AddItemToArray(j_user_groups, j_user_group);
    }
  }

  if (!users.isEmpty())
  {
    cJSON *j_users = cJSON_CreateArray();
    cJSON_AddItemToObject(json, "users", j_users);
    for (int i=0; i < users.count(); i++)
    {
      cJSON *j_user = cJSON_CreateObject();
      users[i].printJSON(j_user, (j_flags & JF_PASSWORDS));
      cJSON_AddItemToArray(j_users, j_user);
    }
  }
}
