/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "widget_mgrclient.h"
#include "ui_widget_mgrclient.h"
#include "../lib/mgrclient-conn.h"
#include "../lib/ssl_helper.h"
#include "gui_settings.h"
#include <QFileDialog>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslCipher>
#include <QSslSocket>
#include <QMessageBox>
#include <QCompleter>

//---------------------------------------------------------------------------
WidgetMgrClient::WidgetMgrClient(int hide_flags, QWidget *parent): QWidget(parent), ui(new Ui::WidgetMgrClient)
{
  params = NULL;
  ui->setupUi(this);

  connect(ui->edtName, SIGNAL(textEdited(QString)), this, SIGNAL(paramsModified()));
  connect(ui->chkEnabled, SIGNAL(clicked(bool)), this, SIGNAL(paramsModified()));

  connect(ui->edtHost, SIGNAL(textEdited(QString)), this, SIGNAL(paramsModified()));
  connect(ui->edtPort, SIGNAL(valueChanged(int)), this, SLOT(spinBox_valueChanged(int)));

  connect(ui->edtCertFile,       SIGNAL(textEdited(QString)), this, SIGNAL(paramsModified()));
  connect(ui->edtServerCertFile, SIGNAL(textEdited(QString)), this, SIGNAL(paramsModified()));
  connect(ui->edtPrivateKeyFile, SIGNAL(textEdited(QString)), this, SIGNAL(paramsModified()));

  connect(ui->chkAuthPwd,  SIGNAL(toggled(bool)), this, SLOT(chkAuthType_toggled(bool)));
  connect(ui->chkAuthCert, SIGNAL(toggled(bool)), this, SLOT(chkAuthType_toggled(bool)));

  connect(ui->edtUsername, SIGNAL(textEdited(QString)), this, SIGNAL(paramsModified()));
  connect(ui->edtPassword, SIGNAL(textEdited(QString)), this, SIGNAL(paramsModified()));

  connect(ui->chkConnAuto,   SIGNAL(clicked(bool)), this, SLOT(chkConnType_clicked(bool)));
  connect(ui->chkConnDemand, SIGNAL(clicked(bool)), this, SLOT(chkConnType_clicked(bool)));

  connect(ui->edtConnTimeout,              SIGNAL(valueChanged(double)), this, SLOT(doubleSpinBox_valueChanged(double)));
  connect(ui->edtIdleTimeout,              SIGNAL(valueChanged(double)), this, SLOT(doubleSpinBox_valueChanged(double)));
  connect(ui->edtReconnInterval,           SIGNAL(valueChanged(double)), this, SLOT(doubleSpinBox_valueChanged(double)));
  connect(ui->edtReconnIntervalMultiplier, SIGNAL(valueChanged(double)), this, SLOT(doubleSpinBox_valueChanged(double)));
  connect(ui->edtReconnIntervalMax,        SIGNAL(valueChanged(double)), this, SLOT(doubleSpinBox_valueChanged(double)));
  connect(ui->edtIOBufferSize,             SIGNAL(valueChanged(double)), this, SLOT(doubleSpinBox_valueChanged(double)));
  connect(ui->edtPingInterval,             SIGNAL(valueChanged(double)), this, SLOT(doubleSpinBox_valueChanged(double)));
  connect(ui->edtRcvTimeout,               SIGNAL(valueChanged(double)), this, SLOT(doubleSpinBox_valueChanged(double)));

  connect(ui->chkCompression,  SIGNAL(clicked(bool)), this, SIGNAL(paramsModified()));
  connect(ui->chkHeartbeat,    SIGNAL(clicked(bool)), this, SIGNAL(paramsModified()));
  connect(ui->chkNoEncryption, SIGNAL(clicked(bool)), this, SIGNAL(paramsModified()));
  connect(ui->chkTcpKeepAlive, SIGNAL(clicked(bool)), this, SIGNAL(paramsModified()));
  connect(ui->chkTcpLowDelay,  SIGNAL(clicked(bool)), this, SIGNAL(paramsModified()));

  connect(ui->selectProtocol,  SIGNAL(currentIndexChanged(int)), this, SIGNAL(paramsModified()));

  ui->gridLayoutExtra->setAlignment(ui->chkHeartbeat, Qt::AlignRight | Qt::AlignVCenter);

  ui->selectProtocol->addItem(tr("Auto"), (int)QSsl::SecureProtocols);
#if QT_VERSION >= 0x050600
  ui->selectProtocol->addItem(tr("TLSv1.2"), (int)QSsl::TlsV1_2OrLater);
  ui->selectProtocol->addItem(tr("TLSv1.1"), (int)QSsl::TlsV1_1OrLater);
  ui->selectProtocol->addItem(tr("TLSv1.0"), (int)QSsl::TlsV1_0OrLater);
#elif QT_VERSION >= 0x050200
  ui->selectProtocol->addItem(tr("TLSv1.2"), (int)QSsl::TlsV1_2);
  ui->selectProtocol->addItem(tr("TLSv1.1"), (int)QSsl::TlsV1_1);
  ui->selectProtocol->addItem(tr("TLSv1.0"), (int)QSsl::TlsV1_0);
#else
  ui->selectProtocol->addItem(tr("TLSv1"), (int)QSsl::TlsV1);
#endif
  ui->selectProtocol->addItem(tr("SSLv3"), (int)QSsl::SslV3);
  ui->selectProtocol->addItem(tr("SSLv2"), (int)QSsl::SslV2);
  ui->selectProtocol->addItem(tr("Any protocol"), (int)QSsl::AnyProtocol);

  edtAllowedCiphers = new MCLineEdit;
  edtAllowedCiphers->setSeparator(":");
  ui->vLayoutCiphers->insertWidget(0, edtAllowedCiphers);
  connect(edtAllowedCiphers, SIGNAL(textEdited(QString)), this, SIGNAL(paramsModified()));
  if (!(hide_flags & FL_NO_DEFAULT_CIPHERS))
  {
    QList<QSslCipher> ciphers = QSslSocket::defaultCiphers();
    QStringList cipher_names;
    for (int i=0; i < ciphers.count(); i++)
    {
      cipher_names.append(ciphers[i].name());
    }
    QCompleter *completer = new QCompleter(cipher_names, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    edtAllowedCiphers->setMultipleCompleter(completer);
  }
  if (hide_flags & FL_NO_SSL_KEY_CERT)
  {
    ui->chkAuthCert->setText(tr("by X.509 certificate (use tunnel server certificate)"));
    ui->grpAuthCert->setVisible(false);
    ui->vLayout->removeItem(ui->vSpacer2);
    delete ui->vSpacer2;
  }
  else
    ui->chkAuthCert->setText(tr("by X.509 certificate (set above)"));
  if (hide_flags & FL_NO_NAME)
  {
    ui->lblName->setVisible(false);
    ui->edtName->setVisible(false);
  }
  if (hide_flags & FL_NO_ENABLED)
    ui->chkEnabled->setVisible(false);
  if (hide_flags & FL_NO_CONN_TYPE)
  {
    ui->chkConnAuto->setVisible(false);
    ui->chkConnDemand->setVisible(false);
    ui->lblIdleTimeout->setVisible(false);
    ui->edtIdleTimeout->setVisible(false);
    ui->vLayout->removeItem(ui->vSpacer1);
    delete ui->vSpacer1;
  }
  if (hide_flags & FL_SHOW_EXTRA)
  {
    ui->btnToggleExtraSettings->setVisible(false);
    delete ui->hLayoutToggleExtra->takeAt(0);
  }
  else
    on_btnToggleExtraSettings_toggled(false);

  QFont fnt = ui->scrollAreaWidgetContents->font();
  fnt.setPointSizeF(fnt.pointSizeF()*0.9);
  ui->btnToggleExtraSettings->setFont(fnt);
  ui->scrollAreaWidgetContents->setFont(fnt);

  if (!QSslSocket::supportsSsl())
  {
    ui->btnBrowseCertFile->setEnabled(false);
    ui->btnBrowsePrivateKeyFile->setEnabled(false);
    ui->btnBrowseServerCertFile->setEnabled(false);
    ui->edtCertFile->setEnabled(false);
    ui->edtPrivateKeyFile->setEnabled(false);
    ui->edtServerCertFile->setEnabled(false);
    ui->chkNoEncryption->setEnabled(false);
    ui->chkNoEncryption->setChecked(true);
  }
}

//---------------------------------------------------------------------------
void WidgetMgrClient::ui_load(cJSON *json)
{
  cJSON *j_section = cJSON_GetObjectItem(json, "WidgetMgrClient");
  if (!j_section)
    return;
  cJSON *j = cJSON_GetObjectItem(j_section, "WidgetMgrClient_showExtraSettings");
  ui->btnToggleExtraSettings->setChecked(j && j->type == cJSON_True);
  on_btnToggleExtraSettings_toggled(ui->btnToggleExtraSettings->isChecked());
}

//---------------------------------------------------------------------------
void WidgetMgrClient::ui_save(cJSON *json)
{
  cJSON *j_section = cJSON_GetObjectItem(json, "WidgetMgrClient");
  if (j_section)
    cJSON_DeleteItemFromObject(json, "WidgetMgrClient");
  j_section = cJSON_CreateObject();
  cJSON_AddItemToObject(json, "WidgetMgrClient", j_section);

  if (ui->btnToggleExtraSettings->isChecked())
    cJSON_AddTrueToObject(j_section, "WidgetMgrClient_showExtraSettings");
}

//---------------------------------------------------------------------------
void WidgetMgrClient::clearParams()
{
  MgrClientParameters null_params;
  loadParams(&null_params);
  params = NULL;
}

//---------------------------------------------------------------------------
void WidgetMgrClient::showEvent(QShowEvent *event)
{
  QWidget::showEvent(event);
  if (!ui->btnToggleExtraSettings->isVisible())
    ui->scrollAreaExtraSettings->setVisible(true);
}

//---------------------------------------------------------------------------
void WidgetMgrClient::loadParams(MgrClientParameters *_params)
{
  if (_params)
    params = _params;
  if (!params)
    return;

  if (ui->edtName->isVisible())
    ui->edtName->setText(params->name);
  if (ui->chkEnabled->isVisible())
    ui->chkEnabled->setChecked(params->enabled);
  ui->edtHost->setText(params->host);
  ui->edtPort->setValue(params->port);
  if (!(params->flags & MgrClientParameters::FL_DISABLE_ENCRYPTION))
    ui->chkAuthCert->setChecked(true);
  if (!params->auth_username.isEmpty() || !params->auth_password.isEmpty())
    ui->chkAuthPwd->setChecked(true);
  chkAuthType_toggled(ui->chkAuthCert->isChecked());

  if (ui->grpAuthCert->isVisible())
  {
    ui->edtCertFile->setText(params->ssl_cert_filename);
    ui->edtPrivateKeyFile->setText(params->private_key_filename);
  }
  ui->edtServerCertFile->setText(params->server_ssl_cert_filename);

  ui->edtUsername->setText(params->auth_username);
  ui->edtPassword->setText(params->auth_password);

  if (ui->chkConnAuto->isVisible())
  {
    ui->chkConnAuto->setChecked(params->conn_type == MgrClientParameters::CONN_AUTO);
    ui->chkConnDemand->setChecked(params->conn_type == MgrClientParameters::CONN_DEMAND);
    on_chkConnDemand_toggled(ui->chkConnDemand->isChecked());
    ui->edtIdleTimeout->setValue((double)params->idle_timeout/1000);
  }

  ui->edtConnTimeout->setValue((double)params->conn_timeout/1000);
  ui->edtReconnInterval->setValue((double)params->reconnect_interval/1000);
  ui->edtReconnIntervalMultiplier->setValue(params->reconnect_interval_multiplicator);
  ui->edtReconnIntervalMax->setValue((double)params->reconnect_interval_max/1000);
  ui->edtIOBufferSize->setValue((double)params->max_io_buffer_size/(1024*1024));
  ui->edtPingInterval->setValue((double)params->ping_interval/1000);
  ui->edtRcvTimeout->setValue((double)params->rcv_timeout/1000);
  ui->chkCompression->setChecked(params->flags & MgrClientParameters::FL_USE_COMPRESSION);
  ui->chkHeartbeat->setChecked(params->flags & MgrClientParameters::FL_ENABLE_HEARTBEATS);
  on_chkHeartbeat_toggled(ui->chkHeartbeat->isChecked());
  ui->chkNoEncryption->setChecked((params->flags & MgrClientParameters::FL_DISABLE_ENCRYPTION) || !QSslSocket::supportsSsl());
  on_chkNoEncryption_toggled(ui->chkNoEncryption->isChecked());
  ui->chkTcpKeepAlive->setChecked(params->flags & MgrClientParameters::FL_TCP_KEEP_ALIVE);
  ui->chkTcpLowDelay->setChecked(params->flags & MgrClientParameters::FL_TCP_NO_DELAY);
  for (int i=0; i < ui->selectProtocol->count(); i++)
  {
    if (ui->selectProtocol->itemData(i).toInt() == params->ssl_protocol)
    {
      ui->selectProtocol->setCurrentIndex(i);
      break;
    }
  }
  edtAllowedCiphers->setText(params->allowed_ciphers);
}

//---------------------------------------------------------------------------
void WidgetMgrClient::saveParams(MgrClientParameters *_params)
{
  if (_params)
    params = _params;
  if (!params)
    return;

  if (ui->edtName->isVisible())
    params->name = ui->edtName->text().trimmed();
  if (ui->chkEnabled->isVisible())
    params->enabled = ui->chkEnabled->isChecked();
  params->host = ui->edtHost->text().trimmed();
  params->port = ui->edtPort->value();
  if (ui->grpAuthCert->isVisible())
  {
    params->ssl_cert_filename = ui->edtCertFile->text();
    params->private_key_filename = ui->edtPrivateKeyFile->text();
  }
  params->server_ssl_cert_filename = ui->edtServerCertFile->text();
  params->auth_username = ui->edtUsername->text();
  params->auth_password = ui->edtPassword->text();
  if (ui->chkConnAuto->isVisible())
  {
    params->conn_type = ui->chkConnAuto->isChecked() ? MgrClientParameters::CONN_AUTO : MgrClientParameters::CONN_DEMAND;
    params->idle_timeout = ui->edtIdleTimeout->value()*1000;
  }
  params->conn_timeout = ui->edtConnTimeout->value()*1000;
  params->reconnect_interval = ui->edtReconnInterval->value()*1000;
  params->reconnect_interval_multiplicator = ui->edtReconnIntervalMultiplier->value();
  params->reconnect_interval_max = ui->edtReconnIntervalMax->value()*1000;
  params->max_io_buffer_size = ui->edtIOBufferSize->value()*1024*1024;
  params->ping_interval = ui->edtPingInterval->value()*1000;
  params->rcv_timeout = ui->edtRcvTimeout->value()*1000;
  if (ui->chkCompression->isChecked())
    params->flags |= MgrClientParameters::FL_USE_COMPRESSION;
  else
    params->flags &= ~MgrClientParameters::FL_USE_COMPRESSION;
  if (ui->chkHeartbeat->isChecked())
    params->flags |= MgrClientParameters::FL_ENABLE_HEARTBEATS;
  else
    params->flags &= ~MgrClientParameters::FL_ENABLE_HEARTBEATS;
  if (ui->chkNoEncryption->isChecked())
    params->flags |= MgrClientParameters::FL_DISABLE_ENCRYPTION;
  else
    params->flags &= ~MgrClientParameters::FL_DISABLE_ENCRYPTION;
  if (ui->chkTcpKeepAlive->isChecked())
    params->flags |= MgrClientParameters::FL_TCP_KEEP_ALIVE;
  else
    params->flags &= ~MgrClientParameters::FL_TCP_KEEP_ALIVE;
  if (ui->chkTcpLowDelay->isChecked())
    params->flags |= MgrClientParameters::FL_TCP_NO_DELAY;
  else
    params->flags &= ~MgrClientParameters::FL_TCP_NO_DELAY;
  params->ssl_protocol = (QSsl::SslProtocol)ui->selectProtocol->itemData(ui->selectProtocol->currentIndex()).toInt();
  params->allowed_ciphers = edtAllowedCiphers->text().trimmed();

  if (ui->grpAuthCert->isVisible() && (!params->ssl_cert_filename.isEmpty() || !params->private_key_filename.isEmpty()))
  {
    if (!QFile::exists(params->ssl_cert_filename))
    {
      QMessageBox::critical(this, tr("Invalid SSL certificate"), tr("SSL Certificate file %1 does not exists").arg(params->ssl_cert_filename));
      return;
    }
    QSslCertificate cert = loadSSLCertFromFile(params->ssl_cert_filename);
    if (cert.isNull())
    {
      QMessageBox::critical(this, tr("Invalid SSL certificate"), tr("File %1 does not contain any valid SSL certificate").arg(params->ssl_cert_filename));
      return;
    }
    if (!QFile::exists(params->private_key_filename))
    {
      QMessageBox::critical(this, tr("Invalid private key"), tr("Private key file %1 does not exists").arg(params->private_key_filename));
      return;
    }
    QFile f(params->private_key_filename);
    if (!f.open(QIODevice::ReadOnly))
    {
      QMessageBox::critical(this, tr("Invalid private key"), tr("Unable to open private key file %1").arg(params->private_key_filename));
      return;
    }
    QByteArray pkey_buffer = f.readAll();
    f.close();
    if (!pkey_buffer.contains("ENCRYPTED"))
    {
      QSslKey pkey(pkey_buffer, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
      if (pkey.isNull())
      {
        QMessageBox::critical(this, tr("Invalid private key"), tr("File %1 does not contain any valid private key").arg(params->private_key_filename));
        return;
      }
      if (!isCorrectKeyCertificatePair(pkey, cert))
      {
        QMessageBox::critical(this, tr("Invalid SSL pair"), tr("Private key file %1 does not correspond with certificate file %2").arg(params->private_key_filename).arg(params->ssl_cert_filename));
        return;
      }
    }
  }
  if (!params->server_ssl_cert_filename.isEmpty())
  {
    if (!QFile::exists(params->server_ssl_cert_filename))
    {
      QMessageBox::critical(this, tr("Invalid SSL certificate"), tr("SSL Certificate file %1 does not exists").arg(params->server_ssl_cert_filename));
      return;
    }
    QSslCertificate cert = loadSSLCertFromFile(params->server_ssl_cert_filename);
    if (cert.isNull())
    {
      QMessageBox::critical(this, tr("Invalid SSL certificate"), tr("File %1 does not contain any valid SSL certificate").arg(params->server_ssl_cert_filename));
      return;
    }
  }
}

//---------------------------------------------------------------------------
void WidgetMgrClient::on_btnToggleExtraSettings_toggled(bool checked)
{
  ui->btnToggleExtraSettings->setText(checked ? tr("Hide additional options <<") : tr("Show additional options >>"));
  ui->scrollAreaExtraSettings->setVisible(checked);
}

//---------------------------------------------------------------------------
void WidgetMgrClient::spinBox_valueChanged(int value)
{
  if (!params)
    return;

  QSpinBox *edt = qobject_cast<QSpinBox *>(sender());
  int old_value = edt->value();
  if (edt == ui->edtPort)
    old_value = params->port;

  if (old_value != value)
    emit paramsModified();
}

//---------------------------------------------------------------------------
void WidgetMgrClient::doubleSpinBox_valueChanged(double value)
{
  if (!params)
    return;
  QDoubleSpinBox *edt = qobject_cast<QDoubleSpinBox *>(sender());
  int old_value = edt->value();
  if (edt == ui->edtConnTimeout)
    old_value = params->conn_timeout;
  else if (edt == ui->edtIdleTimeout)
    old_value = params->idle_timeout;
  else if (edt == ui->edtReconnInterval)
    old_value = params->reconnect_interval;

  if (old_value != value)
    emit paramsModified();
}

//---------------------------------------------------------------------------
void WidgetMgrClient::setMgrConnName(const QString &new_name)
{
  if (ui->edtName->text() != new_name)
  {
    ui->edtName->setText(new_name);
    emit paramsModified();
  }
}

//---------------------------------------------------------------------------
void WidgetMgrClient::chkConnType_clicked(bool)
{
  QRadioButton *chk = qobject_cast<QRadioButton *>(sender());
  if (!chk)
    return;

  ui->chkConnAuto->setChecked(chk == ui->chkConnAuto);
  ui->chkConnDemand->setChecked(chk == ui->chkConnDemand);

  emit paramsModified();
}

//---------------------------------------------------------------------------
void WidgetMgrClient::on_chkConnDemand_toggled(bool checked)
{
  ui->lblIdleTimeout->setEnabled(checked);
  ui->edtIdleTimeout->setEnabled(checked);
}

//---------------------------------------------------------------------------
void WidgetMgrClient::chkAuthType_toggled(bool)
{
  if (!ui->chkAuthPwd->isChecked() && !ui->chkAuthCert->isChecked())
    ui->chkAuthCert->setChecked(true);
  ui->grpAuthPwd->setEnabled(ui->chkAuthPwd->isChecked());

  emit paramsModified();
}

//---------------------------------------------------------------------------
void WidgetMgrClient::on_chkNoEncryption_toggled(bool checked)
{
  if (checked)
  {
    ui->chkAuthPwd->setChecked(true);
    ui->chkAuthCert->setChecked(false);
    chkAuthType_toggled(ui->chkAuthCert->isChecked());
  }
  ui->chkAuthCert->setEnabled(!checked);
  ui->edtServerCertFile->setEnabled(!checked);
  ui->btnBrowseServerCertFile->setEnabled(!checked);
  ui->lblServerCertFile->setEnabled(!checked);
  ui->lblSelectProtocol->setEnabled(!checked);
  ui->selectProtocol->setEnabled(!checked);
  ui->lblAllowedCiphers->setEnabled(!checked);
  ui->widgetCiphers->setEnabled(!checked);
}

//---------------------------------------------------------------------------
void WidgetMgrClient::on_chkHeartbeat_toggled(bool checked)
{
  ui->edtPingInterval->setEnabled(checked);
  ui->lblPingInterval->setEnabled(checked);
}

//---------------------------------------------------------------------------
bool WidgetMgrClient::canApply()
{
  bool can_apply = !ui->edtName->text().trimmed().isEmpty()
                && !ui->edtHost->text().trimmed().isEmpty();
  return can_apply;
}

//---------------------------------------------------------------------------
void WidgetMgrClient::on_btnBrowseCertFile_clicked()
{
  QFileDialog fileDialog(this);
  fileDialog.setFileMode(QFileDialog::ExistingFile);
  fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
  fileDialog.setNameFilters(QStringList() << "CRT files (*.crt)" << "PEM format (*.pem)" << "Any files (*)");

  cJSON *j_fileDialogs = cJSON_GetObjectItem(j_gui_settings, "fileDialogs");
  if (!j_fileDialogs)
  {
    j_fileDialogs = cJSON_CreateObject();
    cJSON_AddItemToObject(j_gui_settings, "fileDialogs", j_fileDialogs);
  }
  cJSON *j_fileDialog_SSLCert = cJSON_GetObjectItem(j_fileDialogs, "SSLCert");
  if (j_fileDialog_SSLCert)
  {
    cJSON *j;
    j = cJSON_GetObjectItem(j_fileDialog_SSLCert, "geometry");
    if (j && j->type == cJSON_String)
      fileDialog.restoreGeometry(QByteArray::fromBase64(j->valuestring));
    j = cJSON_GetObjectItem(j_fileDialog_SSLCert, "state");
    if (j && j->type == cJSON_String)
      fileDialog.restoreState(QByteArray::fromBase64(j->valuestring));
    j = cJSON_GetObjectItem(j_fileDialog_SSLCert, "filter");
    if (j && j->type == cJSON_String)
      fileDialog.selectNameFilter(QString::fromUtf8(j->valuestring));
  }
  if (!ui->edtCertFile->text().isEmpty())
  {
    QFileInfo fi(ui->edtCertFile->text());
    if (QFile::exists(fi.path()))
      fileDialog.setDirectory(fi.path());
  }
  if (!fileDialog.exec())
    return;

  if (j_fileDialog_SSLCert)
    cJSON_DeleteItemFromObject(j_fileDialogs, "SSLCert");
  j_fileDialog_SSLCert = cJSON_CreateObject();
  cJSON_AddItemToObject(j_fileDialogs, "SSLCert", j_fileDialog_SSLCert);

  cJSON_AddStringToObject(j_fileDialog_SSLCert, "geometry", fileDialog.saveGeometry().toBase64());
  cJSON_AddStringToObject(j_fileDialog_SSLCert, "state", fileDialog.saveState().toBase64());
  cJSON_AddStringToObject(j_fileDialog_SSLCert, "filter", fileDialog.selectedNameFilter().toUtf8());

  ::ui_save();

  if (fileDialog.selectedFiles().isEmpty())
    return;
  QString filename = fileDialog.selectedFiles().at(0);
  QSslCertificate crt = loadSSLCertFromFile(filename);
  if (crt.isNull())
  {
    ui->edtCertFile->clear();
    QMessageBox::critical(this, tr("Invalid SSL certificate"), tr("File %1 does not contain any valid SSL certificate").arg(filename));
    return;
  }

  ui->edtCertFile->setText(filename);
  emit paramsModified();
}

//---------------------------------------------------------------------------
void WidgetMgrClient::on_btnBrowseServerCertFile_clicked()
{
  QFileDialog fileDialog(this);
  fileDialog.setFileMode(QFileDialog::ExistingFile);
  fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
  fileDialog.setNameFilters(QStringList() << "CRT files (*.crt)" << "PEM format (*.pem)" << "Any files (*)");

  cJSON *j_fileDialogs = cJSON_GetObjectItem(j_gui_settings, "fileDialogs");
  if (!j_fileDialogs)
  {
    j_fileDialogs = cJSON_CreateObject();
    cJSON_AddItemToObject(j_gui_settings, "fileDialogs", j_fileDialogs);
  }
  cJSON *j_fileDialog_SSLCert = cJSON_GetObjectItem(j_fileDialogs, "SSLCert");
  if (j_fileDialog_SSLCert)
  {
    cJSON *j;
    j = cJSON_GetObjectItem(j_fileDialog_SSLCert, "geometry");
    if (j && j->type == cJSON_String)
      fileDialog.restoreGeometry(QByteArray::fromBase64(j->valuestring));
    j = cJSON_GetObjectItem(j_fileDialog_SSLCert, "state");
    if (j && j->type == cJSON_String)
      fileDialog.restoreState(QByteArray::fromBase64(j->valuestring));
    j = cJSON_GetObjectItem(j_fileDialog_SSLCert, "filter");
    if (j && j->type == cJSON_String)
      fileDialog.selectNameFilter(QString::fromUtf8(j->valuestring));
  }
  if (!ui->edtServerCertFile->text().isEmpty())
  {
    QFileInfo fi(ui->edtServerCertFile->text());
    if (QFile::exists(fi.path()))
      fileDialog.setDirectory(fi.path());
  }
  if (!fileDialog.exec())
    return;

  if (j_fileDialog_SSLCert)
    cJSON_DeleteItemFromObject(j_fileDialogs, "SSLCert");
  j_fileDialog_SSLCert = cJSON_CreateObject();
  cJSON_AddItemToObject(j_fileDialogs, "SSLCert", j_fileDialog_SSLCert);

  cJSON_AddStringToObject(j_fileDialog_SSLCert, "geometry", fileDialog.saveGeometry().toBase64());
  cJSON_AddStringToObject(j_fileDialog_SSLCert, "state", fileDialog.saveState().toBase64());
  cJSON_AddStringToObject(j_fileDialog_SSLCert, "filter", fileDialog.selectedNameFilter().toUtf8());

  ::ui_save();

  if (fileDialog.selectedFiles().isEmpty())
    return;
  QString filename = fileDialog.selectedFiles().at(0);
  QSslCertificate crt = loadSSLCertFromFile(filename);
  if (crt.isNull())
  {
    ui->edtServerCertFile->clear();
    QMessageBox::critical(this, tr("Invalid SSL certificate"), tr("File %1 does not contain any valid SSL certificate").arg(filename));
    return;
  }

  ui->edtServerCertFile->setText(filename);
  emit paramsModified();
}

//---------------------------------------------------------------------------
void WidgetMgrClient::on_btnBrowsePrivateKeyFile_clicked()
{
  QFileDialog fileDialog(this);
  fileDialog.setFileMode(QFileDialog::ExistingFile);
  fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
  fileDialog.setNameFilters(QStringList() << "PEM format (*.pem)" << "KEY files (*.key)" << "Any files (*)");

  cJSON *j_fileDialogs = cJSON_GetObjectItem(j_gui_settings, "fileDialogs");
  if (!j_fileDialogs)
  {
    j_fileDialogs = cJSON_CreateObject();
    cJSON_AddItemToObject(j_gui_settings, "fileDialogs", j_fileDialogs);
  }
  cJSON *j_fileDialog_PrivateKey = cJSON_GetObjectItem(j_fileDialogs, "PrivateKey");
  if (j_fileDialog_PrivateKey)
  {
    cJSON *j;
    j = cJSON_GetObjectItem(j_fileDialog_PrivateKey, "geometry");
    if (j && j->type == cJSON_String)
      fileDialog.restoreGeometry(QByteArray::fromBase64(j->valuestring));
    j = cJSON_GetObjectItem(j_fileDialog_PrivateKey, "state");
    if (j && j->type == cJSON_String)
      fileDialog.restoreState(QByteArray::fromBase64(j->valuestring));
    j = cJSON_GetObjectItem(j_fileDialog_PrivateKey, "filter");
    if (j && j->type == cJSON_String)
      fileDialog.selectNameFilter(QString::fromUtf8(j->valuestring));
  }

  if (!ui->edtPrivateKeyFile->text().isEmpty())
  {
    QFileInfo fi(ui->edtPrivateKeyFile->text());
    if (QFile::exists(fi.path()))
      fileDialog.setDirectory(fi.path());
  }
  if (!fileDialog.exec())
    return;

  if (j_fileDialog_PrivateKey)
    cJSON_DeleteItemFromObject(j_fileDialogs, "PrivateKey");
  j_fileDialog_PrivateKey = cJSON_CreateObject();
  cJSON_AddItemToObject(j_fileDialogs, "PrivateKey", j_fileDialog_PrivateKey);

  cJSON_AddStringToObject(j_fileDialog_PrivateKey, "geometry", fileDialog.saveGeometry().toBase64());
  cJSON_AddStringToObject(j_fileDialog_PrivateKey, "state", fileDialog.saveState().toBase64());
  cJSON_AddStringToObject(j_fileDialog_PrivateKey, "filter", fileDialog.selectedNameFilter().toUtf8());

  ::ui_save();

  QString filename = fileDialog.selectedFiles().at(0);
  QFile f(filename);
  if (!f.open(QIODevice::ReadOnly))
  {
    ui->edtPrivateKeyFile->clear();
    QMessageBox::critical(this, tr("Unable to open file"), tr("Unable to open file %1").arg(filename));
    return;
  }
  QByteArray buffer = f.read(1024*1024);
  f.close();
  if (buffer.isEmpty() || buffer.length() >= 1024*1024)
  {
    ui->edtPrivateKeyFile->clear();
    QMessageBox::critical(this, tr("Invalid file"), tr("Invalid private key file %1: empty or too long").arg(filename));
    return;
  }
  // QSslKey cannot load encrypted key if no passphrase provided
  // so we do not check encrypted keys
  if (!buffer.contains("ENCRYPTED"))
  {
    QSslKey key = QSslKey(buffer, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    if (key.isNull())
    {
      QMessageBox::critical(this, tr("Invalid private key"), tr("File %1 does not contain a valid private RSA key in PEM format").arg(filename));
      return;
    }
  }

  ui->edtPrivateKeyFile->setText(filename);
  emit paramsModified();
}

//---------------------------------------------------------------------------
WidgetMgrClient::~WidgetMgrClient()
{
  delete ui;
}
