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

private slots:
  void on_btnBrowsePrivateKeyFile_clicked();
  void on_btnBrowseSSLCertFile_clicked();
  void on_btnGenerateKeyCert_clicked();

private:
  Ui::UISettingsDialog *ui;

  friend void openUISettingsDialog(int tabIndex);
};

void openUISettingsDialog(int tabIndex=0);

#endif // UI_SETTINGS_DIALOG_H
