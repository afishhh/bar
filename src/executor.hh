#pragma once

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <exception>
#include <functional>
#include <latch>
#include <mutex>
#include <optional>
#include <queue>
#include <semaphore>
#include <stop_token>
#include <thread>
#include <vector>

#include "log.hh"

class Executor {
public:
  virtual ~Executor() = default;
  virtual bool blocking() const = 0;
  virtual void execute(std::function<void()> &&) = 0;
  virtual void execute(std::function<void()> const &f) { return execute(std::function<void()>(f)); }
};

class DirectExecutor final : public Executor {
public:
  bool blocking() const override { return true; };
  void execute(std::function<void()> &&job) override { job(); }
};

static DirectExecutor DIRECT_EXECUTOR = DirectExecutor();

class ThreadPoolExecutor final : public Executor {
  std::mutex _mutex;
  std::condition_variable_any _condvar;
  std::queue<std::function<void()>> _queue;
  std::vector<std::jthread> _workers;
  std::atomic_uint _available_workers;
  unsigned _max_threads;

  void start_worker() {
    _workers.emplace_back([this](std::stop_token token) {
      std::unique_lock lg(_mutex);
      _available_workers.fetch_add(1, std::memory_order_relaxed);
      while (_condvar.wait(_mutex, token, [this] { return !_queue.empty(); })) {
        _available_workers.fetch_sub(1, std::memory_order_relaxed);
        auto job = std::move(_queue.front());
        _queue.pop();

        lg.unlock();
        job();
        lg.lock();
      }
    });
  }

public:
  ThreadPoolExecutor(unsigned max_threads) : _max_threads(max_threads) {
    if (max_threads > 0)
      start_worker();
  }

  bool blocking() const override { return false; };
  void execute(std::function<void()> &&job) override {
    std::unique_lock lg(_mutex);
    _queue.push(std::move(job));
    _condvar.notify_one();
    if (_available_workers.load(std::memory_order_relaxed) == 0 && _workers.size() < _max_threads)
      start_worker();
  }
};

class PinnedThreadExecutor final : public Executor {
  std::jthread _thread;
  std::queue<std::function<void()>> _lazy;
  std::function<void()> _now;
  std::mutex _mutex;
  std::condition_variable_any _condvar;

  void _worker_main(std::stop_token token) {
    std::unique_lock lg(_mutex);

    auto run = [&lg](auto work) {
      lg.unlock();
      try {
        work();
      } catch (std::exception &ex) {
        error << "Pinned executor caught an exception: " << ex.what() << "\n";
      } catch (...) {
        error << "Pinned executor caught an something!\n";
      }
      lg.lock();
    };

    while (true) {
      _condvar.wait(lg, token, [&] { return _now != nullptr || !_lazy.empty() || token.stop_requested(); });
      if (token.stop_requested())
        return;

      if (_now != nullptr) {
        run(_now);
        _now = nullptr;
        _condvar.notify_one();
      } else {
        auto work = std::move(_lazy.front());
        _lazy.pop();
      }
    }
  }

public:
  PinnedThreadExecutor() : _thread([this](std::stop_token t) { _worker_main(t); }) {}

  bool blocking() const override { return false; }
  void execute(std::function<void()> &&job) override {
    std::unique_lock lg(_mutex);
    _lazy.emplace(job);
    _condvar.notify_one();
  }

  void execute_now(std::function<void()> &&job) {
    std::unique_lock lg(_mutex);
    _now = std::move(job);
    _condvar.notify_one();
    _condvar.wait(_mutex, [this] { return _now == nullptr; });
  }
};

class ScopedExecutor final : public Executor {
  // FIXME: Is acquire/release memory ordering appriopriate here?
  constexpr static unsigned WAITING = 1U << 31;
  std::atomic_uint _state = 0;
  std::binary_semaphore _waiter = std::binary_semaphore(0);
  Executor &_executor;

  void _on_async_job_done() {
    unsigned state = _state.fetch_sub(1, std::memory_order_acq_rel);
    // debug << "SE " << (void *)this << " REL: " << (state bitand (compl WAITING))
    //       << " W?: " << (bool)(state bitand WAITING) << '\n';
    if (state == (WAITING + 1))
      _waiter.release();
  }

public:
  ScopedExecutor(Executor &executor) : _executor(executor) {}
  ~ScopedExecutor() { close(); }

  void close() {
    if (!_executor.blocking()) {
      unsigned state = _state.fetch_or(WAITING, std::memory_order_acq_rel);
      if (state & WAITING)
        return;
      else if (state > 0) {
        // debug << "SE " << (void *)this << " WAITING\n";
        _waiter.acquire();
        assert(_state.load(std::memory_order_acquire) == WAITING);
      }
    }
  }

  bool blocking() const override { return _executor.blocking(); }
  void execute(std::function<void()> &&job) override {
    if (_executor.blocking())
      _executor.execute([&] { job(); });
    else {
      unsigned state = _state.fetch_add(1, std::memory_order_acq_rel);
      // debug << "SE " << (void *)this << " AQ: " << val << '\n';
      if (state == WAITING)
        throw std::logic_error("ScopedExecutor::execute called after the executor was closed");

      _executor.execute([this, job = std::move(job)] {
        try {
          job();
        } catch (...) {
          _on_async_job_done();
          throw;
        }

        _on_async_job_done();
      });
    }
  }
};
