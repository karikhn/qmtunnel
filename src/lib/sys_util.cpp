/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "sys_util.h"
#include "prc_log.h"
#include <QHostInfo>
#include <QFileInfo>
#include <QLocale>
#include <QDir>
#include <QtCore/qmath.h>
#include <time.h>
#include <locale.h>
#ifdef Q_OS_LINUX
#include <signal.h>
//#include <utime.h>
//#include <sys/wait.h>
#endif
#include <QCoreApplication>

namespace SysUtil
{
pid_t current_pid;
QString process_name;
QString machine_name;
QString user_name;
int last_signum=0;
}

bool flag_kill = false;

using namespace SysUtil;

#ifdef Q_OS_LINUX
//-----------------------------------------------------------------------------
void linux_signal_handler(int signum)
{
  last_signum = signum;
  if (signum == SIGTERM || signum == SIGQUIT || signum == SIGINT)
  {
    flag_kill = true;
    QCoreApplication::quit();
  }
  else if (signum == SIGHUP)
    prc_log_reopen_flag = true;
}

//-----------------------------------------------------------------------------
void log_last_signum()
{
  if (last_signum == 0)
    return;
  int signum = last_signum;
  last_signum = 0;
  switch(signum)
  {
    case SIGTERM:
      prc_log(LOG_HIGH, QString("Got signal SIGTERM"));
      break;
    case SIGHUP:
      prc_log(LOG_HIGH, QString("Got signal SIGHUP"));
      break;
    case SIGSTOP:
      prc_log(LOG_HIGH, QString("Got signal SIGSTOP"));
      break;
    case SIGKILL:
      prc_log(LOG_HIGH, QString("Got signal SIGKILL"));
      break;
    case SIGINT:
      prc_log(LOG_HIGH, QString("Got signal SIGINT"));
      break;
    case SIGQUIT:
      prc_log(LOG_HIGH, QString("Got signal SIGQUIT"));
      break;
    case SIGCHLD:
      prc_log(LOG_DBG2, QString("Got signal SIGCHLD"));
      break;
    case SIGPIPE:
      prc_log(LOG_HIGH, QString("Got signal SIGPIPE"));
      break;
    default:
      prc_log(LOG_HIGH, QString("Got signal %1").arg(signum));
      break;
  }
}
#endif
//---------------------------------------------------------------------------
void sysutil_init(int , char *[])
{
  flag_kill = false;

  setlocale(LC_NUMERIC, "C");

  qsrand(time(NULL)+QCoreApplication::applicationPid());

#ifdef Q_OS_LINUX
  signal(SIGTERM, linux_signal_handler);
  signal(SIGHUP,  linux_signal_handler);
  signal(SIGINT,  linux_signal_handler);
  signal(SIGQUIT, linux_signal_handler);
#endif

  SysUtil::process_name = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
  SysUtil::machine_name = QHostInfo::localHostName();
  SysUtil::user_name = qgetenv("USER");
  if (SysUtil::user_name.isEmpty())
    SysUtil::user_name = qgetenv("USERNAME");
}

//---------------------------------------------------------------------------
QString pretty_size(quint64 val, int max_precision)
{
  double tiny_val;
  if (val > 1099511627776LL)
  {
    tiny_val = (double)((double)((val/1024)/1024)/1024)/1024;
    return QString("%1 Tb").arg(tiny_val, 0, 'f', tiny_val < 100 ? max_precision : 0);
  }
  else if (val > 1073741824)
  {
    tiny_val = (double)((double)(val/1024)/1024)/1024;
    return QString("%1 Gb").arg(tiny_val, 0, 'f', tiny_val < 100 ? max_precision : 0);
  }
  else if (val > 1048576)
  {
    tiny_val = ((double)val/1024)/1024;
    return QString("%1 Mb").arg(tiny_val, 0, 'f', tiny_val < 100 ? max_precision : 0);
  }
  else if (val > 1024)
  {
    tiny_val = (double)val/1024;
    return QString("%1 Kb").arg(tiny_val, 0, 'f', tiny_val < 100 ? max_precision : 0);
  }
  else
    return QString("%1 b").arg(val);
}

//---------------------------------------------------------------------------
QString pretty_date_time(const QDateTime &dt, const QDateTime &rel_dt, bool include_ms)
{
  QString fmt;
  if (include_ms)
    fmt = QString("yyyy-MM-dd hh:mm:ss.zzz");
  else
    fmt = QString("yyyy-MM-dd hh:mm:ss");

  if (!rel_dt.isValid())
    return dt.toString(fmt);

  qint64 msecs = dt.msecsTo(rel_dt);
  if (msecs > 1000*60*60*24)
    return dt.toString(fmt);

  int h = qFloor(((qreal)msecs/1000)/60/60);
  int m = qFloor(((qreal)(msecs-(h*1000*60*60))/1000)/60);
  int s = qFloor((qreal)(msecs-(h*1000*60*60)-(m*1000*60))/1000);
  int ms = msecs-(h*1000*60*60)-(m*1000*60)-(s*1000);

  QString str;
  if (h > 0)
    str += QString(" %1h").arg(h);
  if (m > 0 || s > 0)
    str += QString(" %1m").arg(m);
  if (s > 0)
    str += QString(" %1s").arg(s);
  if (ms > 0 && include_ms)
    str += QString(" %1ms").arg(ms);
  if (str.isEmpty())
    return QString("just now");
  return str.trimmed()+QString(" ago");
}

//---------------------------------------------------------------------------
bool mkpath(const QString &path)
{
  QDir dir(path);
  if (!dir.exists())
  {
    QDir d;
    return d.mkpath(dir.path());
  }
  return true;
}
