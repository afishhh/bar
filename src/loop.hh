#pragma once

#include <chrono>
#include <compare>
#include <cstddef>
#include <functional>
#include <queue>
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

private:
  struct Timer {
    time_point next;
    // If empty timer is one-shot
    std::optional<duration> interval;
    timer_callback callback;

    time_point last = next;
    auto operator<=>(const Timer &other) const { return other.next <=> next; }
  };

  std::priority_queue<Timer> _timers;
  std::unordered_map<size_t, std::vector<event_callback>> _events;
  static size_t _next_event_id;

public:
  EventLoop();
  ~EventLoop();

  void run();
  void stop();

  static size_t create_event();
  void on_event(size_t id, event_callback callback);
  void fire_event(size_t id);

  void add_timer(bool repeat, duration interval,
                 const timer_callback &callback);
};
