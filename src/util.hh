#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <tuple>
#include <type_traits>

template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

std::string to_sensible_unit(size_t bytes, size_t precision = 2);
std::string_view trim_left(std::string_view str,
                           std::string_view ws = "\t\n\r ");
std::string_view trim_right(std::string_view str,
                            std::string_view ws = "\t\n\r ");
std::string_view trim(std::string_view str, std::string_view ws = "\t\n\r ");

std::string quote(std::string_view, char quote = '"', char escape = '\\');

// clang-format off
template <std::convertible_to<std::string_view>... Args>
requires (requires {
    { std::string() += Args() };
} && ...)
std::string concatenate(Args... args) {
  size_t size = (... + std::string_view(args).size());
  std::string result;
  result.reserve(size);
  (result += ... += args);
  return result;
}
// clang-format on

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
