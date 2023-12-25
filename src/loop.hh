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
#include <variant>
#include <vector>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "log.hh"
#include "util.hh"

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
class DerivedEvent : public Base, virtual public detail::DerivedEventTag {
public:
  using _derived_event_base = Base;

  template <typename... Args>
  explicit DerivedEvent(Args &&...args) : Base(std::forward<Args>(args)...) {}
};

class StopEvent : public Event {
public:
  enum class Cause {
    STOP,
    SIGNAL,
    EXCEPTION
  } cause;

  static void attach_to_signals();

private:
  friend class EventLoop;
  StopEvent(Cause cause) : cause(cause) {}
};

class EventLoop {
public:
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;
  using duration = clock::duration;
  // TODO: Rename to event_callback_id
  using callback_id = std::uint32_t;

private:
  struct task {
    struct oneshot {
      using callback = std::function<void()>;

      callback fn;
    };
    struct timed_oneshot {
      using callback = std::function<void()>;

      callback fn;
      time_point time;
    };
    struct repeated {
      using callback = std::function<void(duration)>;

      repeated(callback &&fn, duration &&interval, time_point &&next)
          : fn(fn), next(next), interval(interval) {}
      repeated(callback &&fn, duration &&interval)
          : fn(fn), next(clock::now() + interval), interval(interval) {}
      // NOTE: I'm not making all the possible variations of this
      repeated(const callback &fn, const duration &interval,
               const time_point &next)
          : fn(fn), next(next), interval(interval) {}
      repeated(const callback &fn, const duration &interval)
          : fn(fn), next(clock::now() + interval), interval(interval) {}

      callback fn;
      time_point next;
      time_point last = next;
      duration interval;
    };

  private:
    using variant = std::variant<oneshot, timed_oneshot, repeated>;
    variant _v;

  public:
    task(oneshot const &os) : _v(os) {}
    task(oneshot &&os) : _v(os) {}
    task(timed_oneshot const &tos) : _v(tos) {}
    task(timed_oneshot &&tos) : _v(tos) {}
    task(repeated const &r) : _v(r) {}
    task(repeated &&r) : _v(r) {}

    operator variant const &() const { return _v; }
    operator variant &() { return _v; }
    variant const &var() const { return _v; }
    variant &var() { return _v; }

    auto operator<=>(task const &other) const {
      return std::visit(
          overloaded{
              [](oneshot const &, oneshot const &) {
                return std::strong_ordering::equal;
              },
              [](oneshot const &, auto const &) {
                return std::strong_ordering::less;
              },
              [](auto const &, oneshot const &) {
                return std::strong_ordering::greater;
              },
              [](repeated const &lhs, repeated const &rhs) {
                return lhs.next <=> rhs.next;
              },
              [](timed_oneshot const &lhs, timed_oneshot const &rhs) {
                return lhs.time <=> rhs.time;
              },
              [](repeated const &lhs, timed_oneshot const &rhs) {
                return lhs.next <=> rhs.time;
              },
              [](timed_oneshot const &lhs, repeated const &rhs) {
                return lhs.time <=> rhs.next;
              },
          },
          this->_v, other._v);
    }
  };

  std::priority_queue<task, std::vector<task>, std::greater<task>> _tasks;

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
        fmt::print(error, "Invalid into_queue_of<E> call (this: {}, E: {})",
                   typeid(*this).name(), typeid(E).name());
        std::exit(1);
      }
