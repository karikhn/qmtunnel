/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef PRC_LOG_H
#define PRC_LOG_H

#include <QString>

enum LogPriority { LOG_NONE=0,
                   LOG_FATAL=1,
                   LOG_CRITICAL=2,
                   LOG_HIGH=3,
                   LOG_NORMAL=4,
                   LOG_LOW=5,
                   LOG_DBG1=6,
                   LOG_DBG2=7,
                   LOG_DBG3=8,
                   LOG_DBG4=9,
                   LOG_PRIO_COUNT=10
                 };

extern LogPriority prc_log_level;
extern qint64 prc_log_max_size;
extern int prc_log_keep_files;
extern bool prc_log_reopen_flag;
extern QString prc_log_filename;

void prc_log(const LogPriority prio, const QString &log_text);
void prc_log_init();
void prc_log_finish();

#endif // PRC_LOG_H
