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
#include "thread-connections.h"

QSystemTrayIcon *trayIcon = NULL;
MainWindow *mainWindow = NULL;
//---------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent): QMainWindow(parent), ui(new Ui::MainWindow)
{
  ui->setupUi(this);

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

  if (gui_no_config)
  {
    gui_flags |= GFL_AUTOOPEN_RECENT_PROFILE_ON_START;
  }

  connect(commThread->obj, SIGNAL(mgrconn_state_changed(quint32,quint16)),
          this, SLOT(connection_state_changed(quint32,quint16)), Qt::QueuedConnection);
  connect(commThread->obj, SIGNAL(mgrconn_socket_error(quint32,QAbstractSocket::SocketError)),
          this, SLOT(connection_error(quint32,QAbstractSocket::SocketError)), Qt::QueuedConnection);
  connect(commThread->obj, SIGNAL(mgrconn_latency_changed(quint32,quint32)),
          this, SLOT(connection_latency_changed(quint32,quint32)), Qt::QueuedConnection);
  connect(commThread->obj, SIGNAL(mgrconn_config_received(quint32,cJSON*)),
          this, SLOT(mgrconn_config_received(quint32,cJSON*)), Qt::QueuedConnection);
  connect(commThread->obj, SIGNAL(mgrconn_config_error(quint32,quint16,QString)),
          this, SLOT(mgrconn_config_error(quint32,quint16,QString)), Qt::QueuedConnection);
  connect(commThread->obj, SIGNAL(mgrconn_password_required(quint32)),
          this, SLOT(connection_password_required(quint32)), Qt::QueuedConnection);
  connect(commThread->obj, SIGNAL(mgrconn_passphrase_required(quint32)),
          this, SLOT(connection_passphrase_required(quint32)), Qt::QueuedConnection);
  connect(commThread->obj, SIGNAL(mgrconn_authorized(quint32,quint64)),
          this, SLOT(connection_authorized(quint32,quint64)), Qt::QueuedConnection);

  connect(commThread->obj, SIGNAL(mgrconn_tunnel_create_reply(quint32,TunnelId,TunnelId,quint16,QString)),
          this, SLOT(mgrconn_tunnel_create_reply(quint32,TunnelId,TunnelId,quint16,QString)), Qt::QueuedConnection);

  connect(this, SIGNAL(gui_profileLoaded(cJSON*)),
          commThread->obj, SLOT(gui_profileLoaded(cJSON*)), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_mgrConnChanged(quint32,MgrClientParameters)),
          commThread->obj, SLOT(gui_mgrConnChanged(quint32,MgrClientParameters)), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_mgrConnDemand(quint32)),
          commThread->obj, SLOT(gui_mgrConnDemand(quint32)), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_mgrConnDemand_disconnect(quint32)),
          commThread->obj, SLOT(gui_mgrConnDemand_disconnect(quint32)), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_mgrConnDemand_idleStart(quint32)),
          commThread->obj, SLOT(gui_mgrConnDemand_idleStart(quint32)), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_mgrConnDemand_idleStop(quint32)),
          commThread->obj, SLOT(gui_mgrConnDemand_idleStop(quint32)), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_mgrserver_config_get(quint32)),
          commThread->obj, SLOT(gui_mgrserver_config_get(quint32)), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_mgrserver_config_set(quint32,cJSON*)),
          commThread->obj, SLOT(gui_mgrserver_config_set(quint32,cJSON*)), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_mgrconn_set_password(quint32,QString)),
          commThread->obj, SLOT(gui_mgrconn_set_password(quint32,QString)), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_mgrconn_set_passphrase(quint32,QString)),
          commThread->obj, SLOT(gui_mgrconn_set_passphrase(quint32,QString)), Qt::QueuedConnection);

  connect(this, SIGNAL(gui_tunnel_create(quint32,cJSON*)),
          commThread->obj, SLOT(gui_tunnel_create(quint32,cJSON*)), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_tunnel_config_get(quint32,TunnelId)),
          commThread->obj, SLOT(gui_tunnel_config_get(quint32,TunnelId)), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_tunnel_config_set(quint32,cJSON*)),
          commThread->obj, SLOT(gui_tunnel_config_set(quint32,cJSON*)), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_tunnel_remove(quint32,TunnelId)),
          commThread->obj, SLOT(gui_tunnel_remove(quint32,TunnelId)), Qt::QueuedConnection);
  connect(commThread->obj, SIGNAL(tunnel_config_received(quint32,cJSON*)),
          this, SLOT(tunnel_config_received(quint32,cJSON*)), Qt::QueuedConnection);
  connect(commThread->obj, SIGNAL(mgrconn_tunnel_config_set_reply(quint32,TunnelId,quint16,QString)),
          this, SLOT(mgrconn_tunnel_config_set_reply(quint32,TunnelId,quint16,QString)), Qt::QueuedConnection);
  connect(commThread->obj, SIGNAL(mgrconn_tunnel_remove(quint32,TunnelId,quint16,QString)),
          this, SLOT(mgrconn_tunnel_remove(quint32,TunnelId,quint16,QString)), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_tunnel_state_get(quint32,TunnelId)),
          commThread->obj, SLOT(gui_tunnel_state_get(quint32,TunnelId)), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_tunnel_connstate_get(quint32,TunnelId,bool)),
          commThread->obj, SLOT(gui_tunnel_connstate_get(quint32,TunnelId,bool)), Qt::QueuedConnection);
  connect(commThread->obj, SIGNAL(tunnel_state_received(quint32,TunnelId,TunnelState)),
          this, SLOT(tunnel_state_received(quint32,TunnelId,TunnelState)), Qt::QueuedConnection);
  connect(commThread->obj, SIGNAL(tunnel_connstate_received(quint32,QByteArray)),
          this, SLOT(tunnel_connstate_received(quint32,QByteArray)), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_tunnel_start(quint32,TunnelId)),
          commThread->obj, SLOT(gui_tunnel_start(quint32,TunnelId)), Qt::QueuedConnection);
  connect(this, SIGNAL(gui_tunnel_stop(quint32,TunnelId)),
          commThread->obj, SLOT(gui_tunnel_stop(quint32,TunnelId)), Qt::QueuedConnection);

  connect(this, SIGNAL(gui_closing()),
          commThread->obj, SLOT(gui_closing()), Qt::QueuedConnection);

  mod_browserTree_init();
  page_mgrConn_init();
  page_tunnel_init();
  mod_profile_init();
  load_ui_settings();
}

