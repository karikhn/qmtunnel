/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "widget_tunnelconnlist.h"
#include "../lib/sys_util.h"

#define UI_TUNNEL_CONNLIST_VERSION    1

//---------------------------------------------------------------------------
WidgetTunnelConnList::WidgetTunnelConnList(QWidget *parent): QWidget(parent), ui(new Ui::WidgetTunnelConnList)
{
  ui->setupUi(this);
  first_run = true;

  model = new QStandardItemModel(0, 7, this);
  ui->tableView->setModel(model);
  model_init();
  connect(ui->tableView->horizontalHeader(), SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)), this, SLOT(sort_conn_list()));

  connect(ui->chkGetDisconnected, SIGNAL(clicked()), this, SLOT(emitUpdateNow()));

  timer_autoUpdate = new QTimer(this);
  timer_autoUpdate->setInterval(ui->spinAutoUpdateInterval->value()*1000);
  connect(timer_autoUpdate, SIGNAL(timeout()), this, SIGNAL(needUpdate()));
}

//---------------------------------------------------------------------------
void WidgetTunnelConnList::model_init()
{
  QByteArray sav_state = ui->tableView->horizontalHeader()->saveState();
  model->clear();
  model->setSortRole(Qt::UserRole+2);
  model->setColumnCount(7);
  model->setHorizontalHeaderLabels(QStringList()
                                   << "Peer address:port"
                                   << "Out.port"
                                   << "Connected"
                                   << "Disconnected"
                                   << "Rx"
                                   << "Tx"
                                   << "Status");
  connlist_rows.clear();
  ui->tableView->horizontalHeader()->restoreState(sav_state);
}

//---------------------------------------------------------------------------
void WidgetTunnelConnList::ui_load(cJSON *json)
{
  cJSON *j = cJSON_GetObjectItem(json, "tunnelConnList_version");
  if (!j || j->type != cJSON_Number || j->valueint != UI_TUNNEL_CONNLIST_VERSION)
  {
    first_run = true;
    ui->tableView->horizontalHeader()->setSortIndicator(2, Qt::DescendingOrder);
    ui->tableView->horizontalHeader()->setSortIndicatorShown(true);
    return;
  }

  j = cJSON_GetObjectItem(json, "tunnelConnList_autoUpdate");
  if (j)
    ui->chkAutoUpdate->setChecked(j->type == cJSON_True);

  j = cJSON_GetObjectItem(json, "tunnelConnList_getDisconnected");
  if (j)
    ui->chkGetDisconnected->setChecked(j->type == cJSON_True);

  j = cJSON_GetObjectItem(json, "tunnelConnList_autoUpdateInterval");
  if (j && j->type == cJSON_Number)
  {
    ui->spinAutoUpdateInterval->setValue(j->valuedouble);
    timer_autoUpdate->setInterval(ui->spinAutoUpdateInterval->value()*1000);
  }

  j = cJSON_GetObjectItem(json, "tunnelConnList_tableHeaderState");
  if (j && j->type == cJSON_String)
    ui->tableView->horizontalHeader()->restoreState(QByteArray::fromBase64(j->valuestring));

  first_run = false;
}

//---------------------------------------------------------------------------
void WidgetTunnelConnList::ui_save(cJSON *json)
{
  cJSON_AddNumberToObject(json, "tunnelConnList_version", UI_TUNNEL_CONNLIST_VERSION);
  if (ui->chkAutoUpdate->isChecked())
    cJSON_AddTrueToObject(json, "tunnelConnList_autoUpdate");
  else
    cJSON_AddFalseToObject(json, "tunnelConnList_autoUpdate");
  if (ui->chkGetDisconnected->isChecked())
    cJSON_AddTrueToObject(json, "tunnelConnList_getDisconnected");
  else
    cJSON_AddFalseToObject(json, "tunnelConnList_getDisconnected");
  cJSON_AddNumberToObject(json, "tunnelConnList_autoUpdateInterval", ui->spinAutoUpdateInterval->value());
  cJSON_AddStringToObject(json, "tunnelConnList_tableHeaderState", ui->tableView->horizontalHeader()->saveState().toBase64());
}

//---------------------------------------------------------------------------
void WidgetTunnelConnList::emitUpdateNow()
{
  emit needUpdate(true);
}

//---------------------------------------------------------------------------
void WidgetTunnelConnList::on_spinAutoUpdateInterval_valueChanged(double)
{
  timer_autoUpdate->setInterval(ui->spinAutoUpdateInterval->value()*1000);
}

//---------------------------------------------------------------------------
void WidgetTunnelConnList::activate()
{
  ui->tableView->setEnabled(false);
  model_init();
  emitUpdateNow();
  if (ui->chkAutoUpdate->isChecked())
    timer_autoUpdate->start();
}

//---------------------------------------------------------------------------
void WidgetTunnelConnList::deactivate()
{
  ui->tableView->setEnabled(false);
  model_init();
  if (timer_autoUpdate->isActive())
    timer_autoUpdate->stop();
}