#endif
      return static_cast<EventQueue<E> *>(this);
    }
  };

  std::map<std::type_index, std::unique_ptr<VEventQueue>> _event_queues;

  template <std::derived_from<Event> E> EventQueue<E> *find_event_queue() {
    if (auto it = _event_queues.find(typeid(E)); it != _event_queues.end())
      return it->second->into_queue_of<E>();
    return nullptr;
  }

  std::atomic<callback_id> _current_event_callback_id =
      std::numeric_limits<callback_id>::min();

  // FIXME: Propagate event deep inside Event's inheritance hierarchy
  //        to EventQueues of all classes on the path.
  //        The easiest way to do such a thing is to use reflection
  //        which isn't yet supported in C++.
  //        Once reflection is supported the whole thing can be refactored.
  //
  //        For now this is hacked together using DerivedEvent.
  template <std::derived_from<Event> E> class EventQueue : public VEventQueue {
    void fire_base_event(const E &event) {
      if constexpr (std::derived_from<E, detail::DerivedEventTag>) {
        using base = typename E::_derived_event_base;

        // FIXME: Don't use ::instance() here
        if (auto queue = EventLoop::instance().find_event_queue<base>())
          queue->fire_one(event);
      } else if constexpr (!std::same_as<E, Event>) {
        if (auto queue = EventLoop::instance().find_event_queue<Event>())
          queue->fire_one(event);
      }
    }

    std::function<void(const E &)> *_current_callback = nullptr;
    bool _remove_current_callback;

  public:
    mutable std::mutex _mutex;
    std::vector<E> queued_events;
    std::map<uint32_t, std::function<void(const E &)>> callbacks;

    bool empty() const {
      std::lock_guard lg(_mutex);
      return queued_events.empty();
    }

    bool try_remove_callback(callback_id id) override {
      std::lock_guard<std::mutex> lock(_mutex);
      if (_current_callback == &callbacks[id]) {
        _remove_current_callback = true;
        return true;
      } else
        return callbacks.erase(id) != 0;
    }
    void fire_one(const E &event) {
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

      auto events = std::move(queued_events);
      for (auto &event : events) {
        for (auto it = callbacks.begin(); it != callbacks.end();) {
          auto &[_, callback] = *it;
          _current_callback = &callback;
          _mutex.unlock();

          fire_base_event(event);
          callback(event);

          _mutex.lock();
          if (_remove_current_callback)
            it = callbacks.erase(it);
          else
            ++it;
        }
      }

      _current_callback = nullptr;
      _mutex.unlock();
    }
  };

  bool _stopped{false};

  EventLoop();
  ~EventLoop();

  bool check_stop();

public:
  EventLoop(const EventLoop &) = delete;
  EventLoop &operator=(const EventLoop &) = delete;
  EventLoop(EventLoop &&) = delete;
  EventLoop &operator=(EventLoop &&) = delete;

  inline static EventLoop &instance() {
    static EventLoop instance;
    return instance;
  }

  void run();
  void pump();
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

  template <std::derived_from<Event> E> void fire_event(E const &event) {
    if (auto it = _event_queues.find(typeid(E)); it != _event_queues.end()) {
      auto queue = it->second->into_queue_of<E>();
      std::lock_guard<std::mutex> lock(queue->_mutex);
      queue->queued_events.emplace_back(event);
    }
  }

  template <std::derived_from<Event> E> void fire_event(E &&event) {
    if (auto it = _event_queues.find(typeid(E)); it != _event_queues.end()) {
      auto queue = it->second->into_queue_of<E>();
      std::lock_guard<std::mutex> lock(queue->_mutex);
      queue->queued_events.emplace_back(std::move(event));
    }
  }

  bool off(callback_id id) {
    for (auto &[_, queue] : _event_queues)
      if (queue->try_remove_callback(id))
        return true;
    return false;
  }
  template <std::derived_from<Event> E> bool off(callback_id id) {
    if (auto basic_queue = _event_queues.find(typeid(E));
        basic_queue != _event_queues.end()) {
      auto queue = (basic_queue->second)->into_queue_of<E>();
      return queue->try_remove_callback(id);
    } else {
      return false;
    }
  }

  template <std::derived_from<Event> E> void wait() {
    return wait([](E const &) { return true; });
  }

  template <std::derived_from<Event> E, typename F>
  requires requires(F fn, E const &ev) {
    { fn(ev) } -> std::same_as<bool>;
  }
  E wait(F condition) {
    std::mutex mutex;
    std::condition_variable condvar;
    bool done = false;
    std::optional<E> out;

    auto cid = this->on<E>([&](E const &ev) {
      if (condition(ev)) {
        std::unique_lock lock(mutex);
        if (!done) {
          done = true;
          out = ev;
          lock.unlock();
          condvar.notify_all();
        }
      }
    });

    std::unique_lock lock(mutex);
    while (!done) {
      {
        if (condvar.wait_for(lock, std::chrono::milliseconds(5)) ==
                std::cv_status::no_timeout ||
            done)
          break;
      }

      lock.unlock();
      this->pump();
      lock.lock();
    }

    this->off(cid);
  }

  void add_oneshot(task::oneshot::callback);
  void add_oneshot(time_point run_point, task::timed_oneshot::callback);
  void add_oneshot(duration run_delay, task::timed_oneshot::callback);

  void add_timer(duration interval, task::repeated::callback);
};

#define EV (EventLoop::instance())

class owned_callback_id {
  EventLoop::callback_id _id;

public:
  owned_callback_id(EventLoop::callback_id id) : _id(id) {}
  ~owned_callback_id() { EV.off(_id); }

  owned_callback_id(owned_callback_id const &) = delete;
  owned_callback_id(owned_callback_id &&) = default;
  owned_callback_id &operator=(owned_callback_id const &) = delete;
  owned_callback_id &operator=(owned_callback_id &&) = default;

  operator EventLoop::callback_id() const { return _id; }
};
