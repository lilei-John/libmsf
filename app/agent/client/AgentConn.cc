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
#include "AgentConn.h"

namespace MSF {
namespace AGENT {

AgentConn::AgentConn(/* args */) :
          txQequeSize_(0),
          txNeedSend_(0),
          pduCount_(0)
{

}

AgentConn::~AgentConn()
{
}


void AgentConn::doRecvBhs()
{
  MSF_INFO << "Recv msg from fd: " << fd_;
  uint32_t count = 0;
  void *head = mpool_->alloc(AGENT_HEAD_LEN);
  assert(head);
  struct iovec iov = { head, AGENT_HEAD_LEN };
  int ret = RecvMsg(fd_, &iov, 1, MSG_NOSIGNAL | MSG_DONTWAIT);
  MSF_INFO << "Recv msg ret: " << ret;
  if (unlikely(ret <= 0)) {
    /* Maybe there is no data anymore */
    if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
      if ((++count) > 4) {
        return;
      }
      // continue;
    }
    MSF_ERROR << "Recv buffer failed for fd: " << fd_;
    doConnClose();
    return;
  }
  assert(ret == AGENT_HEAD_LEN);
  rxQequeIov_.push_back(std::move(iov));

  Agent::AgentBhs bhs;
  bhs.ParseFromArray(head, AGENT_HEAD_LEN);
  proto_->debugBhs(bhs);
  
  uint32_t len = proto_->pduLen(bhs);
  if (len) {
    void *body = mpool_->alloc(len);
    assert(body);
    rxState_ = kRecvPdu;
    iov.iov_base = body;
    iov.iov_len = len;
    ret = RecvMsg(fd_, &iov, 1, MSG_NOSIGNAL | MSG_DONTWAIT);
    if (unlikely(ret <= 0)) {
      /* Maybe there is no data anymore */
      if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
        if ((++count) > 4) {
          return;
        }
        // continue;
      }
      MSF_ERROR << "Recv buffer failed for fd: " << fd_;
      doConnClose();
      return;
    }
    assert(ret == len);
    rxQequeIov_.push_back(std::move(iov));
    pduCount_++;
  }
  rxState_ = kRecvBhs;
  pduCount_++;
}

void AgentConn::doRecvPdu()
{

}

void AgentConn::doConnRead() {
   if (unlikely(fd_ < 0)) {
      MSF_TRACE << "Conn has been closed, cannot read buffer";
      return;
    }
    doRecvBhs();
    // int ret;
    // int count = 0;
    // do {
    //   switch (rxState_)
    //   {
    //     case kRecvBhs:
    //       // doRecvBhs();
    //       return;
    //     case kRecvPdu:
    //       doRecvPdu();
    //       break; 
    //     default:
    //       break;
    //   }
    // } while (true);
    // readCb_();
}

bool AgentConn::writeIovec(struct iovec iov)
{
  if (fd_ < 0) {
    MSF_TRACE << "Conn has been closed, cannot send buffer";
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  txQequeIov_.push_back(std::move(iov));
  txQequeSize_ += iov.iov_len;
  return true;
}

bool AgentConn::writeBuffer(char *data, const uint32_t len)
{
  if (fd_ < 0) {
    MSF_WARN << "Conn has been closed, cannot send buffer";
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  struct iovec iov = { data, len };
  txQequeIov_.push_back(std::move(iov));
  txQequeSize_ += len;
  return true;
}

bool AgentConn::writeBuffer(void *data, const uint32_t len)
{
  return writeBuffer(static_cast<char*>(data), len);
}

void AgentConn::updateBusyIov() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (txQequeIov_.size() > 0) {
    std::move(txQequeIov_.begin(), txQequeIov_.end(), back_inserter(txBusyIov_));
    txNeedSend_ += txQequeSize_;
    txQequeSize_ = 0;
  }
}

bool AgentConn::updateTxOffset(const int ret) {
    txTotalSend_ += ret;
    txNeedSend_ -= ret;
    // https://blog.csdn.net/strengthennn/article/details/97645912
    // https://blog.csdn.net/yang3wei/article/details/7589344
    // https://www.cnblogs.com/dabaopku/p/3912662.html
    /* Half send */
    uint32_t txIovOffset = ret;
    auto iter = txBusyIov_.begin();
    while (iter != txBusyIov_.end()) {
      if (txIovOffset > iter->iov_len) {
        MSF_INFO << "iter->iov_len1: " << iter->iov_len << " txIovOffset:" << txIovOffset;
        txIovOffset -= iter->iov_len;
        mpool_->free(iter->iov_base);
        iter = txBusyIov_.erase(iter);
        ++iter;
      } else if (txIovOffset == iter->iov_len) {
        MSF_INFO << "iter->iov_len2: " << iter->iov_len << " txIovOffset:" << txIovOffset;
        mpool_->free(iter->iov_base);
        iter = txBusyIov_.erase(iter);
        break;
      } else {
        MSF_INFO << "iter->iov_len3: " << iter->iov_len << " txIovOffset:" << txIovOffset;
        iter->iov_base = static_cast<char*>(iter->iov_base) + txIovOffset;
        iter->iov_len -= txIovOffset;
        break;
      }
    }
    if (unlikely(txNeedSend_ > 0)) {
      return false;
    }
    return true;
}

void AgentConn::doConnWrite() {
    if (unlikely(fd_ < 0)) {
      return;
    }
    updateBusyIov();

    int ret;
    int count = 0;
    do {
      ret = SendMsg(fd_, &*txBusyIov_.begin(), txBusyIov_.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
      MSF_INFO << "Send ret: " << ret << " needed: " << txNeedSend_;
      MSF_INFO << "Send iovcnt: " << txBusyIov_.size();
      if (unlikely(ret <= 0)) {
        /* Maybe there is no data anymore */
        if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
          if ((++count) > 4) {
            return;
          }
          continue;
        }
        MSF_ERROR << "Send buffer failed for fd: " << fd_;
        doConnClose();
        return;
      } else {
        if (unlikely(!updateTxOffset(ret))) {
          continue;
        } else {
          disableWriting();
          return;
        }
      }
    } while (true);
}

void AgentConn::doConnClose()
{
  Connector::close();
  auto iter = txBusyIov_.begin();
  while (iter != txBusyIov_.end()) {
    mpool_->free(iter->iov_base);
    iter = txBusyIov_.erase(iter);
    ++iter;
  }
  iter = txQequeIov_.begin();
  while (iter != txQequeIov_.end()) {
    mpool_->free(iter->iov_base);
    iter = txQequeIov_.erase(iter);
    ++iter;
  }
  // iter = rxQequeIov_.begin();
  // while (iter != rxQequeIov_.end()) {
  //   mpool_->free(iter->iov_base);
  //   iter = rxQequeIov_.erase(iter);
  //   ++iter;
  // }
}

}
}