//---------------------------------------------------------------------------
void MainWindow::firstRun()
{
  QMessageBox::information(this, tr("First start"),
                           tr("If this is the first start and you don't have any personal administrator private key and SSL certificate ready, "
                              "begin with \"Generate new private key & certificate pair...\" button on Profile menu -> UI settings -> SSL Certificate&Key tab.\n"
                              "The generated private key and SSL certificate will be used by default for all new qmtunnel server connections.\n"
                              "Add corresponding user with that SSL certificate (copy it contents) to all qmtunnel servers you need to manage."),
                           QMessageBox::Ok);

  openUISettingsDialog(1);
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
void MainWindow::load_ui_settings()
{
  cJSON *j_mainWindow = cJSON_GetObjectItem(j_gui_settings, "mainWindow");
  if (!j_mainWindow)
    return;
  cJSON *j;
  j = cJSON_GetObjectItem(j_mainWindow, "splitterState");
  if (j && j->type == cJSON_String)
    ui->splitter->restoreState(QByteArray::fromBase64(j->valuestring));
  j = cJSON_GetObjectItem(j_mainWindow, "geometry");
  if (j && j->type == cJSON_String)
    this->restoreGeometry(QByteArray::fromBase64(j->valuestring));
  j = cJSON_GetObjectItem(j_mainWindow, "state");
  if (j && j->type == cJSON_String)
    this->restoreState(QByteArray::fromBase64(j->valuestring));
  mod_browserTree_ui_load(j_mainWindow);
  page_tunnel_ui_load(j_mainWindow);
  widget_mgrClient->ui_load(j_mainWindow);
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

  cJSON_AddStringToObject(j_mainWindow, "splitterState", ui->splitter->saveState().toBase64());
  cJSON_AddStringToObject(j_mainWindow, "geometry", this->saveGeometry().toBase64());
  cJSON_AddStringToObject(j_mainWindow, "state", this->saveState().toBase64());

  mod_browserTree_ui_save(j_mainWindow);
  page_tunnel_ui_save(j_mainWindow);
  widget_mgrClient->ui_save(j_mainWindow);
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
  if (cur_profile_modified)
  {
    if (QMessageBox::warning(this, tr("Forgot to save?"), tr("Current profile has been modified.\n"
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
