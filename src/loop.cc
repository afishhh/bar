#include "loop.hh"
#include "executor.hh"

#include <chrono>
#include <csignal>
#include <iostream>
#include <optional>
#include <ratio>
#include <system_error>
#include <thread>

std::map<int, int> SignalEvent::s_attached;

void SignalEvent::_action(int signal, siginfo_t *info, void *) {
  EV.fire_event(SignalEvent(signal, *info));
}

void SignalEvent::attach(int signal, int flags) {
  if (auto it = s_attached.find(signal); it != s_attached.end()) {
    // this api is terrible :sob:
    if (it->second != flags)
      throw std::logic_error("mixed flag SignalEvent::attach");
    return;
  } else
    s_attached.emplace_hint(it, signal, flags);

  struct sigaction act;
  act.sa_sigaction = &_action;
  act.sa_flags = SA_SIGINFO | flags;

  if (sigaction(signal, &act, nullptr))
    throw std::system_error(errno, std::generic_category(), "sigaction");
}

void StopEvent::attach_to_signals() {
  static bool already_attached = false;
  if (already_attached)
    return;
  else
    already_attached = true;

  SignalEvent::attach(SIGINT);
  SignalEvent::attach(SIGTERM);
  EV.on<SignalEvent>([](SignalEvent const &ev) {
    if (ev.signum == SIGINT || ev.signum == SIGTERM)
      EV.fire_event(StopEvent(Cause::SIGNAL));
  });
}

EventLoop::EventLoop() { _event_queues.emplace(typeid(StopEvent), std::make_unique<VectorEventQueue<StopEvent>>()); }
EventLoop::~EventLoop() {}

void EventLoop::add_oneshot(task::oneshot::callback callback) { _tasks.emplace(task::oneshot{callback}); }
void EventLoop::add_oneshot(time_point run_point, task::timed_oneshot::callback callback) {
  _tasks.emplace(task::timed_oneshot{callback, run_point});
}
void EventLoop::add_oneshot(duration run_delay, task::timed_oneshot::callback callback) {
  this->add_oneshot(clock::now() + run_delay, callback);
}

void EventLoop::add_timer(duration interval, task::repeated::callback callback) {
  _tasks.emplace(task::repeated(callback, interval));
}

bool EventLoop::check_stop() {
  auto it = _event_queues.find(std::type_index(typeid(StopEvent)));
  if (it == _event_queues.end())
    return false;

  auto stop_queue = it->second->into_queue_of<StopEvent>();
  if (!stop_queue->empty()) {
    stop_queue->flush(*_executor);
    _stopped = true;
  }
  return _stopped;
}

void EventLoop::pump() {
  if (check_stop())
    return;

  if (!_tasks.empty()) {
    auto const &next = _tasks.top();

    if (check_stop())
      return;

    std::visit(overloaded{
                   [](task::oneshot const &) {},
                   [](task::timed_oneshot const &t) { std::this_thread::sleep_until(t.time); },
                   [](task::repeated const &t) { std::this_thread::sleep_until(t.next); },
               },
               next.var());

    do {
      auto current = _tasks.top();
      _tasks.pop();

      std::visit(overloaded{[](task::oneshot const &t) { t.fn(); }, [](task::timed_oneshot const &t) { t.fn(); },
                            [this](task::repeated &t) {
                              t.fn(clock::now() - t.last);

                              auto now = clock::now();
                              t.last = now;

                              while (t.next < now)
                                t.next += t.interval;

                              _tasks.push(std::move(t));
                            }},
                 current.var());
    } while (std::visit(overloaded{
                            [](task::oneshot const &) { return true; },
                            [](task::timed_oneshot const &t) { return t.time < clock::now(); },
                            [](task::repeated const &t) { return t.next < clock::now(); },
                        },
                        _tasks.top().var()));
  }

  {
    ScopedExecutor executor(*_executor);

    for (auto &[index, queue] : _event_queues) {
      // The StopEvent queue is special cased and checked in every check_stop
      if (index == std::type_index(typeid(StopEvent)))
        continue;

      if (!queue->empty())
        executor.execute([this, &queue] { queue->flush(*_executor); });
    }
  }

  if (check_stop())
    return;
}

void EventLoop::run() {
  while (!_tasks.empty() && !check_stop())
    try {
      this->pump();
    } catch (std::exception &e) {
      ExceptionAction act = ExceptionAction::STOP;
      if (_exception_handler.has_value())
        act = (*_exception_handler)(e);

      if (act == ExceptionAction::STOP) {
        fire_event(StopEvent(StopEvent::Cause::EXCEPTION));
        check_stop();
        _stopped = false;
        throw;
      }
    }

  _stopped = false;
}

void EventLoop::stop() { fire_event(StopEvent(StopEvent::Cause::STOP)); }
