#pragma once

#include <cstddef>
#include <iomanip>
#include <sstream>

static std::string to_sensible_unit(size_t bytes) {
  const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
  size_t unit = 1;
  double result = bytes;
  while (result > 1024) {
    result /= 1024;
    unit++;
  }
  std::ostringstream ostr;
  ostr << std::setprecision(2) << std::fixed << result;
  return ostr.str() +
         units[std::min(unit, sizeof units / sizeof(units[0])) - 1];
}
