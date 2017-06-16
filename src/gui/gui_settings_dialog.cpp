/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "gui_settings_dialog.h"
#include "ui_gui_settings_dialog.h"
#include "gui_settings.h"
#include "mainwindow.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include "../lib/ssl_helper.h"
#include "../lib/ssl_keygen.h"
#include "../lib/sys_util.h"

//---------------------------------------------------------------------------
UISettingsDialog::UISettingsDialog(QWidget *parent): QDialog(parent), ui(new Ui::UISettingsDialog)
{
  ui->setupUi(this);

  if (!QSslSocket::supportsSsl())
  {
    ui->btnGenerateKeyCert->setEnabled(false);
    ui->btnBrowsePrivateKeyFile->setEnabled(false);
    ui->btnBrowseSSLCertFile->setEnabled(false);
    ui->edtPrivateKeyFile->setEnabled(false);
    ui->edtSSLCertFile->setEnabled(false);
  }

  ui->tabWidget->setCurrentIndex(0);
}

//---------------------------------------------------------------------------
void UISettingsDialog::loadSettings()
{
  ui->chkAutoOpenRecentOnStart->setChecked(gui_flags & GFL_AUTOOPEN_RECENT_PROFILE_ON_START);
  ui->chkMinimizeToTray->setChecked(gui_flags & GFL_MINIMIZE_TO_TRAY);
  ui->chkMinimizeToTrayOnStart->setChecked(gui_flags & GFL_MINIMIZE_TO_TRAY_ON_START);
  ui->chkMinimizeToTrayOnClose->setChecked(gui_flags & GFL_MINIMIZE_TO_TRAY_ON_CLOSE);
  ui->chkAlwaysShowTray->setChecked(gui_flags & GFL_TRAY_ALWAYS_VISIBLE);

  cJSON *j = cJSON_GetObjectItem(j_gui_settings, "defaultSSL_pkeyFilename");
  if (j && j->type == cJSON_String)
    ui->edtPrivateKeyFile->setText(QString::fromUtf8(j->valuestring));

  j = cJSON_GetObjectItem(j_gui_settings, "defaultSSL_certFilename");
  if (j && j->type == cJSON_String)
    ui->edtSSLCertFile->setText(QString::fromUtf8(j->valuestring));
}

//---------------------------------------------------------------------------
void set_gui_flags(bool is_checked, unsigned int flag_mask)
{
  if (is_checked)
    gui_flags |= flag_mask;
  else
    gui_flags &= ~flag_mask;
}

//---------------------------------------------------------------------------
void UISettingsDialog::saveSettings()
{
  set_gui_flags(ui->chkAutoOpenRecentOnStart->isChecked(), GFL_AUTOOPEN_RECENT_PROFILE_ON_START);
  set_gui_flags(ui->chkMinimizeToTray->isChecked(), GFL_MINIMIZE_TO_TRAY);
  set_gui_flags(ui->chkMinimizeToTrayOnStart->isChecked(), GFL_MINIMIZE_TO_TRAY_ON_START);
  set_gui_flags(ui->chkMinimizeToTrayOnClose->isChecked(), GFL_MINIMIZE_TO_TRAY_ON_CLOSE);
  set_gui_flags(ui->chkAlwaysShowTray->isChecked(), GFL_TRAY_ALWAYS_VISIBLE);

  cJSON *j = cJSON_GetObjectItem(j_gui_settings, "defaultSSL_pkeyFilename");
  if (j)
    cJSON_DeleteItemFromObject(j_gui_settings, "defaultSSL_pkeyFilename");
  cJSON_AddStringToObject(j_gui_settings, "defaultSSL_pkeyFilename", ui->edtPrivateKeyFile->text().toUtf8());

  j = cJSON_GetObjectItem(j_gui_settings, "defaultSSL_certFilename");
  if (j)
    cJSON_DeleteItemFromObject(j_gui_settings, "defaultSSL_certFilename");
  cJSON_AddStringToObject(j_gui_settings, "defaultSSL_certFilename", ui->edtSSLCertFile->text().toUtf8());

  ui_save();

  if (gui_flags & GFL_TRAY_ALWAYS_VISIBLE)
    trayIcon->setVisible(true);
  else
    trayIcon->setVisible(false);
}

//---------------------------------------------------------------------------
void openUISettingsDialog(int tabIndex)
{
  UISettingsDialog dialog;
  dialog.loadSettings();
  if (tabIndex > 0)
    dialog.ui->tabWidget->setCurrentIndex(tabIndex);
  if (dialog.exec() == QDialog::Accepted)
  {
    dialog.saveSettings();
  }
}


//---------------------------------------------------------------------------
void UISettingsDialog::on_btnBrowseSSLCertFile_clicked()
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
  if (!ui->edtSSLCertFile->text().isEmpty())
  {
    QFileInfo fi(ui->edtSSLCertFile->text());
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
    ui->edtSSLCertFile->clear();
    QMessageBox::critical(this, tr("Invalid SSL certificate"), tr("File %1 does not contain any valid SSL certificate").arg(filename));
    return;
  }

  ui->edtSSLCertFile->setText(filename);
}

//---------------------------------------------------------------------------
void UISettingsDialog::on_btnBrowsePrivateKeyFile_clicked()
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

  ui_save();

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
  if (!buffer.contains("ENCRYPTED"))
  {
    QSslKey key = QSslKey(buffer, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    if (key.isNull())
    {
      QMessageBox::critical(this, tr("Invalid private key"), tr("File %1 does not contain a valid private key").arg(filename));
      return;
    }
  }

  ui->edtPrivateKeyFile->setText(filename);
}

//---------------------------------------------------------------------------
void UISettingsDialog::on_btnGenerateKeyCert_clicked()
{
  CertificateGeneratorWizard *wizard = new CertificateGeneratorWizard(SysUtil::user_name, this);
  if (wizard->exec() == QDialog::Accepted)
  {
    ui->edtSSLCertFile->setText(wizard->getCertificateFilename());
    ui->edtPrivateKeyFile->setText(wizard->getPrivateKeyFilename());
  }
  wizard->deleteLater();
}


//---------------------------------------------------------------------------
UISettingsDialog::~UISettingsDialog()
{
  delete ui;
}
