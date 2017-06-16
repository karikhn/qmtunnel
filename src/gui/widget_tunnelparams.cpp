/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "widget_tunnelparams.h"
#include "ui_widget_tunnelparams.h"
#include <QPushButton>
#include <QDialog>
#include <QCompleter>
#include "widget_mgrclient.h"
#include "../lib/tunnel-state.h"

//---------------------------------------------------------------------------
WidgetTunnelParams::WidgetTunnelParams(QWidget *parent): QWidget(parent), ui(new Ui::WidgetTunnelParams)
{
  ui->setupUi(this);

#if QT_VERSION >= 0x050000
  ui->tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  ui->tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  ui->tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  ui->tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
#else
  ui->tableWidget->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
  ui->tableWidget->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);
  ui->tableWidget->horizontalHeader()->setResizeMode(2, QHeaderView::ResizeToContents);
  ui->tableWidget->horizontalHeader()->setResizeMode(3, QHeaderView::ResizeToContents);
#endif

  connect(ui->edtName, SIGNAL(textEdited(QString)), this, SIGNAL(paramsModified()));
  connect(ui->edtBindAddress, SIGNAL(textEdited(QString)), this, SIGNAL(paramsModified()));
  connect(ui->spinBindPort, SIGNAL(valueChanged(int)), this, SIGNAL(paramsModified()));
  connect(ui->edtRemoteHost, SIGNAL(textEdited(QString)), this, SIGNAL(paramsModified()));
  connect(ui->spinRemotePort, SIGNAL(valueChanged(int)), this, SIGNAL(paramsModified()));
  connect(ui->comboAppProto, SIGNAL(currentIndexChanged(int)), this, SIGNAL(paramsModified()));
  connect(ui->comboDirection, SIGNAL(currentIndexChanged(int)), this, SIGNAL(paramsModified()));
  connect(ui->chkPermanent, SIGNAL(toggled(bool)), this, SIGNAL(paramsModified()));
  connect(ui->spinIdleTimeout, SIGNAL(valueChanged(double)), this, SIGNAL(paramsModified()));
  connect(ui->spinConnectTimeout, SIGNAL(valueChanged(double)), this, SIGNAL(paramsModified()));
  connect(ui->spinFailureToleranceTimeout, SIGNAL(valueChanged(double)), this, SIGNAL(paramsModified()));
  connect(ui->spinMaxIncomingConnections, SIGNAL(valueChanged(int)), this, SIGNAL(paramsModified()));

  QCompleter *bindAddressCompleter = new QCompleter(QStringList()
                                                    << "127.0.0.1"
                                                    << "0.0.0.0",
                                                    this);
  ui->edtBindAddress->setCompleter(bindAddressCompleter);

  tableWidget_addRow();

  QFont fnt = ui->lblSeeSchema->font();
  fnt.setPointSizeF(fnt.pointSizeF()*0.8);
  ui->lblSeeSchema->setFont(fnt);
  ui->lblHostnameRelative->setFont(fnt);
}

//---------------------------------------------------------------------------
void WidgetTunnelParams::on_comboDirection_currentIndexChanged(int index)
{
  ui->lblSchema->setPixmap(QPixmap(index == 0
                                   ? "://images/schemas/shema-local-to-remote.png"
                                   : "://images/schemas/shema-remote-to-local.png"));
  ui->chkPermanent->setEnabled(index == 0);
  if (index == 1)
    ui->chkPermanent->setChecked(true);
}

//---------------------------------------------------------------------------
void WidgetTunnelParams::applyError(quint16 res_code)
{
  switch(res_code)
  {
    case TunnelState::RES_CODE_NAME_ERROR:
    {
      ui->edtName->setFocus();
      break;
    }
    case TunnelState::RES_CODE_BIND_ERROR:
    {
      ui->edtBindAddress->setFocus();
      break;
    }
    case TunnelState::RES_CODE_REMOTE_HOST_ERROR:
    {
      ui->edtRemoteHost->setFocus();
      break;
    }
    default:
      break;
  }
}

