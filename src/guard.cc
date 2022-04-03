#include <csignal>
#include <ranges>

#include "guard.hh"

guard_like *guard_like::s_head{nullptr};

void guard_like::cleanup_all() {
  guard_like *cur = s_head;
  while (cur) {
    guard_like *next = cur->_next;
    cur->cleanup();
    cur = next;
  }
}
void guard_like::maybe_init() {
  static bool s_did_init = false;
  if (!s_did_init) {
    atexit(cleanup_all);
    signal(SIGINT, [](int) { exit(1); });
    signal(SIGTERM, [](int) { exit(1); });
    s_did_init = true;
  }
}
guard_like::guard_like() {
  maybe_init();
  if (s_head) {
    s_head->_prev = this;
    _next = s_head;
    s_head = this;
  } else
    s_head = this;
}
guard_like::~guard_like() {
  if (_prev)
    _prev->_next = _next;
  if (_next)
    _next->_prev = _prev;
  if (s_head == this)
    s_head = _next;
}
