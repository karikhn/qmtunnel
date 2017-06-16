/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "gui_settings.h"
#include "../lib/sys_util.h"
#include "../lib/prc_log.h"
#include <stdlib.h>
#include <QString>
#include <QFile>

#define UI_SETTINGS_VERSION               4
#define UI_SETTINGS_MAX_SIZE              1024*1024

cJSON *j_gui_settings=NULL;
QString gui_settings_filepath("./qmtunnel-gui.uiconf");
unsigned int gui_flags=0;
bool gui_no_config=true;
//---------------------------------------------------------------------------
bool ui_load()
{
  QFile f_ui_settings(gui_settings_filepath);
  if (!f_ui_settings.exists())
  {
    j_gui_settings = cJSON_CreateObject();
    return true;
  }
  if (!f_ui_settings.open(QIODevice::ReadOnly))
  {
    prc_log(LOG_CRITICAL, QString("Unable to open ui settings file %1 for read").arg(gui_settings_filepath));
    j_gui_settings = cJSON_CreateObject();
    return false;
  }
  QByteArray buffer = f_ui_settings.read(UI_SETTINGS_MAX_SIZE);
  f_ui_settings.close();

  j_gui_settings = cJSON_Parse(buffer);
  if (!j_gui_settings)
  {
    prc_log(LOG_CRITICAL, QString("Unable to parse ui settings file %1").arg(gui_settings_filepath));
    j_gui_settings = cJSON_CreateObject();
    return false;
  }

  cJSON *j = cJSON_GetObjectItem(j_gui_settings, "version");
  if (!j || j->type != cJSON_Number || j->valueint != UI_SETTINGS_VERSION)
  {
    prc_log(LOG_HIGH, QString("Ignoring ui settings file %1 because of invalid version").arg(gui_settings_filepath));
    cJSON_Delete(j_gui_settings);
    j_gui_settings = cJSON_CreateObject();
    return false;
  }

  j = cJSON_GetObjectItem(j_gui_settings, "gui_flags");
  if (j && j->type == cJSON_String)
    gui_flags = QByteArray(j->valuestring).toULongLong(0, 16);

  gui_no_config = false;

  return true;
}

//---------------------------------------------------------------------------
bool ui_save()
{
  QFile f_ui_settings(gui_settings_filepath);
  if (!f_ui_settings.open(QIODevice::WriteOnly))
  {
    prc_log(LOG_CRITICAL, QString("Unable to open ui settings file %1 for write").arg(gui_settings_filepath));
    return false;
  }

  cJSON *j = cJSON_GetObjectItem(j_gui_settings, "version");
  if (j && (j->type != cJSON_Number || j->valueint != UI_SETTINGS_VERSION))
  {
    cJSON_DeleteItemFromObject(j_gui_settings, "version");
    j = NULL;
  }
  if (!j)
    cJSON_AddNumberToObject(j_gui_settings, "version", UI_SETTINGS_VERSION);


  j = cJSON_GetObjectItem(j_gui_settings, "gui_flags");
  if (j && (j->type != cJSON_String || QByteArray(j->valuestring).toULongLong(0, 16) != gui_flags))
  {
    cJSON_DeleteItemFromObject(j_gui_settings, "gui_flags");
    j = NULL;
  }
  if (!j)
    cJSON_AddStringToObject(j_gui_settings, "gui_flags", QByteArray::number(gui_flags, 16));

  char *buffer = cJSON_PrintBuffered(j_gui_settings, UI_SETTINGS_MAX_SIZE, 1);
  qint64 n_bytes = f_ui_settings.write(buffer);
  free(buffer);
  f_ui_settings.close();
  if (n_bytes <= 0)
  {
    prc_log(LOG_CRITICAL, QString("Unable to write to ui settings file %1").arg(gui_settings_filepath));
    return false;
  }
  return true;
}

//---------------------------------------------------------------------------
void ui_free()
{
  if (j_gui_settings)
  {
    cJSON_Delete(j_gui_settings);
    j_gui_settings = NULL;
  }
}
