/*
 Copyright (C) 2017 Nikolay N. Karikh <knn@qmtunnel.com>

 This file is part of qmtunnel and is licensed under GNU General Public License 3.0, with
 the additional special exception to link portions of this program with the OpenSSL library.
 See LICENSE file for more details.
*/

#ifndef MGRCLIENTSTATE_H
#define MGRCLIENTSTATE_H

#include <QtGlobal>
#include "../lib/mgrserver-parameters.h"

class MgrClientState
{
public:
  MgrClientState()
  {
    flags = 0;
    user_id = 0;
  }

  quint32 user_id;

  quint32 flags;                // Subscription flags, such as:
  enum
  {
    FL_CONFIG                = 0x00000001      // mgrserver configuration
   ,FL_TUNNELS_CONFIG        = 0x00000002      // tunnels configuration
   ,FL_TUNNELS_STATE         = 0x00000004      // tunnels state
  };
};

#endif // MGRCLIENTSTATE_H
