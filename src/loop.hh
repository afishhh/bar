#pragma once

#include <any>
#include <atomic>
#include <cassert>
#include <chrono>
#include <compare>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "format.hh"
#include "log.hh"

class VEventQueue;
// Base class for all events to be used in the EventLoop.
// The class cannot be instantiated directly but must be derived from.
class Event {
protected:
  Event(){};
};
namespace detail {
class DerivedEventTag {};
}; // namespace detail
template <std::derived_from<Event> Base>
class DerivedEvent : public Base, public detail::DerivedEventTag {
public:
  using _derived_event_base = Base;

  template <typename... Args>
  DerivedEvent(Args &&...args) : Base(std::forward<Args>(args)...) {}
};

class EventLoop {
public:
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;
  using duration = clock::duration;
  using timer_callback = std::function<void(duration)>;
  using callback_id = std::uint32_t;

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

  template <std::derived_from<Event>> class EventQueue;
  class VEventQueue {
  public:
    virtual ~VEventQueue() {}

    // These are the only things we need to do without knowing the type of the
    // event.
    virtual bool try_remove_callback(callback_id id) = 0;
    virtual void flush() = 0;

    template <std::derived_from<Event> E> EventQueue<E> *into_queue_of() {
#ifndef NDEBUG
      // NOTE: This is here only for debugging purposes.
      //       Since we always know what our desired type is and we can just
      //       reinterpret_cast directly to it.
      if (dynamic_cast<EventQueue<E> *>(this) == nullptr) {
        std::print(error, "Invalid into_queue_of<E> call (this: {}, E: {})",
                   typeid(*this).name(), typeid(E).name());
        std::exit(1);
      }
#endif
      return reinterpret_cast<EventQueue<E> *>(this);
    }
  };

  std::map<std::type_index, std::unique_ptr<VEventQueue>> _event_queues;
  std::atomic<callback_id> _current_event_callback_id =
      std::numeric_limits<callback_id>::min();

  // FIXME: Propagate event deep inside Event's inheritance hierarchy
  //        to EventQueues of all classes on the path.
  //        The easiest way to do such a thing is to use reflection
  //        which is yet supported in C++.
  //        Once reflection is supported the whole thing can be refactored.
  //
  //        For now this is hacked together using DerivedEvent.
  template <std::derived_from<Event> E> class EventQueue : public VEventQueue {
    void fire_base_event(const E &event) {
        if constexpr (std::derived_from<E, detail::DerivedEventTag>) {
          using base = typename E::_derived_event_base;

          const auto &queues = EventLoop::instance()._event_queues;
          if (auto it = queues.find(typeid(base)); it != queues.end())
            it->second->into_queue_of<base>()->flush_single(event);
        }
    }


  public:
    std::mutex _mutex;
    std::vector<E> queued_events;
    std::map<uint32_t, std::function<void(const E &)>> callbacks;

    bool try_remove_callback(callback_id id) override {
      std::lock_guard<std::mutex> lock(_mutex);
      return callbacks.erase(id) != 0;
    }
    void flush_single(const E &event) {
      _mutex.lock();
      for (auto &[_, callback] : callbacks) {
        _mutex.unlock();
        fire_base_event(event);
        callback(event);
        _mutex.lock();
      }
      _mutex.unlock();
    }

    void flush() override {
      _mutex.lock();

      for (auto &event : queued_events) {
        for (auto &[_, callback] : callbacks) {
          _mutex.unlock();
          fire_base_event(event);
          callback(event);
          _mutex.lock();
        }
      }
      _mutex.unlock();

      queued_events.clear();
    }
  };

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

  template <std::derived_from<Event> E, typename Callback>
  requires std::constructible_from<std::function<void(const E &)>, Callback>
      callback_id on(Callback cb) {
    if (auto it = _event_queues.lower_bound(typeid(E));
        it == _event_queues.end() || it->first != typeid(E)) {
      auto new_queue = std::make_unique<EventQueue<E>>();
      auto id = _current_event_callback_id.fetch_add(1);
      new_queue->callbacks.emplace(id, std::move(cb));
      _event_queues.emplace_hint(it, typeid(E), std::move(new_queue));
      return id;
    } else {
      auto id = _current_event_callback_id.fetch_add(1);
      auto queue = it->second->into_queue_of<E>();
      std::lock_guard<std::mutex> lock(queue->_mutex);
      queue->callbacks.emplace(id, std::move(cb));
      return id;
    }
  }
  template <std::derived_from<Event> E> void fire_event(const E &event) {
    if (auto it = _event_queues.find(typeid(E)); it != _event_queues.end()) {
      auto queue = it->second->into_queue_of<E>();
      std::lock_guard<std::mutex> lock(queue->_mutex);
      queue->queued_events.emplace_back(event);
    }
  }

  bool off(callback_id id) {
    for (auto &[_, queue] : _event_queues)
      if (queue->try_remove_callback(id))
        return true;
    return false;
  }
  template <std::derived_from<Event> E> bool off(callback_id id) {
    auto queue = _event_queues[typeid(E)]->into_queue_of<E>();
    std::lock_guard<std::mutex> lock(queue->_mutex);
    return queue->callbacks.erase(id) != 0;
  }

  void add_timer(bool repeat, duration interval,
                 const timer_callback &callback);
};

#define EV (EventLoop::instance())