//---------------------------------------------------------------------------
void WidgetTunnelParams::tableWidget_addRow()
{
  int row = ui->tableWidget->rowCount();
  ui->tableWidget->setRowCount(row+1);
  MgrClientParameters *mgrconn_params = new MgrClientParameters;
  QTableWidgetItem *item = new QTableWidgetItem();
  ui->tableWidget->setItem(row, 0, item);
  item->setData(Qt::UserRole, QVariant::fromValue(mgrconn_params));
  QLineEdit *edt = new QLineEdit;
  edt->setMaxLength(255);
  QSpinBox *spin = new QSpinBox;
  spin->setMinimum(1);
  spin->setMaximum(65535);
  spin->setValue(mgrconn_params->port);
  QPushButton *btnSettings = new QPushButton(tr("Settings..."));
  QPushButton *btnRemove = new QPushButton(QIcon("://images/icons/remove16.png"), QString());
  spin->setEnabled(false);
  btnSettings->setEnabled(false);
  btnRemove->setEnabled(false);
  btnSettings->setDefault(false);
  btnRemove->setDefault(false);
  ui->tableWidget->setCellWidget(row,0,edt);
  ui->tableWidget->setCellWidget(row,1,spin);
  ui->tableWidget->setCellWidget(row,2,btnSettings);
  ui->tableWidget->setCellWidget(row,3,btnRemove);

  connect(edt, SIGNAL(textChanged(QString)), this, SLOT(tunserverHostChanged()));
  connect(spin, SIGNAL(valueChanged(int)), this, SLOT(tunserverPortChanged()));
  connect(btnSettings, SIGNAL(clicked()), this, SLOT(tunserverSettingsClicked()));
  connect(btnRemove, SIGNAL(clicked()), this, SLOT(tunserverRemoveClicked()));

  if (row-1 >= 0)
  {
    QPushButton *prev_btnRemove = qobject_cast<QPushButton *>(ui->tableWidget->cellWidget(row-1, 3));
    prev_btnRemove->setEnabled(true);
  }
}

//---------------------------------------------------------------------------
void WidgetTunnelParams::on_comboAppProto_currentIndexChanged(int index)
{
  ui->lblBindPort->setVisible(index+1 != (int)TunnelParameters::PIPE);
  ui->lblRemotePort->setVisible(index+1 != (int)TunnelParameters::PIPE);
  ui->spinBindPort->setVisible(index+1 != (int)TunnelParameters::PIPE);
  ui->spinRemotePort->setVisible(index+1 != (int)TunnelParameters::PIPE);
  ui->widgetConnectTimeout->setVisible(index+1 != (int)TunnelParameters::UDP);
  if (index+1 == (int)TunnelParameters::PIPE)
  {
    ui->edtBindAddress->setPlaceholderText(tr("e.g. /tmp/unixsocket1"));
    ui->edtRemoteHost->setPlaceholderText(tr("e.g. /tmp/unixsocket1"));
  }
  else
  {
    ui->edtBindAddress->setPlaceholderText(tr("e.g. 127.0.0.1 or 0.0.0.0"));
    ui->edtRemoteHost->setPlaceholderText(tr("e.g. 127.0.0.1 or example.com"));
  }
}

//---------------------------------------------------------------------------
void WidgetTunnelParams::on_chkPermanent_toggled(bool checked)
{
  ui->lblIdleTimeout->setEnabled(!checked);
  ui->spinIdleTimeout->setEnabled(!checked);
}

//---------------------------------------------------------------------------
void WidgetTunnelParams::tunserverHostChanged()
{
  QLineEdit *edt = qobject_cast<QLineEdit *>(sender());
  for (int row=0; row < ui->tableWidget->rowCount(); row++)
  {
    if (ui->tableWidget->cellWidget(row, 0) == edt)
    {
      MgrClientParameters *mgrconn_params = ui->tableWidget->item(row, 0)->data(Qt::UserRole).value<MgrClientParameters *>();
      QSpinBox *spin = qobject_cast<QSpinBox *>(ui->tableWidget->cellWidget(row, 1));
      QPushButton *btnSettings = qobject_cast<QPushButton *>(ui->tableWidget->cellWidget(row, 2));
      QPushButton *btnRemove = qobject_cast<QPushButton *>(ui->tableWidget->cellWidget(row, 3));
      spin->setEnabled(!mgrconn_params->host.isEmpty());
      btnSettings->setEnabled(!mgrconn_params->host.isEmpty());
      btnRemove->setEnabled(ui->tableWidget->rowCount() > 1);
      if (!mgrconn_params->host.isEmpty() && row == ui->tableWidget->rowCount()-1)
        tableWidget_addRow();
      if (mgrconn_params->host != edt->text().trimmed())
        emit paramsModified();
      mgrconn_params->host = edt->text().trimmed();
      break;
    }
  }
}

