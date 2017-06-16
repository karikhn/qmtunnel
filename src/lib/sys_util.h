/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef SYS_UTIL_H
#define SYS_UTIL_H

#include <QString>
#include <QDateTime>

#define FULL_TIMESTAMP_FORMAT      "yyyy-MM-dd hh:mm:ss.zzz"
#define FULL_DATE_TIME_FORMAT      "yyyy-MM-dd hh:mm:ss"

namespace SysUtil
{
extern pid_t current_pid;
extern QString process_name;
extern QString machine_name;
extern QString user_name;
}

extern bool flag_kill;

void sysutil_init(int argc, char *argv[]);
QString pretty_size(quint64 val, int max_precision=0);
QString pretty_date_time(const QDateTime &dt, const QDateTime &rel_dt, bool include_ms=false);

#ifdef Q_OS_LINUX
void termination_handler(int signum);
void log_last_signum();
#endif

bool mkpath(const QString &path);

#endif // SYS_UTIL_H
