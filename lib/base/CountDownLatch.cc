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
#include "CountDownLatch.h"

#include <butil/logging.h>

using namespace MSF;

namespace MSF {

CountDownLatch::CountDownLatch(int count) : _count(count) {}

void CountDownLatch::wait() {
  std::unique_lock<std::mutex> lock(_mutex);

  /**
   * 当前线程被阻塞 Unlock mu and wait to be notified
   * https://www.jianshu.com/p/c1dfa1d40f53
   * https://zh.cppreference.com/w/cpp/thread/condition_variable
   * //https://blog.csdn.net/business122/article/details/80881925
   * */
  _condition.wait(lock, [this]() { return (_count == 0); });

  lock.unlock();
}

// http://www.voidcn.com/article/p-wonhtnlp-bsz.html
// https://blog.csdn.net/fengbingchun/article/details/73695596
// https://www.cnblogs.com/haippy/p/3252041.html
// https://www.2cto.com/kf/201506/411327.html
// wait_for: std::cv_status::timeout
bool CountDownLatch::waitFor(const uint32_t ts) {
  std::unique_lock<std::mutex> lock(_mutex);
  if (_condition.wait_for(lock, std::chrono::seconds(ts)) == std::cv_status::timeout /*,
     [this]() { 
       LOG(INFO) << "count wait: " << _count;
       return (_count == 0);
       }) == false*/) {
    // LOG(INFO) << "count timeout: " << _count;
    return false;
  }
  return true;
}

void CountDownLatch::countDown() {
  // LOG(INFO) << "count is: " << _count;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    --_count;
  }

  if (_count == 0) {
    _condition.notify_all();
  }
}

int CountDownLatch::getCount() const {
  // std::lock_guard<std::mutex> lock(_mutex);
  return _count;
}

}  // namespace MSF