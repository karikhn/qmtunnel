/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "gui_settings_dialog.h"
#include "ui_gui_settings_dialog.h"
#include "gui_settings.h"
#include "mainwindow.h"

//---------------------------------------------------------------------------
UISettingsDialog::UISettingsDialog(QWidget *parent): QDialog(parent), ui(new Ui::UISettingsDialog)
{
  ui->setupUi(this);
}

//---------------------------------------------------------------------------
void UISettingsDialog::loadSettings()
{
  ui->chkMinimizeToTray->setChecked(gui_flags & GFL_MINIMIZE_TO_TRAY);
  ui->chkMinimizeToTrayOnStart->setChecked(gui_flags & GFL_MINIMIZE_TO_TRAY_ON_START);
  ui->chkMinimizeToTrayOnClose->setChecked(gui_flags & GFL_MINIMIZE_TO_TRAY_ON_CLOSE);
  ui->chkAlwaysShowTray->setChecked(gui_flags & GFL_TRAY_ALWAYS_VISIBLE);
}

//---------------------------------------------------------------------------
void set_gui_flags(bool is_checked, unsigned int flag_mask)
{
  if (is_checked)
    gui_flags |= flag_mask;
  else
    gui_flags &= ~flag_mask;
}

//---------------------------------------------------------------------------
void UISettingsDialog::saveSettings()
{
  set_gui_flags(ui->chkMinimizeToTray->isChecked(), GFL_MINIMIZE_TO_TRAY);
  set_gui_flags(ui->chkMinimizeToTrayOnStart->isChecked(), GFL_MINIMIZE_TO_TRAY_ON_START);
  set_gui_flags(ui->chkMinimizeToTrayOnClose->isChecked(), GFL_MINIMIZE_TO_TRAY_ON_CLOSE);
  set_gui_flags(ui->chkAlwaysShowTray->isChecked(), GFL_TRAY_ALWAYS_VISIBLE);
  ui_save();

  if (gui_flags & GFL_TRAY_ALWAYS_VISIBLE)
    trayIcon->setVisible(true);
  else
    trayIcon->setVisible(false);
}

//---------------------------------------------------------------------------
void openUISettingsDialog()
{
  UISettingsDialog dialog;
  dialog.loadSettings();
  if (dialog.exec() == QDialog::Accepted)
  {
    dialog.saveSettings();
  }
}

//---------------------------------------------------------------------------
UISettingsDialog::~UISettingsDialog()
{
  delete ui;
}
