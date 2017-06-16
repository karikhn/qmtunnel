/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "mainwindow.h"
#include "../lib/sys_util.h"
#include "../lib/prc_log.h"
#include "gui_settings.h"
#include <QApplication>
#include "thread-connections.h"

// openssl x509 -inform DER -outform PEM -in server.crt -out server.crt.pem
// openssl rsa -inform DER -outform PEM -in server.key -out server.key.pem
// openssl rsa -in ssl.key.secure -out ssl.key

extern QString gui_settings_filepath;

//---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  prc_log_filename = QString("qmtunnel-gui.log");
  prc_log_level = LOG_NONE;
  bool show_opt_hint = false;
  for (int i=1; i < argc; i++)
  {
    QString opt = QString::fromUtf8(argv[i]);
    if (opt == QString("-debug"))
    {
      bool ok=false;
      if (i+1 < argc && QByteArray(argv[i+1]).toUInt(&ok) > 0 && ok)
      {
        prc_log_level = (LogPriority)QByteArray(argv[i+1]).toUInt();
        i++;
      }
      else
        prc_log_level = LOG_DBG4;
    }
    else if (opt == QString("-logfile") && i+1 < argc)
    {
      prc_log_filename = QString::fromUtf8(argv[i+1]);
      i++;
    }
    else
    {
      fprintf(stderr, "Unknown argument \"%s\"\r\n", argv[i]);
      fflush(stderr);
      show_opt_hint = true;
    }
  }
  if (show_opt_hint)
  {
    fprintf(stdout, "qmtunnel GUI v%d.%d\r\n", GUI_MAJOR_VERSION, GUI_MINOR_VERSION);
    fprintf(stdout, "Command line usage: qmtunnel-gui [-logfile LOGFILENAME] [-debug DEBUGLEVEL]\r\n");
    fprintf(stdout, "Where:\r\n");
    fprintf(stdout, "  DEBUGLEVEL varies from 0 (no log output) to 9 (maximum verbosity)\r\n");
    fprintf(stdout, "\r\n");
    fflush(stdout);
  }
  QApplication a(argc, argv);
  a.setWindowIcon(QIcon(":/images/icons/icon64.png"));
  sysutil_init(argc, argv);
  prc_log_init();
  prc_log(LOG_LOW, QString("qmtunnel GUI v%2.%3 started").arg(GUI_MAJOR_VERSION).arg(GUI_MINOR_VERSION));
  ui_load();
  commThread_init();

  mainWindow = new MainWindow;
  if (gui_flags & GFL_MINIMIZE_TO_TRAY_ON_START)
    trayIcon->setVisible(true);
  else
  {
    mainWindow->show();
    if (!QSslSocket::supportsSsl())
      QTimer::singleShot(250, mainWindow, SLOT(noOpenSSL()));
    else if (!QFile::exists(gui_settings_filepath))
      QTimer::singleShot(500, mainWindow, SLOT(firstRun()));
  }
  int res = a.exec();
  commThread_free();
  delete mainWindow;

#ifdef Q_OS_LINUX
  log_last_signum();
#endif
  ui_save();
  ui_free();
  prc_log(LOG_LOW, QString("qmtunnel GUI stopped"));
  prc_log_finish();
  return res;
}
