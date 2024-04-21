

#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>

class CancellationException : std::exception {
  const char *what() const noexcept(true) override { return "Thread has been cancelled"; }
};

class CancellationToken {
  std::mutex _mutex;
  std::condition_variable _cv;
  bool _cancelled = false;

  friend class CancellableThread;

  void _cancel() {
    {
      std::unique_lock lg(_mutex);
      _cancelled = true;
    }
    _cv.notify_all();
  }

public:
  CancellationToken() {}
  CancellationToken(CancellationToken const &) = delete;
  CancellationToken(CancellationToken &&) = delete;
  CancellationToken &operator=(CancellationToken const &) = delete;
  CancellationToken &operator=(CancellationToken &&) = delete;

  void wait_for(auto until) {
    std::unique_lock lg(_mutex);
    if (_cv.wait_for(lg, until, [&] { return _cancelled; }))
      throw CancellationException();
  }

  void wait_until(auto until) {
    std::unique_lock lg(_mutex);
    if (_cv.wait_until(lg, until, [&] { return _cancelled; }))
      throw CancellationException();
  }

  bool cancelled() {
    std::unique_lock lg(_mutex);
    return _cancelled;
  }
};

class CancellationSource {
  virtual void cancel() = 0;
};

class CancellableThread : public CancellationSource {
  std::shared_ptr<CancellationToken> _token;
  std::thread _thread;

public:
  CancellableThread() : _token(nullptr), _thread() {}
  template <typename Fn>
  CancellableThread(Fn &&fn)
      : _token(std::make_shared<CancellationToken>()), _thread([token = _token, fn = std::forward<Fn>(fn)] {
          try {
            fn(static_cast<CancellationToken &>(*token));
          } catch (CancellationException &) {
          }
        }) {}

  CancellableThread &operator=(CancellableThread &&other) {
    this->_thread = std::move(other._thread);
    this->_token = std::move(other._token);
    return *this;
  }

  bool joinable() const { return _thread.joinable(); }
  void join() { _thread.join(); }
  void cancel() override {
    if (_token)
      _token->_cancel();
  }

  ~CancellableThread() {
    if (joinable()) {
      cancel();
      join();
    }
  }
};
