/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "gui_settings.h"
#include "gui_settings_dialog.h"
#include "aboutdialog.h"
#include "../lib/sys_util.h"
#include <QMessageBox>
#include <QFileInfo>
#include <QToolTip>
#include <QTimer>
#include "mgr_server.h"

QSystemTrayIcon *trayIcon = NULL;
MainWindow *mainWindow = NULL;
MgrServerParameters old_server_params;

//---------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent): QMainWindow(parent), ui(new Ui::MainWindow)
{
  ui->setupUi(this);
  setWindowTitle(tr("qmtunnel Server v%1.%2").arg(SERVER_MAJOR_VERSION).arg(SERVER_MINOR_VERSION));

  widget_mgrServer = new WidgetMgrServer(false);
  ui->tabWidget->addTab(widget_mgrServer, "Manager Connection");
  connect(widget_mgrServer, SIGNAL(paramsModified()), this, SLOT(mgrServerConfig_modified()));

  widget_userGroups = new WidgetUserGroups(false);
  ui->tabWidget->addTab(widget_userGroups, "Users && Groups");
  connect(widget_userGroups, SIGNAL(paramsModified()), this, SLOT(mgrServerConfig_modified()));

  mgrconfig_loadParams();

  ui->tabWidget->setCurrentIndex(0);

  connect(mgrServer, SIGNAL(listenError(QAbstractSocket::SocketError,QString)),
          this, SLOT(mgrServerSocketError(QAbstractSocket::SocketError,QString)), Qt::QueuedConnection);
  connect(mgrServer, SIGNAL(config_changed(cJSON*)),
          this, SLOT(mgrServerConfig_load(cJSON*)), Qt::QueuedConnection);

  connect(mgrServer, SIGNAL(server_started()),
          this, SLOT(server_started()), Qt::QueuedConnection);
  connect(mgrServer, SIGNAL(server_stopped()),
          this, SLOT(server_stopped()), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_start_server()),
          mgrServer, SLOT(start()), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_stop_server()),
          mgrServer, SLOT(stop()), Qt::QueuedConnection);

  connect(this, SIGNAL(mgrServerConfig_changed(cJSON*)),
          mgrServer, SLOT(gui_config_set(cJSON*)), Qt::QueuedConnection);

  // setup tray icon
  trayIcon = new QSystemTrayIcon(this);
  trayIcon->setIcon(this->windowIcon());
  trayIcon->setToolTip(windowTitle());
  if (gui_flags & GFL_TRAY_ALWAYS_VISIBLE)
    trayIcon->setVisible(true);
  QMenu *trayIconMenu = new QMenu(this);
  trayIcon->setContextMenu(trayIconMenu);
  trayIconMenu->addAction(tr("Show/Hide"), this, SLOT(trayIconToggleState()));
  trayIconMenu->addSeparator();
  trayIconMenu->addAction(tr("Exit"), this, SLOT(on_action_exit_triggered()));

  connect(this, SIGNAL(gui_closing()),
          mgrServer, SLOT(quit()), Qt::QueuedConnection);
  load_ui_settings();

}

//---------------------------------------------------------------------------
void MainWindow::firstRun()
{
  QMessageBox::information(this, tr("First start"),
                           tr("Configuration file %1 not found.\n"
                              "If this is the first start and you don't have any private key and SSL certificate ready for qmtunnel server, "
                              "begin with \"%2\" button.\n"
                              "Then press \"%3\" button to create and save configuration file.")
                           .arg(QFileInfo(qmtunnel_config_filepath).absoluteFilePath())
                           .arg(widget_mgrServer->ui->btnGenerateKeyCert->text().replace("&&", "&"))
                           .arg(ui->btnMgrApplyAndSave->text().replace("&&", "&")),
                           QMessageBox::Ok);
}

//---------------------------------------------------------------------------
void MainWindow::on_action_About_triggered()
{
  AboutDialog dialog(this);
  dialog.exec();
}

