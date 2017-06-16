/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef WIDGET_MGRSERVER_H
#define WIDGET_MGRSERVER_H

#include <QWidget>
#include "../lib/mgrserver-parameters.h"
#include "ui_widget_mgrserver.h"

namespace Ui {
class WidgetMgrServer;
}

class WidgetMgrServer : public QWidget
{
  Q_OBJECT

public:
  explicit WidgetMgrServer(bool remote, QWidget *parent = 0);
  ~WidgetMgrServer();

  bool canApply();
  void loadParams(MgrServerParameters *params);
  void saveParams(MgrServerParameters *params);

  bool loadNetworkInterfaces(cJSON *j_network_interfaces);

  void applyError(quint16 res_code);

  Ui::WidgetMgrServer *ui;

private slots:
  void mgrServerConfig_valueChanged(int value);
  void on_btnBrowseMgrPrivateKeyFile_clicked();
  void on_btnBrowseMgrSSLCertFile_clicked();
  void on_btnGenerateKeyCert_clicked();

signals:
  void paramsModified();

private:

  QHostAddress old_listen_interface;
  quint16 old_listen_port;
  quint32 old_max_incoming_mgrconn;
};

#endif // WIDGET_MGRSERVER_H
