/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "widget_mgrserver.h"
#include "gui_settings.h"
#include <QMessageBox>
#include <QNetworkInterface>
#include <QFileDialog>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include "../lib/ssl_helper.h"
#include "../lib/ssl_keygen.h"
#include "../lib/sys_util.h"

//---------------------------------------------------------------------------
WidgetMgrServer::WidgetMgrServer(bool remote, QWidget *parent): QWidget(parent), ui(new Ui::WidgetMgrServer)
{
  ui->setupUi(this);

  if (!QSslSocket::supportsSsl())
  {
    ui->btnGenerateKeyCert->setEnabled(false);
    ui->btnBrowseMgrSSLCertFile->setEnabled(false);
    ui->btnBrowseMgrPrivateKeyFile->setEnabled(false);
    ui->edtMgrPrivateKeyFile->setEnabled(false);
    ui->edtMgrSSLCertFile->setEnabled(false);
  }

  if (remote)
  {
    ui->btnBrowseMgrSSLCertFile->setVisible(false);
    ui->btnBrowseMgrPrivateKeyFile->setVisible(false);
    ui->btnGenerateKeyCert->setVisible(false);
  }
  else
  {
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    ui->edtMgrListenInterface->addItem(QString("%1 (Any IPv4)").arg(QHostAddress(QHostAddress::Any).toString()), QHostAddress(QHostAddress::Any).toString());
    ui->edtMgrListenInterface->addItem(QString("%1 (Any IPv6&IPv4)").arg(QHostAddress(QHostAddress::AnyIPv6).toString()), QHostAddress(QHostAddress::AnyIPv6).toString());
    for (int i=0; i < interfaces.count(); i++)
    {
      for (int a=0; a < interfaces.at(i).addressEntries().count(); a++)
        ui->edtMgrListenInterface->addItem(interfaces.at(i).addressEntries().at(a).ip().toString()+QString(" (%1)").arg(interfaces.at(i).name()), interfaces.at(i).addressEntries().at(a).ip().toString());
    }
  }

  connect(ui->edtMgrListenInterface, SIGNAL(activated(int)), this, SLOT(mgrServerConfig_valueChanged(int)));
  connect(ui->edtMgrListenPort, SIGNAL(valueChanged(int)), this, SLOT(mgrServerConfig_valueChanged(int)));
  connect(ui->edtMgrPrivateKeyFile, SIGNAL(textEdited(QString)), this, SIGNAL(paramsModified()));
  connect(ui->edtMgrSSLCertFile, SIGNAL(textEdited(QString)), this, SIGNAL(paramsModified()));
  connect(ui->spinMaxIncomingMgrconn, SIGNAL(valueChanged(int)), this, SLOT(mgrServerConfig_valueChanged(int)));
}

//---------------------------------------------------------------------------
WidgetMgrServer::~WidgetMgrServer()
{
  delete ui;
}

//---------------------------------------------------------------------------
bool WidgetMgrServer::loadNetworkInterfaces(cJSON *j_network_interfaces)
{
  ui->edtMgrListenInterface->clear();
  ui->edtMgrListenInterface->setEnabled(false);
  int interface_count = cJSON_GetArraySize(j_network_interfaces);
  if (interface_count == 0)
    return false;
  ui->edtMgrListenInterface->addItem(QString("%1 (Any IPv4)").arg(QHostAddress(QHostAddress::Any).toString()), QHostAddress(QHostAddress::Any).toString());
  ui->edtMgrListenInterface->addItem(QString("%1 (Any IPv6&IPv4)").arg(QHostAddress(QHostAddress::AnyIPv6).toString()), QHostAddress(QHostAddress::AnyIPv6).toString());
  for (int i=0; i < interface_count; i++)
  {
    cJSON *j_item = cJSON_GetArrayItem(j_network_interfaces, i);
    cJSON *j;
    j = cJSON_GetObjectItem(j_item, "name");
    QString iface_name;
    if (j && j->type == cJSON_String)
      iface_name = QString::fromUtf8(j->valuestring);
    j = cJSON_GetObjectItem(j_item, "address");
    QString iface_address;
    if (j && j->type == cJSON_String)
      iface_address = QString::fromUtf8(j->valuestring);
    ui->edtMgrListenInterface->addItem(iface_address+QString(" (%1)").arg(iface_name), iface_address);
  }
  ui->edtMgrListenInterface->setEnabled(true);
  return true;
}

//---------------------------------------------------------------------------
void WidgetMgrServer::loadParams(MgrServerParameters *params)
{
  for (int i=0; i < ui->edtMgrListenInterface->count(); i++)
  {
    if (ui->edtMgrListenInterface->itemData(i).toString() == params->listen_interface.toString())
    {
      ui->edtMgrListenInterface->setCurrentIndex(i);
      break;
    }
  }
  ui->edtMgrListenPort->setValue(params->listen_port);
  ui->edtMgrSSLCertFile->setText(params->ssl_cert_filename);
  ui->edtMgrPrivateKeyFile->setText(params->private_key_filename);

  ui->spinMaxIncomingMgrconn->setValue(params->max_incoming_mgrconn);

  old_listen_interface = params->listen_interface;
  old_listen_port = params->listen_port;
  old_max_incoming_mgrconn = params->max_incoming_mgrconn;
}