//---------------------------------------------------------------------------
void WidgetTunnelConnList::on_chkAutoUpdate_toggled(bool checked)
{
  ui->spinAutoUpdateInterval->setEnabled(checked);
  if (checked)
  {
    emitUpdateNow();
    timer_autoUpdate->start();
  }
  else if (timer_autoUpdate->isActive())
    timer_autoUpdate->stop();
}

//---------------------------------------------------------------------------
void WidgetTunnelConnList::sort_conn_list()
{
  model->sort(ui->tableView->horizontalHeader()->sortIndicatorSection(), ui->tableView->horizontalHeader()->sortIndicatorOrder());
  connlist_rows.clear();
  for (int row=0; row < model->rowCount(); row++)
    connlist_rows.insert(model->item(row, 0)->data(Qt::UserRole+1).toInt(), row);
}

//---------------------------------------------------------------------------
void WidgetTunnelConnList::tunnel_connstate_received(const QByteArray &data)
{
  int data_pos = 0;
  QHash<TunnelConnId, TunnelConnInInfo> connlist;
  while (data_pos < data.length())
  {
    TunnelConnId conn_id = *((TunnelConnId *)(data.constData()+data_pos));
    data_pos += sizeof(conn_id);
    TunnelConnInInfo conn_info;
    int conn_info_len = conn_info.parseFromBuffer(data.constData()+data_pos, data.length()-data_pos);
    if (conn_info_len < 0)
      break;
    data_pos += conn_info_len;
    connlist.insert(conn_id, conn_info);
  }

  ui->tableView->setUpdatesEnabled(false);
  QHashIterator<TunnelConnId, TunnelConnInInfo> i(connlist);
  QList<int> existing_rows;
  while (i.hasNext())
  {
    i.next();
    int row = connlist_rows.value(i.key(), -1);
    if (row < 0)
    {
      QList<QStandardItem *> items;
      for (int col=0; col < model->columnCount(); col++)
        items.append(new QStandardItem());
      model->appendRow(items);
      row = model->rowCount()-1;

      model->item(row, 0)->setData((int)i.key(), Qt::UserRole+1);
      model->item(row, 0)->setText(i.value().peer_address.toString()+QString(":%1").arg(i.value().peer_port));
      model->item(row, 0)->setData(model->item(row, 0)->text(), Qt::UserRole+2);
      model->item(row, 2)->setText(pretty_date_time(i.value().t_connected.toLocalTime(), QDateTime(), false));
      model->item(row, 2)->setData(i.value().t_connected, Qt::UserRole+2);
    }
    existing_rows.append(row);
    model->item(row, 1)->setText(i.value().tun_out_port > 0 ? QString::number(i.value().tun_out_port) : QString());
    model->item(row, 1)->setData(i.value().tun_out_port, Qt::UserRole+2);
    model->item(row, 3)->setText(i.value().t_disconnected.isValid() ? pretty_date_time(i.value().t_disconnected.toLocalTime(), QDateTime(), false) : QString());
    model->item(row, 3)->setData(i.value().t_disconnected, Qt::UserRole+2);
    model->item(row, 4)->setText(pretty_size(i.value().bytes_rcv, 1));
    model->item(row, 4)->setData(i.value().bytes_rcv, Qt::UserRole+2);
    model->item(row, 5)->setText(pretty_size(i.value().bytes_snd, 1));
    model->item(row, 5)->setData(i.value().bytes_snd, Qt::UserRole+2);
    QString status;
    if (!i.value().errorstring.isEmpty())
      status = i.value().errorstring;
    else if (!i.value().t_disconnected.isValid() && !(i.value().flags & TunnelConnInInfo::FL_OUT_CONNECTED))
      status = tr("Connecting");
    else if (!i.value().t_disconnected.isValid())
      status = tr("Connected");
    else
      status = tr("Disconnected");
    model->item(row, 6)->setText(status);
    model->item(row, 6)->setData(status, Qt::UserRole+2);
    if (i.value().t_disconnected.isValid())
    {
      for (int col=0; col < model->columnCount(); col++)
        model->item(row, col)->setForeground(QBrush(Qt::gray));
    }
    else if (model->item(row, 0)->foreground() == QBrush(Qt::gray))
    {
      for (int col=0; col < model->columnCount(); col++)
        model->item(row, col)->setForeground(QBrush());
    }
  }
  for (int row=model->rowCount()-1; row >= 0; row--)
  {
    if (!existing_rows.contains(row))
      model->removeRow(row);
  }
  sort_conn_list();
  ui->tableView->setUpdatesEnabled(true);

  if (first_run)
    ui->tableView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
  ui->tableView->setEnabled(true);
}

//---------------------------------------------------------------------------
WidgetTunnelConnList::~WidgetTunnelConnList()
{
  delete ui;
}
