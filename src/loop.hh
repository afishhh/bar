#pragma once

#include <chrono>
#include <compare>
#include <cstddef>
#include <functional>
#include <queue>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

class EventLoop {
public:
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;
  using duration = clock::duration;
  using timer_callback = std::function<void(duration)>;
  using event_callback = std::function<void()>;
  enum class Event { REDRAW };

private:
  struct Timer {
    time_point next;
    // If empty timer is one-shot
    std::optional<duration> interval;
    timer_callback callback;

    time_point last = next;
    auto operator<=>(const Timer &other) const { return next <=> other.next; }
  };

  std::priority_queue<Timer, std::vector<Timer>, std::greater<Timer>> _timers;
  std::unordered_map<Event, std::vector<event_callback>> _events;
  // FIXME: Use a deduplicated data structure that keeps track of the order of
  //        insertion.
  //
  //        Currently a std::set is used but the set doesn't keep track of the
  //        order of insertion which may be unintuitive.
  std::set<Event> _queued_events;

  static EventLoop *_instance;

  EventLoop();
  ~EventLoop();

public:
  EventLoop(const EventLoop &) = delete;
  EventLoop &operator=(const EventLoop &) = delete;
  EventLoop(EventLoop &&) = delete;
  EventLoop &operator=(EventLoop &&) = delete;

  static EventLoop &instance();

  void run();
  void stop();

  void on_event(Event, event_callback callback);
  void fire_event(Event);

  void add_timer(bool repeat, duration interval,
                 const timer_callback &callback);
};
