#pragma once

#include <cassert>
#include <cerrno>
#include <csignal>
#include <set>
#include <sys/signalfd.h>
#include <system_error>
#include <unistd.h>

#include "loop.hh"

class SignalEvent : public Event {
  SignalEvent() {}

  static int _sigfd;
  static sigset_t _mask;
  struct MaskConstructor;
  static std::mutex _mask_mutex;

  static void _handler(int num, siginfo_t *info, void *) {
    auto event = SignalEvent();
    event.signum = num;
    event.info = *info;
    EV.fire_event(event);
  }

public:
  int signum;
  siginfo_t info;

  template <std::same_as<int>... Args> static void attach(Args... signals) {
    for (int signal : {signals...}) {
      assert(signal > 0 && signal <= SIGRTMAX);

      if (sigismember(&_mask, signal))
        continue;

      {
        std::unique_lock lg(_mask_mutex);
        if (sigismember(&_mask, signal))
          continue;
        if (sigaddset(&_mask, signal) < 0)
          throw std::system_error(errno, std::generic_category(), "sigaddset");
      }

      if (pthread_sigmask(SIG_BLOCK, &_mask, NULL) < 0)
        throw std::system_error(errno, std::generic_category(), "sigprocmask");

      int ret = signalfd(_sigfd, &_mask, SFD_CLOEXEC);
      if (ret < 0)
        throw std::system_error(errno, std::generic_category(), "signalfd");

      if (_sigfd == -1) {
        _sigfd = ret;

        std::thread([fd = ret] {
          signalfd_siginfo info;
          while (true) {
            ssize_t ret = read(fd, &info, sizeof info);
            if (ret < 0) {
              if (errno == EINTR)
                continue;
              else
                throw std::system_error(errno, std::generic_category(), "read");
            } else if (ret != sizeof info)
              throw std::runtime_error("read() on signalfd returned size != sizeof(signalfd_siginfo)");
          }
        }).detach();
      }
    }
  }
};

inline sigset_t make_empty_sigset() {
  sigset_t set;
  if (sigemptyset(&set) < 0)
    throw std::system_error(errno, std::generic_category(), "sigemptyset");
  return set;
}

inline std::mutex SignalEvent::_mask_mutex;
inline sigset_t SignalEvent::_mask = make_empty_sigset();
;
inline int SignalEvent::_sigfd = -1;
