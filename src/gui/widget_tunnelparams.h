/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef WIDGET_TUNNELPARAMS_H
#define WIDGET_TUNNELPARAMS_H

#include <QWidget>
#include "../lib/tunnel-parameters.h"

namespace Ui {
class WidgetTunnelParams;
}

class WidgetTunnelParams : public QWidget
{
  Q_OBJECT

public:
  explicit WidgetTunnelParams(QWidget *parent = 0);
  ~WidgetTunnelParams();

  void loadParams(TunnelParameters *params, bool _is_editable);
  void saveParams(TunnelParameters *params);
  void setTunnelName(const QString &name);
  bool canApply();
  void applyError(quint16 res_code);

private slots:
  void on_comboDirection_currentIndexChanged(int index);
  void on_comboAppProto_currentIndexChanged(int index);
  void on_chkPermanent_toggled(bool checked);
  void tunserverHostChanged();
  void tunserverPortChanged();
  void tunserverSettingsClicked();
  void tunserverRemoveClicked();

signals:
  void paramsModified();

private:
  Ui::WidgetTunnelParams *ui;
  bool is_editable;

  void tableWidget_addRow();
  void tableWidget_clear();
};

#endif // WIDGET_TUNNELPARAMS_H
