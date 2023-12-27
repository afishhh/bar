#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <future>
#include <iomanip>
#include <span>
#include <sstream>
#include <string_view>
#include <tuple>
#include <type_traits>

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
