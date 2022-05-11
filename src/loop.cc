#include "loop.hh"

#include <chrono>
#include <iostream>
#include <optional>
#include <ratio>
#include <thread>

EventLoop::EventLoop() {}
EventLoop::~EventLoop() {}

EventLoop &EventLoop::instance() {
  static EventLoop instance;
  return instance;
}

void EventLoop::add_timer(bool repeat, duration interval,
                          const timer_callback &callback) {
  _timers.push({
      .next = clock::now() + interval,
      .interval = repeat ? std::optional(interval) : std::nullopt,
      .callback = callback,
  });
}

void EventLoop::run() {
  while (!_timers.empty()) {
    std::this_thread::sleep_until(_timers.top().next);
    do {
      auto timer = _timers.top();
      _timers.pop();

      timer.callback(clock::now() - timer.last);
      auto now = clock::now();
      timer.last = now;

      if (timer.interval) {
        while (timer.next < now)
          timer.next += *timer.interval;
        _timers.push(std::move(timer));
      }
    } while (_timers.top().next < clock::now());

    for (auto &[index, queue] : _event_queues)
      queue->flush();
  }
}

void EventLoop::stop() {
  while (!_timers.empty())
    _timers.pop();
}
