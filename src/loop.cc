#include "loop.hh"

#include <chrono>
#include <iostream>
#include <optional>
#include <ratio>
#include <thread>

EventLoop::EventLoop() {}
EventLoop::~EventLoop() {}

void EventLoop::add_oneshot(task::oneshot::callback callback) {
  _tasks.emplace(task::oneshot{callback});
}
void EventLoop::add_oneshot(time_point run_point,
                            task::timed_oneshot::callback callback) {
  _tasks.emplace(task::timed_oneshot{callback, run_point});
}
void EventLoop::add_oneshot(duration run_delay,
                            task::timed_oneshot::callback callback) {
  this->add_oneshot(clock::now() + run_delay, callback);
}

void EventLoop::add_timer(duration interval,
                          task::repeated::callback callback) {
  _tasks.emplace(task::repeated(callback, interval));
}

void EventLoop::pump() {
  if (!_tasks.empty()) {
    auto const &next = _tasks.top();

    std::visit(overloaded{
                   [](task::oneshot const &) {},
                   [](task::timed_oneshot const &t) {
                     std::this_thread::sleep_until(t.time);
                   },
                   [](task::repeated const &t) {
                     std::this_thread::sleep_until(t.next);
                   },
               },
               next.var());

    do {
      auto current = std::move(_tasks.top());
      _tasks.pop();

      std::visit(overloaded{[](task::oneshot const &t) { t.fn(); },
                            [](task::timed_oneshot const &t) { t.fn(); },
                            [this](task::repeated &t) {
                              t.fn(clock::now() - t.last);

                              auto now = clock::now();
                              t.last = now;

                              while (t.next < now)
                                t.next += t.interval;

                              _tasks.push(std::move(t));
                            }},
                 current.var());
    } while (std::visit(
        overloaded{
            [](task::oneshot const &) { return true; },
            [](task::timed_oneshot const &t) { return t.time < clock::now(); },
            [](task::repeated const &t) { return t.next < clock::now(); },
        },
        _tasks.top().var()));
  }

  for (auto &[index, queue] : _event_queues)
    queue->flush();
}

void EventLoop::run() {
  while (!_tasks.empty())
    this->pump();
}

void EventLoop::stop() {
  while (!_tasks.empty())
    _tasks.pop();
}
