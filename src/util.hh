#pragma once

#include <cstddef>
#include <iomanip>
#include <sstream>

static std::string to_sensible_unit(size_t bytes, size_t precision = 2) {
  const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
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

static std::string_view trim_left(const std::string_view &str, const std::string_view& ws = " \r\n\t") {
  const auto it = std::find_if_not(str.begin(), str.end(), [&ws](char c) {
    return ws.find(c) != std::string_view::npos;
  });
  return str.substr(it - str.begin());
}
static std::string_view trim_right(const std::string_view &str, const std::string_view& ws = " \r\n\t") {
  const auto it = std::find_if_not(str.rbegin(), str.rend(), [&ws](char c) {
    return ws.find(c) != std::string_view::npos;
  }).base();
  return str.substr(0, it - str.begin());
}
static std::string_view trim(const std::string_view &str, const std::string_view& ws = " \r\n\t") {
  return trim_left(trim_right(str, ws), ws);
}
