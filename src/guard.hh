#pragma once

#include <cstddef>
#include <iostream>
#include <map>

class guard_like {
  static guard_like *s_head;
  guard_like *_prev = nullptr;
  guard_like *_next = nullptr;

  static void cleanup_all();
  virtual void cleanup() = 0;

public:
  static void maybe_init();

  guard_like();
  ~guard_like();
};

template <typename T> class guard : guard_like {
  T _callback;
  bool _clean = false;

public:
  guard(const guard &) = delete;
  guard &operator=(const guard &) = delete;

  guard(T callback) : _callback(callback) {}
  void cleanup() override {
    if (!_clean) {
      _callback();
      _clean = true;
    }
  }
  ~guard() { cleanup(); }
};
