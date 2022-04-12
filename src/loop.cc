#include "loop.hh"
#include "dwmipcpp/types.hpp"

#include <chrono>
#include <iostream>
#include <optional>
#include <ratio>
#include <thread>

using namespace std::literals;

EventLoop::EventLoop() {}
EventLoop::~EventLoop() {}

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
    auto timer = _timers.top();
    _timers.pop();
    std::this_thread::sleep_until(timer.next);

    timer.callback(clock::now() - timer.last);
    auto now = clock::now();
    timer.last = now;

    if (timer.interval) {
      timer.next += *timer.interval;
      while (timer.next < now) {
        timer.next += *timer.interval;
        std::cerr << "A timer callback was skipped since timer execution took "
                     "too long!\n";
      }
      _timers.push(std::move(timer));
    }
  }
}

void EventLoop::stop() {
  while (!_timers.empty())
    _timers.pop();
}

size_t EventLoop::_next_event_id = 0;
size_t EventLoop::create_event(Event ev) {
  _events[_next_event_id] = std::move(ev);
  return _next_event_id++;
}
void EventLoop::on_event(size_t id, event_callback callback) {
  _events[id].callbacks.push_back(std::move(callback));
}
void EventLoop::fire_event(size_t id) {
  auto &event = _events[id];
  if (!event.batched || !_events_batched.contains(id)) {
    add_timer(false, event.batched ? event.batch_time : 0ns, [&](auto delta) {
      for (auto &callback : event.callbacks)
        callback();
      _events_batched.erase(id);
    });
    _events_batched.insert(id);
  }
}
