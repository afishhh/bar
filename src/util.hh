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
std::string_view trim_left(std::string_view str,
                           std::string_view ws = "\t\n\r ");
std::string_view trim_right(std::string_view str,
                            std::string_view ws = "\t\n\r ");
std::string_view trim(std::string_view str, std::string_view ws = "\t\n\r ");

std::string quote(std::string_view, char quote = '"', char escape = '\\');

namespace _private {
inline std::size_t concatenate_size_helper(char const *cstr) {
  return std::strlen(cstr);
}

template <typename T>
inline std::size_t concatenate_size_helper(T const &value) {
  using std::size;
  return size(value);
}
} // namespace _private

// clang-format off
template <typename... Args>
requires (requires {
    { std::string() += std::declval<Args>() };
    { _private::concatenate_size_helper(std::declval<Args>()) } -> std::same_as<std::size_t>;
} && ...)
void concatenate_into(std::string &out, Args... args) {
  size_t size = (... + _private::concatenate_size_helper(args));
  out.reserve(size);
  (out += ... += args);
}
// clang-format on

template <typename... Args> std::string concatenate(Args... args) {
  std::string result;
  concatenate_into(result, args...);
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
inline double map_range(double number, double in_start, double in_end,
                        double out_start, double out_end) {
  // https://stackoverflow.com/a/5732390
  return (number - in_start) * (out_end - out_start) / (in_end - in_start) +
         out_start;
}
