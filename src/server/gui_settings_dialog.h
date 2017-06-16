/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef GUI_SETTINGS_DIALOG_H
#define GUI_SETTINGS_DIALOG_H

#include <QDialog>

namespace Ui {
class UISettingsDialog;
}

class UISettingsDialog : public QDialog
{
  Q_OBJECT

public:
  explicit UISettingsDialog(QWidget *parent = 0);
  ~UISettingsDialog();

  void loadSettings();
  void saveSettings();

private:
  Ui::UISettingsDialog *ui;
};

void openUISettingsDialog();

#endif // UI_SETTINGS_DIALOG_H
