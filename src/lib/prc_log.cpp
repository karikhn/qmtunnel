/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#include "prc_log.h"
#include "sys_util.h"
#include <QMutexLocker>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QThread>

LogPriority prc_log_level = LOG_NORMAL;
qint64 prc_log_max_size = 1024*1024;
int prc_log_keep_files = 4;
QString prc_log_filename("./qmtunnel.log");

QMutex mutex_prc_log(QMutex::Recursive);
QFile f_log;
QList<unsigned long long> prc_log_threads;
const char *log_first_letters = "FCHNL12345";
int PRC_LOG_DUPLICATES_TIMEOUT = 30;
bool prc_log_reopen_flag = false;

//---------------------------------------------------------------------------
bool prc_log_try_open()
{
  if (!f_log.open(QIODevice::Append))
  {
    fprintf(stderr, "Unable to open log file %s\r\n", (const char *)prc_log_filename.toUtf8());
    fflush(stderr);
    return false;
  }
  return true;
}

//---------------------------------------------------------------------------
void prc_log_init()
{
  if (prc_log_level == LOG_NONE)
    return;
  mkpath(QFileInfo(prc_log_filename).path());
  f_log.setFileName(prc_log_filename);
  prc_log_try_open();
}

//---------------------------------------------------------------------------
bool prc_log_truncate()
{
  f_log.close();
  // rename old files
  if (prc_log_keep_files > 0)
  {
    // remove the oldest file
    if (QFile::exists(prc_log_filename+QString(".%1").arg(prc_log_keep_files)) &&
       !QFile::remove(prc_log_filename+QString(".%1").arg(prc_log_keep_files)))
      return false;
    for (int i=prc_log_keep_files-1; i > 0; i--)
    {
      if (QFile::exists(prc_log_filename+QString(".%1").arg(i)) &&
         !QFile::rename(prc_log_filename+QString(".%1").arg(i), prc_log_filename+QString(".%1").arg(i+1)))
        return false;
    }
    if (!QFile::rename(prc_log_filename, prc_log_filename+QString(".%1").arg(1)))
      return false;
    return prc_log_try_open();
  }
  return false;
}

//---------------------------------------------------------------------------
void prc_log_finish()
{
  if (f_log.isOpen())
    f_log.close();
}

//---------------------------------------------------------------------------
void prc_log(const LogPriority prio, const QString &log_text)
{
  static QString prev_log_text;
  static LogPriority prev_log_prio;
  static int prev_log_text_count=0;
  static QDateTime prev_log_ts;

  if (prio > prc_log_level || prc_log_level == LOG_NONE)
    return;
  if (prc_log_reopen_flag)
  {
    prc_log_reopen_flag = false;
    if (f_log.isOpen())
      f_log.close();
  }
  if (!f_log.isOpen())
  {
    if (!prc_log_try_open())
      return;
  }

  if (f_log.size() > prc_log_max_size && !prc_log_truncate())
    return;

  QDateTime now = QDateTime::currentDateTime();
  QString new_log_text = log_text;
  QMutexLocker mutex_locker(&mutex_prc_log);
  QString thread_str;
  /*
  // this block was to log thread number
  int thread_index = prc_log_threads.indexOf((unsigned long long)QThread::currentThreadId());
  if (thread_index < 0)
  {
    prc_log_threads.append((unsigned long long)QThread::currentThreadId());
    thread_index = prc_log_threads.indexOf((unsigned long long)QThread::currentThreadId());
    if (prc_log_threads.count() > 10)
      prc_log_threads.removeAt(5);
  }
  thread_str = QString("(T%1) ").arg(thread_index+1);
  */
  // check for frequent duplicate records
  if (prev_log_text == log_text && prev_log_prio == prio && prev_log_ts.secsTo(now) < PRC_LOG_DUPLICATES_TIMEOUT)
  {
    prev_log_text_count++;
    return;
  }
  else if (prev_log_text == log_text && prev_log_prio == prio)
  {
    prev_log_text_count++;
    new_log_text = QString("last event occured %1 times").arg(prev_log_text_count);
  }
  else if (prev_log_text_count > 0)
  {
    f_log.write((now.toString("yyyy-MM-dd hh:mm:ss.zzz")+QString(" [%1] ").arg(QChar(log_first_letters[(int)prev_log_prio % LOG_PRIO_COUNT]))+thread_str+QString("last event occured %1 times").arg(prev_log_text_count)+QString("\r\n")).toUtf8());
  }
  prev_log_text_count = 0;
  if (f_log.write((now.toString("yyyy-MM-dd hh:mm:ss.zzz ")+QString("[%1] ").arg(QChar(log_first_letters[(int)prio % LOG_PRIO_COUNT]))+thread_str+new_log_text+QString("\r\n")).toUtf8()) <= 0)
  {
    f_log.close();
    if (!prc_log_try_open())
      return;
    f_log.write((now.toString("yyyy-MM-dd hh:mm:ss.zzz ")+QString("[%1] ").arg(QChar(log_first_letters[(int)prio % LOG_PRIO_COUNT]))+thread_str+new_log_text+QString("\r\n")).toUtf8());
  }
  if (prc_log_level < LOG_NORMAL)
    fprintf(stderr, "%s: %s\r\n", (const char *)SysUtil::process_name.toUtf8(), (const char *)new_log_text.toUtf8());
  if (!f_log.flush())
  {
    f_log.close();
    if (!prc_log_try_open())
      return;
    f_log.write((now.toString("yyyy-MM-dd hh:mm:ss.zzz ")+QString("[%1] ").arg(QChar(log_first_letters[(int)prio % LOG_PRIO_COUNT]))+thread_str+new_log_text+QString("\r\n")).toUtf8());
  }
  prev_log_text = log_text;
  prev_log_prio = prio;
  prev_log_ts = now;
}