//---------------------------------------------------------------------------
void WidgetMgrServer::saveParams(MgrServerParameters *params)
{
  params->listen_interface = QHostAddress(ui->edtMgrListenInterface->itemData(ui->edtMgrListenInterface->currentIndex()).toString());
  params->listen_port = ui->edtMgrListenPort->value();
  params->ssl_cert_filename = ui->edtMgrSSLCertFile->text();
  params->private_key_filename = ui->edtMgrPrivateKeyFile->text();

  params->max_incoming_mgrconn = ui->spinMaxIncomingMgrconn->value();

  old_listen_interface = params->listen_interface;
  old_listen_port = params->listen_port;
  old_max_incoming_mgrconn = params->max_incoming_mgrconn;
}

//---------------------------------------------------------------------------
void WidgetMgrServer::mgrServerConfig_valueChanged(int value)
{
  QSpinBox *spinBox = qobject_cast<QSpinBox*>(sender());
  if (spinBox)
  {
    if (spinBox == ui->edtMgrListenPort)
    {
      if (value != old_listen_port)
        emit paramsModified();
    }
    else if (spinBox == ui->spinMaxIncomingMgrconn)
    {
      if ((quint32)value != old_max_incoming_mgrconn)
        emit paramsModified();
    }
    return;
  }
  QComboBox *comboBox = qobject_cast<QComboBox*>(sender());
  if (comboBox)
  {
    if (comboBox == ui->edtMgrListenInterface)
    {
      if (QHostAddress(ui->edtMgrListenInterface->itemData(value).toString()) != old_listen_interface)
        emit paramsModified();
    }
    return;
  }
}

//---------------------------------------------------------------------------
bool WidgetMgrServer::canApply()
{
  return ui->edtMgrListenInterface->currentIndex() >= 0;
}

//---------------------------------------------------------------------------
void WidgetMgrServer::applyError(quint16 res_code)
{
  switch(res_code)
  {
    case MgrServerParameters::RES_CODE_LISTEN_ERROR:
    {
      ui->edtMgrListenPort->setFocus();
      break;
    }
    case MgrServerParameters::RES_CODE_SSL_CERT_ERROR:
    {
      ui->edtMgrSSLCertFile->setFocus();
      break;
    }
    case MgrServerParameters::RES_CODE_PRIVATE_KEY_ERROR:
    {
      ui->edtMgrPrivateKeyFile->setFocus();
      break;
    }
    default:
      break;
  }
}

//---------------------------------------------------------------------------
void WidgetMgrServer::on_btnBrowseMgrSSLCertFile_clicked()
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
  if (!ui->edtMgrSSLCertFile->text().isEmpty())
  {
    QFileInfo fi(ui->edtMgrSSLCertFile->text());
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

  ui_save();

  if (fileDialog.selectedFiles().isEmpty())
    return;
  QString filename = fileDialog.selectedFiles().at(0);
  QSslCertificate crt = loadSSLCertFromFile(filename);
  if (crt.isNull())
  {
    ui->edtMgrSSLCertFile->clear();
    QMessageBox::critical(this, tr("Invalid SSL certificate"), tr("File %1 does not contain any valid SSL certificate").arg(filename));
    return;
  }

  ui->edtMgrSSLCertFile->setText(filename);
  emit paramsModified();
}

//---------------------------------------------------------------------------
void WidgetMgrServer::on_btnBrowseMgrPrivateKeyFile_clicked()
{
  QFileDialog fileDialog(this);
  fileDialog.setFileMode(QFileDialog::ExistingFile);
  fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
  fileDialog.setNameFilters(QStringList() << "KEY files (*.key)" << "PEM format (*.pem)" << "Any files (*)");

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
  if (!ui->edtMgrPrivateKeyFile->text().isEmpty())
  {
    QFileInfo fi(ui->edtMgrPrivateKeyFile->text());
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

  ui_save();

  QString filename = fileDialog.selectedFiles().at(0);
  QFile f(filename);
  if (!f.open(QIODevice::ReadOnly))
  {
    ui->edtMgrPrivateKeyFile->clear();
    QMessageBox::critical(this, tr("Unable to open file"), tr("Unable to open file %1").arg(filename));
    return;
  }
  QByteArray buffer = f.read(1024*1024);
  f.close();
  if (buffer.isEmpty() || buffer.length() >= 1024*1024)
  {
    ui->edtMgrPrivateKeyFile->clear();
    QMessageBox::critical(this, tr("Invalid file"), tr("Invalid private key file %1: empty or too long").arg(filename));
    return;
  }
  // QSslKey cannot load encrypted key if no passphrase provided
  if (buffer.contains("ENCRYPTED"))
  {
    QMessageBox::critical(this, tr("Invalid private key"), tr("Private key file %1 must NOT be encrypted with passphrase").arg(filename));
    return;
  }
  QSslKey key = QSslKey(buffer, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
  if (key.isNull())
  {
    QMessageBox::critical(this, tr("Invalid private key"), tr("File %1 does not contain a valid private key").arg(filename));
    return;
  }

  ui->edtMgrPrivateKeyFile->setText(filename);
  emit paramsModified();
}

//---------------------------------------------------------------------------
void WidgetMgrServer::on_btnGenerateKeyCert_clicked()
{
  CertificateGeneratorWizard *wizard = new CertificateGeneratorWizard(SysUtil::machine_name, this);
  if (wizard->exec() == QDialog::Accepted)
  {
    ui->edtMgrSSLCertFile->setText(wizard->getCertificateFilename());
    ui->edtMgrPrivateKeyFile->setText(wizard->getPrivateKeyFilename());

    emit paramsModified();
  }
  wizard->deleteLater();
}

