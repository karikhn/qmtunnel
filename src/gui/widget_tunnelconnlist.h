/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef WIDGET_TUNNELCONNLIST_H
#define WIDGET_TUNNELCONNLIST_H

#include <QWidget>
#include <QTimer>
#include <QStandardItemModel>
#include "ui_widget_tunnelconnlist.h"
#include "../lib/cJSON.h"
#include "../lib/tunnel-state.h"

namespace Ui {
class WidgetTunnelConnList;
}

class WidgetTunnelConnList : public QWidget
{
  Q_OBJECT

public:
  explicit WidgetTunnelConnList(QWidget *parent = 0);
  ~WidgetTunnelConnList();

  Ui::WidgetTunnelConnList *ui;

  void tunnel_connstate_received(const QByteArray &data);

  void ui_load(cJSON *json);
  void ui_save(cJSON *json);

public slots:
  void activate();
  void deactivate();
  void emitUpdateNow();

private slots:
  void on_spinAutoUpdateInterval_valueChanged(double);
  void on_chkAutoUpdate_toggled(bool checked);

  void sort_conn_list();

signals:
  void needUpdate(bool now=false);

private:
  QTimer *timer_autoUpdate;
  QStandardItemModel *model;
  bool first_run;

  QHash<TunnelConnId, int> connlist_rows;

  void model_init();
};

#endif // WIDGET_TUNNELCONNLIST_H
