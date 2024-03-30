#pragma once

#include <any>
#include <atomic>
#include <cassert>
#include <chrono>
#include <compare>
#include <concepts>
#include <condition_variable>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <latch>
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

#include "executor.hh"
#include "log.hh"
#include "util.hh"

class VEventQueue;
// Base class for all events to be used in the EventLoop.
// The class cannot be instantiated directly but must be derived from.
class Event {
protected:
  Event() {}
};

namespace detail {
class DerivedEventTag {};
}; // namespace detail

template <std::derived_from<Event> Base> class DerivedEvent : public Base, virtual public detail::DerivedEventTag {
public:
  using _derived_event_base = Base;

  template <typename... Args> explicit DerivedEvent(Args &&...args) : Base(std::forward<Args>(args)...) {}
};

class SignalEvent : public Event {
  SignalEvent(int signum, siginfo_t info) : signum(signum), info(info) {}
  friend class EventLoop;

  static std::map<int, int> s_attached;

  static void _action(int, siginfo_t *, void *);

public:
  int signum;
  siginfo_t info;

  // NOT THREAD SAFE
  static void attach(int signal, int flags = 0);
};

class StopEvent : public Event {
public:
  enum class Cause { STOP, SIGNAL, EXCEPTION } cause;

  static void attach_to_signals();

private:
  friend class EventLoop;
  friend class SignalEvent;
  StopEvent(Cause cause) : cause(cause) {}
};

class EventLoop {
public:
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;
  using duration = clock::duration;
  // TODO: Rename to event_callback_id
  using callback_id = std::uint32_t;

  friend class SignalEvent;

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

