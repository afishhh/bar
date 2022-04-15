#pragma once

#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <tuple>

static std::string to_sensible_unit(size_t bytes, size_t precision = 2) {
  static const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
  size_t unit = 1;
  double result = bytes;
  while (result > 1024) {
    result /= 1024;
    unit++;
  }
  std::ostringstream ostr;
  ostr << std::setprecision(precision) << std::fixed << result;
  return ostr.str() +
         units[std::min(unit, sizeof units / sizeof(units[0])) - 1];
}

static std::string_view trim_left(const std::string_view &str,
                                  const std::string_view &ws = " \r\n\t") {
  const auto it = std::find_if_not(str.begin(), str.end(), [&ws](char c) {
    return ws.find(c) != std::string_view::npos;
  });
  return str.substr(it - str.begin());
}
static std::string_view trim_right(const std::string_view &str,
                                   const std::string_view &ws = " \r\n\t") {
  const auto it = std::find_if_not(str.rbegin(), str.rend(), [&ws](char c) {
                    return ws.find(c) != std::string_view::npos;
                  }).base();
  return str.substr(0, it - str.begin());
}
static std::string_view trim(const std::string_view &str,
                             const std::string_view &ws = " \r\n\t") {
  return trim_left(trim_right(str, ws), ws);
}

/** @brief Convert HSL color values to RGB.
 *
 * @param h Hue        [0, 1]
 * @param s Saturation [0, 1]
 * @param l Lightness  [0, 1]
 * @returns Tuple of RGB components [0, 255]
 */
inline std::tuple<unsigned char, unsigned char, unsigned char>
hsl_to_rgb(double h, double s, double l) {
  // https://stackoverflow.com/a/9493060
  double r, g, b;
  if (s == 0) {
    return {l, l, l};
  } else {
    auto hue_to_rgb = [](double p, double q, double t) {
      if (t < 0)
        t += 1;
      if (t > 1)
        t -= 1;
      if (t < 1. / 6)
        return p + (q - p) * 6 * t;
      if (t < 1. / 2)
        return q;
      if (t < 2. / 3)
        return p + (q - p) * (2. / 3 - t) * 6;
      return p;
    };

    auto q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    auto p = 2 * l - q;
    r = hue_to_rgb(p, q, h + 1. / 3);
    g = hue_to_rgb(p, q, h);
    b = hue_to_rgb(p, q, h - 1. / 3);
  }
  return {r * 255, g * 255, b * 255};
}

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
