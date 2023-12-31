#pragma once

#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <set>
#include <sys/signalfd.h>
#include <system_error>
#include <unistd.h>

#include "loop.hh"

class SignalEvent : public Event {
  SignalEvent() {}

  static int _sigfd;
  static sigset_t _attached_mask;
  struct MaskConstructor;
  static std::mutex _mask_mutex;

public:
  int signum;
  signalfd_siginfo info;

  static void setup() {
    sigset_t fullmask;
    if (sigfillset(&fullmask) < 0)
      throw std::system_error(errno, std::generic_category(), "sigfillset");

    // Blocking these signals results in undefined behaviour
    if (sigdelset(&fullmask, SIGBUS) < 0)
      throw std::system_error(errno, std::generic_category(), "sigdelset");
    if (sigdelset(&fullmask, SIGFPE) < 0)
      throw std::system_error(errno, std::generic_category(), "sigdelset");
    if (sigdelset(&fullmask, SIGILL) < 0)
      throw std::system_error(errno, std::generic_category(), "sigdelset");
    if (sigdelset(&fullmask, SIGSEGV) < 0)
      throw std::system_error(errno, std::generic_category(), "sigdelset");

    if (pthread_sigmask(SIG_BLOCK, &fullmask, NULL) < 0)
      throw std::system_error(errno, std::generic_category(), "sigprocmask");

    _sigfd = signalfd(_sigfd, &fullmask, SFD_CLOEXEC);
    if (_sigfd < 0)
      throw std::system_error(errno, std::generic_category(), "signalfd");

    std::thread([fd = _sigfd] {
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

        bool handled;
        {
          std::unique_lock lg(_mask_mutex);
          handled = sigismember(&_attached_mask, info.ssi_signo);
        }

        if (handled) {
          SignalEvent ev;
          ev.signum = info.ssi_signo;
          ev.info = info;
          EV.fire_event(std::move(ev));
        } else
          warn << "Unhandled signal SIG" << sigabbrev_np(info.ssi_signo) << " received\n";
      }
    }).detach();
  }

  template <std::same_as<int>... Args> static void attach(Args... signals) {
    for (int signal : {signals...}) {
      assert(signal > 0 && signal <= SIGRTMAX);

      if (sigismember(&_attached_mask, signal))
        continue;

      {
        std::unique_lock lg(_mask_mutex);
        if (sigismember(&_attached_mask, signal))
          continue;
        if (sigaddset(&_attached_mask, signal) < 0)
          throw std::system_error(errno, std::generic_category(), "sigaddset");
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
inline sigset_t SignalEvent::_attached_mask = make_empty_sigset();
;
inline int SignalEvent::_sigfd = -1;
