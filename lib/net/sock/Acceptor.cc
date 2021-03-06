
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
#include "Acceptor.h"

#include <butil/logging.h>
#include <fcntl.h>

#include "GccAttr.h"
#include "SockUtils.h"

using namespace MSF;

namespace MSF {

Acceptor::Acceptor(const InetAddress &addr, const NewConnCb &cb)
    : addr_(addr), new_conn_cb_(std::move(cb)) {
  if (!StartListen()) {
    LOG(FATAL) << "Fail to listen for " << addr.hostPort2String();
  }
  OpenIdleFd();
}

Acceptor::~Acceptor() {
  CloseSocket(idle_fd_);
  CloseSocket(sfd_);
}

void Acceptor::OpenIdleFd() {
  idle_fd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
  if (idle_fd_ < 0) {
    LOG(ERROR) << "Open idle fd failed, errno: " << errno;
  }
}

void Acceptor::DiscardAccept() {
  /* 立即关闭这个连接, 表示服务器不再提供服务
   * 因为系统资源已经不足,服务器使用这个方法来拒绝客户的连接 */
  CloseSocket(idle_fd_);
  idle_fd_ = ::accept(sfd_, nullptr, nullptr);
  CloseSocket(idle_fd_);
  OpenIdleFd();
}

bool Acceptor::EnableListen() {
  if (::listen(sfd_, back_log_) < 0) {
    LOG(ERROR) << "Failed to enable listen for fd: " << sfd_;
    return false;
  }
  return true;
}

bool Acceptor::DisableListen() {
  if (::listen(sfd_, 0) < 0) {
    LOG(ERROR) << "Failed to disable listen for fd: " << sfd_;
    return false;
  }
  return true;
}

bool Acceptor::StartListen() {
  uint16_t event = EPOLLIN | EPOLLERR | EPOLLRDHUP;

/* 解决惊群 */
#ifdef EPOLLEXCLUSIVE
  event |= EPOLLEXCLUSIVE;
#endif

  if (addr_.family() == AF_INET || addr_.family() == AF_INET6) {
    if (addr_.proto() == SOCK_STREAM) {
      LOG(INFO) << "host: " << addr_.host() << " port: " << addr_.port();
      sfd_ = CreateTcpServer(addr_.host(), addr_.port(), SOCK_STREAM, 5);
      if (sfd_ < 0) {
        return false;
      }
    } else if (addr_.proto() == SOCK_DGRAM) {
      sfd_ = CreateTcpServer(addr_.host(), addr_.port(), SOCK_DGRAM, 5);
      if (sfd_ < 0) {
        return false;
      }
    }
  } else if (addr_.family() == AF_UNIX) {
    // sfd_ = CreateUnixServer(addr_.host(), 5, 0777);
    if (sfd_ < 0) {
      return false;
    }
  } else {
    LOG(ERROR) << "Not support this family type: " << addr_.family();
    return false;
  }

  started_ = true;
  return true;
}

/* Internal wrapper around 'accept' or 'accept4' to provide Linux-style
 * support for syscall-saving methods where available.
 *
 * In addition to regular accept behavior, you can set one or more of flags
 * SOCK_NONBLOCK and SOCK_CLOEXEC in the 'flags' argument, to make the
 * socket nonblocking or close-on-exec with as few syscalls as possible.
 */
void Acceptor::AcceptCb() {
  bool stop = false;
  int new_fd = -1;
  struct sockaddr_storage addr;
  socklen_t addrlen = sizeof(struct sockaddr_storage);

  do {
    ::memset(&addr, 0, sizeof(struct sockaddr_storage));
    /* NOTE: we aren't using static inline function here; because accept4
     * requires defining _GNU_SOURCE and we don't want users to be forced to
     * define it in their application */
#if defined(HAVE_ACCEPT4) && defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
    new_fd = ::accept4(_sfd_, (struct sockaddr *)&addr, addrlen, SOCK_NONBLOCK);
    if (new_fd >= 0 || (errno != EINVAL && errno != ENOSYS)) {
      /* A nonnegative result means that we succeeded, so return.
       * Failing with EINVAL means that an option wasn't supported,
       * and failing with ENOSYS means that the syscall wasn't
       * there: in those cases we want to fall back. Otherwise, we
       * got a real error, and we should return. */
      break;
    }
#endif
    /*
     * The returned socklen is ignored here, because sockaddr_in and
     * sockaddr_in6 socklens are not changed.  As to unspecified sockaddr_un
     * it is 3 byte length and already prepared, because old BSDs return zero
     * socklen and do not update the sockaddr_un at all; Linux returns 2 byte
     * socklen and updates only the sa_family part; other systems copy 3 bytes
     * and truncate surplus zero part.  Only bound sockaddr_un will be really
     * truncated here.
     */
    new_fd = ::accept(sfd_, (struct sockaddr *)&addr, &addrlen);
    if (new_fd < 0) {
      if (unlikely(errno == EINTR)) continue;

      /* these are transient, so don't log anything */
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED) {
        stop = true;
      } else if (errno == EMFILE || errno == ENFILE) {
        //系统的上限ENFILE 进程的上限EMFILE
        LOG(ERROR) << "Too many open connections, give up accept.";
        /* Read the section named "The special problem of
         * accept()ing when you can't" in libev's doc.
         * By Marc Lehmann, author of libev.*/
        DiscardAccept();
        DisableListen();
        stop = true;
      } else {
        perror("accept()");
        stop = true;
      }
      break;
    } else {
      SetNonBlock(new_fd, true);
      /* 监听线程放在单独的线程, 如果想分发到线程池也可以*/
      new_conn_cb_(new_fd, EPOLLIN);
    }
  } while (stop);

  return;
}

void Acceptor::ErrorCb() {
  LOG(INFO) << "Acceptor close for fd: " << sfd_;
  CloseSocket(sfd_);
  sfd_ = -1;
  new_conn_cb_ = nullptr;
  started_ = false;
}

}  // namespace MSF