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
#ifndef LIB_SYSTEM_H_
#define LIB_SYSTEM_H_

#include <iostream>
#include "Noncopyable.h"

using namespace MSF;

namespace MSF {

class SystemInspector : Noncopyable {
 public:
  // void registerCommands(Inspector* ins);

  // static std::string overview(HttpRequest::Method, const
  // Inspector::ArgList&); static std::string loadavg(HttpRequest::Method, const
  // Inspector::ArgList&); static std::string version(HttpRequest::Method, const
  // Inspector::ArgList&); static std::string cpuinfo(HttpRequest::Method, const
  // Inspector::ArgList&); static std::string meminfo(HttpRequest::Method, const
  // Inspector::ArgList&); static std::string stat(HttpRequest::Method, const
  // Inspector::ArgList&);
};

}  // namespace MSF
#endif