#include <regex>
#include <string>

#include "format.hh"
#include "util.hh"

std::string to_sensible_unit(size_t bytes, size_t precision) {
  static const char *units[] = {"B",  "KB", "MB", "GB", "TB",
                                "PB", "EB", "ZB", "YB"};
  size_t unit = 1;
  double result = bytes;
  while (result > 1024) {
    result /= 1024;
    unit++;
  }
  return std::format(
      "{:.{}f}{}", result, precision,
      units[std::min(unit, sizeof units / sizeof(units[0])) - 1]);
}
std::string_view trim_left(std::string_view str, std::string_view ws) {
  const auto it = std::find_if_not(str.begin(), str.end(), [&ws](char c) {
    return ws.find(c) != std::string_view::npos;
  });
  return str.substr(it - str.begin());
}
std::string_view trim_right(std::string_view str, std::string_view ws) {
  const auto it = std::find_if_not(str.rbegin(), str.rend(), [&ws](char c) {
                    return ws.find(c) != std::string_view::npos;
                  }).base();
  return str.substr(0, it - str.begin());
}

std::string_view trim(std::string_view str, std::string_view ws) {
  auto start = str.find_first_not_of(ws);
  if (start == std::string_view::npos)
    return "";
  auto end = str.find_last_not_of(ws);
  return str.substr(start, end - start + 1);
}

std::string quote(std::string_view str, char quote, char escape) {
  std::string s;
  s.reserve(str.size() + 5);
  s.push_back(quote);
  for (auto c : str) {
    if (c == quote)
      s.push_back(escape);
    s.push_back(c);
  }
  s.push_back(quote);
  return s;
}
