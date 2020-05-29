/**************************************************************************
 *
 * Copyright (c) 2017-2021, luotang.me <wypx520@gmail.com>, China.
 * All rights reserved.
 *
 * Distributed under the terms of the GNU General Public License v2.
 *
 * This software is provided 'as is' with no explicit or implied warranties
 * in respect of its properties, including, but not limited to, correctness
 * and/or fitness for purpose.
 *
 **************************************************************************/
#ifndef __MSF_DAEMON_H__
#define __MSF_DAEMON_H__

#include <base/Define.h>

using namespace MSF::BASE;

namespace MSF {
namespace BASE {

void daemonize(bool chdir = true, bool close = true);

}  // namespace BASE
}  // namespace MSF
#endif