//---------------------------------------------------------------------------
void MainWindow::noOpenSSL()
{
  QString openssl_min_version;
#if QT_VERSION >= 0x050400
    openssl_min_version = tr(" (at least v.%1)").arg(QSslSocket::sslLibraryBuildVersionString());
#endif
  QMessageBox::critical(this, tr("OpenSSL error"),
                           tr("OpenSSL library is not available.\n"
                              "This makes qmtunnel unsecure in public networks.\n"
#ifdef __WIN32__
                              "Make sure that you have proper%1 libeay32.dll and ssleay32.dll in qmtunnel directory."
#elif defined(__LINUX__)
                              "Make sure that you have proper%1 openssl/crypto package installed."
#else
                              "Make sure that you have proper%1 OpenSSL library files installed."
#endif
                              ).arg(openssl_min_version),
                           QMessageBox::Ok);
}

//---------------------------------------------------------------------------
void MainWindow::mgrconfig_loadParams()
{
  widget_mgrServer->loadParams(&gui_mgrServerParams);
  widget_userGroups->loadParams(&gui_mgrServerParams);

  mgr_server_params_gui_modified = false;
  mgr_server_params_modified_on_server_while_editing = false;
  update_mgrConfigButtons();
}

//---------------------------------------------------------------------------
void MainWindow::mgrconfig_saveParams()
{
  widget_mgrServer->saveParams(&gui_mgrServerParams);
  widget_userGroups->saveParams(&gui_mgrServerParams);
}

//---------------------------------------------------------------------------
void MainWindow::load_ui_settings()
{
  cJSON *j_mainWindow = cJSON_GetObjectItem(j_gui_settings, "mainWindow");
  if (!j_mainWindow)
    return;
  cJSON *j;
  j = cJSON_GetObjectItem(j_mainWindow, "geometry");
  if (j && j->type == cJSON_String)
    this->restoreGeometry(QByteArray::fromBase64(j->valuestring));
  j = cJSON_GetObjectItem(j_mainWindow, "state");
  if (j && j->type == cJSON_String)
    this->restoreState(QByteArray::fromBase64(j->valuestring));
}

//---------------------------------------------------------------------------
void MainWindow::server_started()
{
  ui->btnStartStop->setText(tr("Stop server"));
}

//---------------------------------------------------------------------------
void MainWindow::server_stopped()
{
  ui->btnStartStop->setText(tr("Start server"));
}

//---------------------------------------------------------------------------
void MainWindow::on_btnStartStop_clicked()
{
  if (ui->btnStartStop->text() == tr("Start server"))
    emit gui_start_server();
  else if (ui->btnStartStop->text() == tr("Stop server"))
    emit gui_stop_server();
}

//---------------------------------------------------------------------------
void MainWindow::on_action_UISettings_triggered()
{
  openUISettingsDialog();
}

//---------------------------------------------------------------------------
void MainWindow::save_ui_settings()
{
  cJSON *j_mainWindow = cJSON_GetObjectItem(j_gui_settings, "mainWindow");
  if (j_mainWindow)
    cJSON_DeleteItemFromObject(j_gui_settings, "mainWindow");
  j_mainWindow = cJSON_CreateObject();
  cJSON_AddItemToObject(j_gui_settings, "mainWindow", j_mainWindow);

  cJSON_AddStringToObject(j_mainWindow, "geometry", this->saveGeometry().toBase64());
  cJSON_AddStringToObject(j_mainWindow, "state", this->saveState().toBase64());
}

//---------------------------------------------------------------------------
void MainWindow::update_mgrConfigButtons()
{
  bool can_apply = widget_mgrServer->canApply() && widget_userGroups->canApply();
  ui->btnMgrApplyAndSave->setEnabled(mgr_server_params_gui_modified && can_apply);
  ui->btnMgrCancel->setEnabled(mgr_server_params_gui_modified);
}

//---------------------------------------------------------------------------
void MainWindow::mgrServerConfig_load(cJSON *j_config)
{
  MgrServerParameters new_params;
  new_params.parseJSON(j_config);

  if (!mgr_server_params_gui_modified)
  {
    gui_mgrServerParams = new_params;
    mgrconfig_loadParams();
  }
  else if (new_params != gui_mgrServerParams)
  {
    mgr_server_params_modified_on_server_while_editing = true;
    gui_mgrServerParams = new_params;
  }
  else if (new_params.config_version == gui_mgrServerParams.config_version+1)
  {
    gui_mgrServerParams.config_version = new_params.config_version;
    widget_mgrServer->setEnabled(true);
    widget_userGroups->setEnabled(true);
    mgr_server_params_gui_modified = false;
    update_mgrConfigButtons();
  }

  cJSON_Delete(j_config);
}

