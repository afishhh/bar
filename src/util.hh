#pragma once

#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <tuple>

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

/** @brief Convert HSL color values to RGB.
 *
 * @param h Hue        [0, 1]
 * @param s Saturation [0, 1]
 * @param l Lightness  [0, 1]
 * @returns Tuple of RGB components [0, 255]
 */
std::tuple<unsigned char, unsigned char, unsigned char>
hsl_to_rgb(double h, double s, double l);

/** @brief Converts seprate RGB components into a single 32-bit integer.
 *
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @return 32-bit integer representing the color.
 */
inline unsigned long rgb_to_long(unsigned char r, unsigned char g,
                                 unsigned char b) {
  return (r << 16) | (g << 8) | b;
}

/** @brief Converts separate RGB components into a single 32-bit integer.
 *
 * @param rgb Tuple of RGB components.
 * @return 32-bit integer representing the color.
 */
inline unsigned long
rgb_to_long(std::tuple<unsigned char, unsigned char, unsigned char> rgb) {
  return rgb_to_long(std::get<0>(rgb), std::get<1>(rgb), std::get<2>(rgb));
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
