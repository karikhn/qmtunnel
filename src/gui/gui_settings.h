/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef GUI_SETTINGS_H
#define GUI_SETTINGS_H

#include "../lib/cJSON.h"
#include <QString>

extern cJSON *j_gui_settings;

enum GuiFlags
{
  GFL_AUTOOPEN_RECENT_PROFILE_ON_START   = 0x00000001
 ,GFL_MINIMIZE_TO_TRAY                   = 0x00000002
 ,GFL_MINIMIZE_TO_TRAY_ON_START          = 0x00000004
 ,GFL_MINIMIZE_TO_TRAY_ON_CLOSE          = 0x00000008
 ,GFL_TRAY_ALWAYS_VISIBLE                = 0x00000010
};
extern unsigned int gui_flags;
extern bool gui_no_config;

bool ui_load();
bool ui_save();
void ui_free();

#endif // UI_SETTINGS_H