//---------------------------------------------------------------------------
void WidgetTunnelParams::setTunnelName(const QString &name)
{
  if (ui->edtName->text() != name)
  {
    ui->edtName->setText(name);
    emit paramsModified();
  }
}

//---------------------------------------------------------------------------
void WidgetTunnelParams::tunserverPortChanged()
{
  QSpinBox *spin = qobject_cast<QSpinBox *>(sender());
  for (int row=0; row < ui->tableWidget->rowCount(); row++)
  {
    if (ui->tableWidget->cellWidget(row, 1) == spin)
    {
      MgrClientParameters *mgrconn_params = ui->tableWidget->item(row, 0)->data(Qt::UserRole).value<MgrClientParameters *>();
      if (mgrconn_params->port != spin->value())
      {
        mgrconn_params->port = spin->value();
        emit paramsModified();
      }
      break;
    }
  }
}

//---------------------------------------------------------------------------
void WidgetTunnelParams::tunserverSettingsClicked()
{
  QPushButton *btnSettings = qobject_cast<QPushButton *>(sender());
  for (int row=0; row < ui->tableWidget->rowCount(); row++)
  {
    if (ui->tableWidget->cellWidget(row, 2) == btnSettings)
    {
      MgrClientParameters *mgrconn_params = ui->tableWidget->item(row, 0)->data(Qt::UserRole).value<MgrClientParameters *>();
      QLineEdit *edt = qobject_cast<QLineEdit *>(ui->tableWidget->cellWidget(row, 0));
      QSpinBox *spin = qobject_cast<QSpinBox *>(ui->tableWidget->cellWidget(row, 1));
      QDialog settingsDialog(this);
      settingsDialog.setWindowTitle(tr("Tunnel server connection settings"));
      QVBoxLayout *vbox = new QVBoxLayout();
      settingsDialog.setLayout(vbox);
      WidgetMgrClient *mgrconn_widget = new WidgetMgrClient(WidgetMgrClient::FL_NO_NAME
                                                    | WidgetMgrClient::FL_NO_DEFAULT_CIPHERS
                                                    | WidgetMgrClient::FL_NO_SSL_KEY_CERT
                                                    | WidgetMgrClient::FL_NO_ENABLED
                                                    | WidgetMgrClient::FL_SHOW_EXTRA
                                                    | WidgetMgrClient::FL_NO_CONN_TYPE);
      mgrconn_widget->loadParams(mgrconn_params);
      vbox->addWidget(mgrconn_widget);
      QHBoxLayout *hbox = new QHBoxLayout();
      vbox->addLayout(hbox);
      QPushButton *btnOK = new QPushButton(tr("OK"));
      hbox->addWidget(btnOK);
      QPushButton *btnCancel = new QPushButton(tr("Cancel"));
      hbox->addWidget(btnCancel);
      connect(btnOK, SIGNAL(clicked()), &settingsDialog, SLOT(accept()));
      connect(btnCancel, SIGNAL(clicked()), &settingsDialog, SLOT(reject()));
      mgrconn_widget->on_btnToggleExtraSettings_toggled(false);
      settingsDialog.resize(600, 500);
      if (settingsDialog.exec() == QDialog::Accepted)
      {
        mgrconn_widget->saveParams(mgrconn_params);
        edt->setText(mgrconn_params->host);
        spin->setValue(mgrconn_params->port);
        emit paramsModified();
      }
      break;
    }
  }
}

//---------------------------------------------------------------------------
void WidgetTunnelParams::tunserverRemoveClicked()
{
  QPushButton *btnRemove = qobject_cast<QPushButton *>(sender());
  for (int row=0; row < ui->tableWidget->rowCount(); row++)
  {
    if (ui->tableWidget->cellWidget(row, 3) == btnRemove)
    {
      MgrClientParameters *mgrconn_params = ui->tableWidget->item(row, 0)->data(Qt::UserRole).value<MgrClientParameters *>();
      delete mgrconn_params;
      ui->tableWidget->removeRow(row);
      emit paramsModified();
      break;
    }
  }
}

