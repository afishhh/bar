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

public:
  ThreadPoolExecutor(unsigned threads) {
    for (unsigned i = 0; i < threads; ++i)
      _workers.emplace_back([this](std::stop_token token) {
        std::unique_lock lg(_mutex);
        while (_condvar.wait(_mutex, token, [this] { return !_queue.empty(); })) {
          auto job = std::move(_queue.front());
          _queue.pop();

          lg.unlock();
          job();
          lg.lock();
        }
      });
  }

  bool blocking() const override { return false; };
  void execute(std::function<void()> &&job) override {
    std::unique_lock lg(_mutex);
    _queue.push(std::move(job));
    _condvar.notify_one();
  }
};

class ScopedExecutor final : public Executor {
  // FIXME: Is acquire/release memory ordering appriopriate here?
  constexpr static unsigned WAITING = 1U << 31;
  std::atomic_uint _state = 0;
  std::binary_semaphore _waiter = std::binary_semaphore(0);
  Executor &_executor;

public:
  ScopedExecutor(Executor &executor) : _executor(executor) {}
  ~ScopedExecutor() noexcept(false) { close(); }

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
        job();

        unsigned state = _state.fetch_sub(1, std::memory_order_acq_rel);
        // debug << "SE " << (void *)this << " REL: " << val << '\n';
        if (state == (WAITING + 1))
          _waiter.release();
      });
    }
  }
};
