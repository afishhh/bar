#pragma once

#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <future>
#include <iomanip>
#include <list>
#include <optional>
#include <span>
#include <sstream>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "ui/gl.hh"

#define BAR_NON_COPYABLE(Self)                                                                                         \
  Self(Self const &) = delete;                                                                                         \
  Self &operator=(Self const &) = delete

#define BAR_NON_MOVEABLE(Self)                                                                                         \
  Self(Self &&) = delete;                                                                                              \
  Self &operator=(Self &&) = delete

class glfw_error : public std::runtime_error {
  int _code;

public:
  glfw_error(std::string message, int code) : std::runtime_error(message), _code(code) {}
};

inline void glfw_throw_error(char const *function = nullptr) {
  char const *description;
  int code = glfwGetError(&description);
  if (code != GLFW_NO_ERROR) {
    if (function)
      throw glfw_error(std::format("{}: {}", function, description), code);
    else
      throw glfw_error(description, code);
  }
}

namespace _private {

template <typename T> T check_glfw_return(T, char const *function = nullptr) = delete;

template <typename T> T *check_glfw_return(T *ptr, char const *function = nullptr) {
  if (ptr == nullptr)
    glfw_throw_error(function);
  return ptr;
}

} // namespace _private

#define BAR_GLFW_CHECK(value) _private::check_glfw_return(value)
#define BAR_GLFW_CALL(name, ...) _private::check_glfw_return(glfw##name(__VA_ARGS__), #name)

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

std::string to_sensible_unit(size_t bytes, size_t precision = 2);
std::string_view trim_left(std::string_view str, std::string_view ws = "\t\n\r ");
std::string_view trim_right(std::string_view str, std::string_view ws = "\t\n\r ");
std::string_view trim(std::string_view str, std::string_view ws = "\t\n\r ");

std::string quote(std::string_view, char quote = '"', char escape = '\\');

namespace _private {
template <typename T> struct concatenate_sizer {
  std::size_t operator()(T const &value) {
    using std::size;
    return size(value);
  }
};

template <> struct concatenate_sizer<char const *> {
  std::size_t operator()(char const *cstr) { return std::strlen(cstr); }
};
} // namespace _private

template <typename... Args>
  requires(requires {
    { std::string() += std::declval<Args>() };
    { _private::concatenate_sizer<Args>()(std::declval<Args>()) } -> std::same_as<std::size_t>;
  } && ...)
void concatenate_into(std::string &out, Args &&...args) {
  size_t size = (... + _private::concatenate_sizer<Args>()(std::forward<Args>(args)));
  out.reserve(size);
  (out += ... += args);
}

template <typename... Args> inline std::string concatenate(Args &&...args) {
  std::string result;
  concatenate_into(result, std::forward<Args>(args)...);
  return result;
}

/**
 * @brief Maps number from one range to another.
 *
 * @param {double} number    Number to map.
 * @param {double} in_start  Start of the input range.
 * @param {double} in_end    End of the input range.
 * @param {double} out_start Start of the output range.
 * @param {double} out_end   End of the output range.
 * @returns {double}         Mapped number.
 */
inline double map_range(double number, double in_start, double in_end, double out_start, double out_end) {
  // https://stackoverflow.com/a/5732390
  return (number - in_start) * (out_end - out_start) / (in_end - in_start) + out_start;
}

namespace _private {

template <std::invocable Fn> class defer {
private:
  Fn _func;

public:
  defer(Fn func) : _func(func) {}
  ~defer() { _func(); }
};

} // namespace _private

#define DEFER3(fn, c) auto _defer__##c = _private::defer((fn))
#define DEFER2(fn, c) DEFER3((fn), c)
#define DEFER(fn) DEFER2((fn), __COUNTER__)

template <typename K, typename V, std::size_t MaxSize>
  requires(!std::is_reference_v<K>) && requires(K k) {
    { std::hash<K>()(k) } -> std::same_as<size_t>;
  }
class LRUMap {
  constexpr static size_t MapSize = MaxSize + MaxSize / 2;

  struct Node {
    K key;
    V value;
    std::list<size_t>::iterator activity_list_entry;
  };

  std::span<std::optional<Node>> _values;
  std::list<size_t> _list;

  template <typename KeyComparable> size_t _find(size_t hash, KeyComparable const &key) const {
    size_t index = hash % MapSize;
    while (_values[index].has_value()) {
      if (_values[index]->key == key)
        return index;

      index = (index + 1) % MapSize;
    }
    return index;
  }

public:
  LRUMap() { _values = std::span(new std::optional<Node>[MapSize], MapSize); }
  ~LRUMap() { delete[] _values.data(); }

  V &insert(K &&key, V &&value) {
    if (_list.size() >= MaxSize) {
      _values[_list.back()].reset();
      _list.pop_back();
    }

    size_t idx = _find(std::hash<K>()(key), key);
    assert(!_values[idx].has_value());

    auto iterator = _list.emplace(_list.cbegin(), idx);
    return _values[idx].emplace(std::move(key), std::move(value), iterator).value;
  }

  template <typename KeyComparable> V *get(KeyComparable const &key) {
    size_t idx = _find(std::hash<KeyComparable>()(key), key);

    if (_values[idx].has_value()) {
      _list.splice(_list.begin(), _list, _values[idx]->activity_list_entry);
      return &_values[idx]->value;
    } else
      return nullptr;
  }
};