//---------------------------------------------------------------------------
void WidgetTunnelParams::tableWidget_clear()
{
  for (int row=ui->tableWidget->rowCount()-1; row >= 0; row--)
  {
    MgrClientParameters *mgrconn_params = ui->tableWidget->item(row, 0)->data(Qt::UserRole).value<MgrClientParameters *>();
    delete mgrconn_params;
    ui->tableWidget->removeRow(row);
  }
}

//---------------------------------------------------------------------------
void WidgetTunnelParams::loadParams(TunnelParameters *params, bool _is_editable)
{
  is_editable = _is_editable;

  this->setEnabled(is_editable);

  ui->edtName->setText(params->name);
  ui->comboAppProto->setCurrentIndex((int)params->app_protocol-1);
  ui->comboDirection->setCurrentIndex((int)params->fwd_direction-1);
  ui->edtBindAddress->setText(params->bind_address);
  ui->spinBindPort->setValue(params->bind_port);
  ui->edtRemoteHost->setText(params->remote_host);
  ui->spinRemotePort->setValue(params->remote_port);
  ui->spinMaxIncomingConnections->setValue(params->max_incoming_connections);

  ui->chkPermanent->setChecked(params->flags & TunnelParameters::FL_PERMANENT_TUNNEL);
  ui->spinIdleTimeout->setValue((double)params->idle_timeout/1000);
  ui->spinConnectTimeout->setValue((double)params->connect_timeout/1000);
  ui->spinFailureToleranceTimeout->setValue((double)params->failure_tolerance_timeout/1000);

  tableWidget_clear();
  tableWidget_addRow();
  for (int row=0; row < params->tunservers.count(); row++)
  {
    MgrClientParameters *mgrconn_params = ui->tableWidget->item(row, 0)->data(Qt::UserRole).value<MgrClientParameters *>();
    *mgrconn_params = params->tunservers[row];
    QLineEdit *edt = qobject_cast<QLineEdit *>(ui->tableWidget->cellWidget(row, 0));
    QSpinBox *spin = qobject_cast<QSpinBox *>(ui->tableWidget->cellWidget(row, 1));
    edt->setText(mgrconn_params->host);
    spin->setValue(mgrconn_params->port);
  }
}

//---------------------------------------------------------------------------
void WidgetTunnelParams::saveParams(TunnelParameters *params)
{
  params->name = ui->edtName->text();
  params->app_protocol = (TunnelParameters::AppProtocol)(ui->comboAppProto->currentIndex()+1);
  params->fwd_direction = (TunnelParameters::FwdDirection)(ui->comboDirection->currentIndex()+1);
  params->bind_address = ui->edtBindAddress->text();
  params->bind_port = ui->spinBindPort->value();
  params->remote_host = ui->edtRemoteHost->text();
  params->remote_port = ui->spinRemotePort->value();
  params->max_incoming_connections = ui->spinMaxIncomingConnections->value();
  params->max_incoming_connections_info = ui->spinMaxIncomingConnections->value()*2;
  if (params->max_incoming_connections_info > 1000)
    params->max_incoming_connections_info = 1000;

  if (ui->chkPermanent->isChecked())
    params->flags |= TunnelParameters::FL_PERMANENT_TUNNEL;
  else
    params->flags &= ~TunnelParameters::FL_PERMANENT_TUNNEL;

  params->idle_timeout = ui->spinIdleTimeout->value()*1000;
  params->connect_timeout = ui->spinConnectTimeout->value()*1000;
  params->failure_tolerance_timeout = ui->spinFailureToleranceTimeout->value()*1000;

  params->tunservers.clear();
  for (int row=0; row < ui->tableWidget->rowCount()-1; row++)
  {
    MgrClientParameters *mgrconn_params = ui->tableWidget->item(row, 0)->data(Qt::UserRole).value<MgrClientParameters *>();
    if (!mgrconn_params->host.isEmpty())
      params->tunservers.append(*mgrconn_params);
  }
}

//---------------------------------------------------------------------------
bool WidgetTunnelParams::canApply()
{
  if (!is_editable)
    return false;
  if (ui->edtName->text().isEmpty())
    return false;
  if (ui->edtBindAddress->text().isEmpty())
    return false;
  if (ui->edtRemoteHost->text().isEmpty())
    return false;
  if (ui->tableWidget->rowCount() < 2)
    return false;
  return true;
}

//---------------------------------------------------------------------------
WidgetTunnelParams::~WidgetTunnelParams()
{
  tableWidget_clear();
  delete ui;
}
