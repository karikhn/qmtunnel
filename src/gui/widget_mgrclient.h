/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef WIDGET_MGRCLIENT_H
#define WIDGET_MGRCLIENT_H

#include <QWidget>
#include "../lib/cJSON.h"
#include "../lib/mgrclient-parameters.h"
#include "../lib/mclineedit.h"

namespace Ui {
class WidgetMgrClient;
}

class WidgetMgrClient : public QWidget
{
  Q_OBJECT

public:
  enum
  {
    FL_NO_NAME                 = 0x0001
   ,FL_NO_SSL_KEY_CERT         = 0x0002
   ,FL_NO_DEFAULT_CIPHERS      = 0x0004
   ,FL_NO_ENABLED              = 0x0008
   ,FL_SHOW_EXTRA              = 0x0010
   ,FL_NO_CONN_TYPE            = 0x0020
  };

  explicit WidgetMgrClient(int hide_flags, QWidget *parent = 0);
  ~WidgetMgrClient();

  bool canApply();
  void clearParams();
  void loadParams(MgrClientParameters *_params);
  void saveParams(MgrClientParameters *_params);

  void setMgrConnName(const QString &new_name);

  void ui_load(cJSON *json);
  void ui_save(cJSON *json);

  MgrClientParameters *params;

signals:
  void paramsModified();

public slots:
  void on_btnToggleExtraSettings_toggled(bool checked);

private slots:
  void spinBox_valueChanged(int value);
  void doubleSpinBox_valueChanged(double value);
  void chkConnType_clicked(bool);
  void chkAuthType_toggled(bool checked);
  void on_chkConnDemand_toggled(bool checked);
  void on_btnBrowseCertFile_clicked();
  void on_btnBrowsePrivateKeyFile_clicked();
  void on_btnBrowseServerCertFile_clicked();
  void on_chkNoEncryption_toggled(bool checked);
  void on_chkHeartbeat_toggled(bool checked);

protected:
  void showEvent(QShowEvent *event);

private:
  Ui::WidgetMgrClient *ui;
  MCLineEdit *edtAllowedCiphers;
};

#endif // TAB_MGRCLIENT_H