      repeated(callback &&fn, duration &&interval, time_point &&next) : fn(fn), next(next), interval(interval) {}
      repeated(callback &&fn, duration &&interval) : fn(fn), next(clock::now() + interval), interval(interval) {}
      // NOTE: I'm not making all the possible variations of this
      repeated(const callback &fn, const duration &interval, const time_point &next)
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
      return std::visit(overloaded{
                            [](oneshot const &, oneshot const &) { return std::strong_ordering::equal; },
                            [](oneshot const &, auto const &) { return std::strong_ordering::less; },
                            [](auto const &, oneshot const &) { return std::strong_ordering::greater; },
                            [](repeated const &lhs, repeated const &rhs) { return lhs.next <=> rhs.next; },
                            [](timed_oneshot const &lhs, timed_oneshot const &rhs) { return lhs.time <=> rhs.time; },
                            [](repeated const &lhs, timed_oneshot const &rhs) { return lhs.next <=> rhs.time; },
                            [](timed_oneshot const &lhs, repeated const &rhs) { return lhs.time <=> rhs.next; },
                        },
                        this->_v, other._v);
    }
  };

  std::priority_queue<task, std::vector<task>, std::greater<task>> _tasks;

  template <std::derived_from<Event>> class EventQueue;
  class VEventQueue {
  public:
    virtual ~VEventQueue() {}

    virtual bool empty() const = 0;

    // These are the only things we need to do without knowing the type of the
    // event.
    virtual bool try_remove_callback(callback_id id) = 0;
    virtual void flush(Executor &executor = DIRECT_EXECUTOR) = 0;

    template <std::derived_from<Event> E> EventQueue<E> *into_queue_of() {
#ifndef NDEBUG
      // NOTE: This is here only for debugging purposes.
      //       Since we always know what our desired type is and we can just
      //       static_cast directly to it.
      if (dynamic_cast<EventQueue<E> *>(this) == nullptr) {
        fmt::print(error, "Invalid into_queue_of<E> call (this: {}, E: {})", typeid(*this).name(), typeid(E).name());
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

  std::atomic<callback_id> _current_event_callback_id = std::numeric_limits<callback_id>::min();

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

    // FIXME: Rework this with parallel execution in mind
    std::unordered_set<int> _current_callbacks;
    std::map<uint32_t, std::function<void(const E &)>> _callbacks;

    virtual std::optional<E> _pop_event_unsync() = 0;

  protected:
    mutable std::recursive_mutex _mutex;

  public:
    template <typename Callback>
      requires std::constructible_from<std::function<void(const E &)>, Callback>
    void emplace_callback(callback_id id, Callback cb) {
      std::unique_lock lock(_mutex);
      _callbacks.emplace(id, std::move(cb));
    }

    bool try_remove_callback(callback_id id) override {
      std::unique_lock lock(_mutex);
      if (auto it = _current_callbacks.find(id); it != _current_callbacks.end()) {
        _current_callbacks.erase(it);
        return true;
      } else
        return _callbacks.erase(id) != 0;
    }

    void fire_one(const E &event) {
      _mutex.lock();
      for (auto &[_, callback] : _callbacks) {
        _mutex.unlock();

        fire_base_event(event);
        callback(event);
        _mutex.lock();
      }
      _mutex.unlock();
    }

    void flush(Executor &executor = DIRECT_EXECUTOR) override {
      std::unique_lock lg(_mutex);

      while (true) {
        auto maybe_event = _pop_event_unsync();
        if (!maybe_event.has_value())
          break;
        auto &event = maybe_event.value();

        {
          ScopedExecutor scoped_executor(executor);

          for (auto it = _callbacks.begin(); it != _callbacks.end(); ++it) {
            auto &[id, callback] = *it;
            _current_callbacks.emplace(id);

            scoped_executor.execute([&] {
              fire_base_event(event);
              callback(event);
            });
          }

          if (!executor.blocking()) {
            lg.unlock();
            scoped_executor.close();
            lg.lock();
          }
        }

        for (auto it = _callbacks.begin(); it != _callbacks.end();)
          if (!_current_callbacks.contains(it->first))
            it = _callbacks.erase(it);
          else
            ++it;
      }
    }
  };

  template <std::derived_from<Event> E> class VectorEventQueue final : public EventQueue<E> {
    std::queue<E> _queued_events;

    std::optional<E> _pop_event_unsync() override {
      if (_queued_events.empty())
        return std::nullopt;
      E event = _queued_events.front();
      _queued_events.pop();
      return event;
    }

  public:
    bool empty() const override {
      std::unique_lock lg(this->_mutex);
      return _queued_events.empty();
    }

    template <typename... Args> void emplace(Args &&...args) {
      std::unique_lock lg(this->_mutex);
      _queued_events.emplace(std::forward<Args>(args)...);
    }
  };

  // A ring buffer event queue that will not allocate on insertion.
  //
  // Allocates 17KB of storage, enough for 128 signals.
  // Signals will be dropped if buffer capacity is exceeded.
  class SignalEventQueue final : public EventQueue<SignalEvent> {
    std::allocator<SignalEvent> _allocator;
    size_t _capacity = 128;
    SignalEvent *_buffer = _allocator.allocate(_capacity);
    SignalEvent *_read = _buffer;
    SignalEvent *_write = _buffer;
    bool _full{false}, _overflowed{false};

    std::optional<SignalEvent> _pop_event_unsync() override {
      if(_overflowed) {
        warn << "Signal event queue overflowed, some events have been dropped!\n";
        _overflowed = false;
      }

      if (_read == _write && !_full)
        return std::nullopt;

      SignalEvent value = *_read++;
      if (_read == _buffer + _capacity)
        _read = _buffer;
      _full = false;
      return value;
    }

  public:
    ~SignalEventQueue() { _allocator.deallocate(_buffer, _capacity); }

    bool empty() const override {
      std::unique_lock lg(_mutex);
      return _read == _write && !_full;
    }

    template <typename... Args> void emplace(Args &&...args) {
      std::unique_lock lg(_mutex);
      if (_full) {
        _overflowed = true;
        return;
      }
      *_write++ = SignalEvent(std::forward<Args>(args)...);
      if (_write == _buffer + _capacity)
        _write = _buffer;
      if (_write == _read)
        _full = true;
    }
  };

  template <typename E>
  using EventQueueFor = std::conditional_t<std::same_as<E, SignalEvent>, SignalEventQueue, VectorEventQueue<E>>;

  bool _stopped{false};
  std::vector<std::exception_ptr> _uncaught_exceptions;
  std::unique_ptr<Executor> _executor = std::make_unique<DirectExecutor>();

  void _queue_uncaught_exception(std::exception_ptr &&ptr) { _uncaught_exceptions.emplace_back(std::move(ptr)); }

  EventLoop();
  ~EventLoop();

  bool check_stop();

public:
  enum class ExceptionAction { STOP, IGNORE };
  using ExceptionHandler = std::function<ExceptionAction(std::exception &)>;

private:
  std::optional<ExceptionHandler> _exception_handler;

public:
  EventLoop(const EventLoop &) = delete;
  EventLoop &operator=(const EventLoop &) = delete;
  EventLoop(EventLoop &&) = delete;
  EventLoop &operator=(EventLoop &&) = delete;

  inline static EventLoop &instance() {
    static EventLoop instance;
    return instance;
  }

  inline void reset() {
    _event_queues.clear();
    _tasks = decltype(_tasks)();
    _uncaught_exceptions.clear();
    _stopped = false;
  }

  // NOT THREAD SAFE
  inline void set_executor(std::unique_ptr<Executor> &&new_executor) { _executor = std::move(new_executor); }
  // NOT THREAD SAFE
  inline void set_exception_handler(ExceptionHandler &&handler) { _exception_handler = handler; }

  void run();
  void pump();
  void stop();

  template <std::derived_from<Event> E, typename Callback>
    requires std::constructible_from<std::function<void(const E &)>, Callback>
  callback_id on(Callback cb) {
    if (auto it = _event_queues.lower_bound(typeid(E)); it == _event_queues.end() || it->first != typeid(E)) {
      auto new_queue = std::make_unique<EventQueueFor<E>>();
      auto id = _current_event_callback_id.fetch_add(1);
      new_queue->emplace_callback(id, std::move(cb));
      _event_queues.emplace_hint(it, typeid(E), std::move(new_queue));
      return id;
    } else {
      auto id = _current_event_callback_id.fetch_add(1);
      auto queue = it->second->into_queue_of<E>();
      queue->emplace_callback(id, cb);
      return id;
    }
  }

  template <std::derived_from<Event> E> void fire_event(E const &event) {
    if (auto it = _event_queues.find(typeid(E)); it != _event_queues.end()) {
      auto queue = it->second->into_queue_of<E>();
      static_cast<EventQueueFor<E> *>(queue)->emplace(event);
    }
  }

  template <std::derived_from<Event> E> void fire_event(E &&event) {
    if (auto it = _event_queues.find(typeid(E)); it != _event_queues.end()) {
      auto queue = it->second->into_queue_of<E>();
      static_cast<EventQueueFor<E> *>(queue)->emplace(std::move(event));
    }
  }

  bool off(callback_id id) {
    for (auto &[_, queue] : _event_queues)
      if (queue->try_remove_callback(id))
        return true;
    return false;
  }
  template <std::derived_from<Event> E> bool off(callback_id id) {
    if (auto basic_queue = _event_queues.find(typeid(E)); basic_queue != _event_queues.end()) {
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
        if (condvar.wait_for(lock, std::chrono::milliseconds(5)) == std::cv_status::no_timeout || done)
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
