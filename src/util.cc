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
  std::ostringstream ostr;
  ostr << std::setprecision(precision) << std::fixed << result;
  return ostr.str() +
         units[std::min(unit, sizeof units / sizeof(units[0])) - 1];
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

std::tuple<unsigned char, unsigned char, unsigned char>
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
