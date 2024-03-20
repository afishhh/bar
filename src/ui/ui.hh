#pragma once

#include "../executor.hh"

namespace ui {

class UiThread final {
  std::mutex _mutex;
  std::jthread _thread;
  std::atomic<std::function<void()> const *> _work;

  void _worker_main(std::stop_token token) {
    debug << "worker main called on thread " << std::hex << std::this_thread::get_id() << ' ' << _thread.get_id()
          << "\n";

    while (true) {
      _work.wait(nullptr, std::memory_order::acquire);
      if (token.stop_requested())
        return;

      try {
        (*_work)();
      } catch (std::exception &ex) {
        error << "Exception in UI thread: " << ex.what() << "\n";
      } catch (...) {
        error << "Something has been thrown in the UI thread!\n";
      }

      _work.store(nullptr, std::memory_order_release);
      _work.notify_all();
    }
  }

public:
  UiThread() : _thread([this](std::stop_token t) { _worker_main(t); }) {}

  ~UiThread() {
    _thread.request_stop();
    _work.store((std::function<void()> *)1, std::memory_order::release);
  }

  BAR_NON_COPYABLE(UiThread);
  BAR_NON_MOVEABLE(UiThread);

  void execute_blocking(std::function<void()> const &job) {
    if (std::this_thread::get_id() == _thread.get_id())
      return;

    while (true) {
      std::function<void()> const *current = nullptr;
      if (_work.compare_exchange_strong(current, &job, std::memory_order_acq_rel)) {
        _work.notify_all();
        return _work.wait(&job, std::memory_order_acquire);
      } else if (current != nullptr)
        _work.wait(current, std::memory_order_acq_rel);
    }
  }
};

extern UiThread THREAD;

} // namespace ui