//---------------------------------------------------------------------------
void MainWindow::mgrServerConfig_modified()
{
  mgr_server_params_gui_modified = true;
  update_mgrConfigButtons();
}

//---------------------------------------------------------------------------
void MainWindow::mgrServerSocketError(QAbstractSocket::SocketError, const QString &errorString)
{
  QMessageBox::critical(this, tr("Error"),
                        tr("Failed to start server on %1:%2!\n").arg(gui_mgrServerParams.listen_interface.toString()).arg(gui_mgrServerParams.listen_port)+errorString, QMessageBox::Ok);
}

//---------------------------------------------------------------------------
void MainWindow::on_btnMgrApplyAndSave_clicked()
{
  if (receivers(SIGNAL(mgrServerConfig_changed(cJSON *))) == 0)
    return;

  if (mgr_server_params_modified_on_server_while_editing)
  {
    if (QMessageBox::warning(this, tr("Configuration modified"), tr("ATTENTION!\n"
                                                                    "Server configuration has been modified by someone else since you started making your changes.\n"
                                                                    "Are you sure you want to override those modifications with your changes?\n"
                                                                    "If not, press No and then Cancel & Restore to reload current server configuration."),
                                QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
      return;
  }

  old_server_params = gui_mgrServerParams;
  mgrconfig_saveParams();

  quint16 res_code = MgrServerParameters::RES_CODE_OK;
  QString config_error;
  if (!config_check(&old_server_params, &gui_mgrServerParams, res_code, config_error))
  {
    gui_mgrServerParams = old_server_params;

    if (res_code == MgrServerParameters::RES_CODE_LISTEN_ERROR ||
        res_code == MgrServerParameters::RES_CODE_SSL_CERT_ERROR ||
        res_code == MgrServerParameters::RES_CODE_PRIVATE_KEY_ERROR)
      widget_mgrServer->applyError(res_code);

    QMessageBox::critical(this, tr("Configuration error"), config_error, QMessageBox::Ok);
    return;
  }

  cJSON *j_config = cJSON_CreateObject();
  gui_mgrServerParams.printJSON(j_config);
  emit mgrServerConfig_changed(j_config);

  widget_mgrServer->setEnabled(false);
  widget_userGroups->setEnabled(false);
  ui->btnMgrApplyAndSave->setEnabled(false);
  ui->btnMgrCancel->setEnabled(false);
}

//---------------------------------------------------------------------------
void MainWindow::on_btnMgrCancel_clicked()
{
  mgrconfig_loadParams();
}

//---------------------------------------------------------------------------
void MainWindow::on_action_exit_triggered()
{
  flag_kill = true;
  this->close();
}

//---------------------------------------------------------------------------
void MainWindow::closeEvent(QCloseEvent *event)
{
  if (!flag_kill && (gui_flags & GFL_MINIMIZE_TO_TRAY_ON_CLOSE) && trayIcon)
  {
    trayIcon->show();
    QTimer::singleShot(250, this, SLOT(hide()));
    event->ignore();
    return;
  }
  save_ui_settings();

  if (mgr_server_params_gui_modified)
  {
    if (QMessageBox::warning(this, tr("Forgot to save?"), tr("Current configuration has been modified.\n"
                                                             "Are you sure you want to discard changes and exit?"),
                         QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
    {
      event->ignore();
      return;
    }
  }

  emit gui_closing();
  QMainWindow::closeEvent(event);
}

//---------------------------------------------------------------------------
void MainWindow::changeEvent(QEvent *event)
{
  if (event->type() == QEvent::WindowStateChange)
  {
    if (this->windowState() & Qt::WindowMinimized)
    {
      if ((gui_flags & GFL_MINIMIZE_TO_TRAY) && trayIcon)
      {
        trayIcon->show();
        QTimer::singleShot(250, this, SLOT(hide()));
        event->ignore();
        return;
      }
    }
  }
  QMainWindow::changeEvent(event);
}

//---------------------------------------------------------------------------
void MainWindow::trayIconToggleState()
{
  if (!this->isVisible())
    this->showNormal();
  else
    this->hide();
}

//---------------------------------------------------------------------------
void MainWindow::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
  this->showNormal();
  trayIcon->hide();
  switch (reason)
  {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
      break;
    default:
      break;
  }
}

//---------------------------------------------------------------------------
MainWindow::~MainWindow()
{
  delete ui;
}
