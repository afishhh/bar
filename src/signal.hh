#pragma once

#include "loop.hh"
#include <bits/types/siginfo_t.h>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <set>
#include <system_error>

class SignalEvent : public Event {
  SignalEvent() {}

  static void _handler(int num, siginfo_t *info, void *) {
    auto event = SignalEvent();
    event.signum = num;
    event.info = *info;
    EV.fire_event(event);
  }

public:
  int signum;
  siginfo_t info;

  static void attach(int signum, int additional_sa_flags = 0) {
    assert(signum > 0 && signum <= SIGRTMAX);
    static std::set<int> already_registered;

    if(already_registered.contains(signum))
      return;
    already_registered.insert(signum);

    struct sigaction action;
    action.sa_sigaction = &_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO | additional_sa_flags;
    if (sigaction(signum, &action, nullptr) < 0)
      throw std::system_error(errno, std::system_category());
  }
};
