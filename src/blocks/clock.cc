#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include "../format.hh"
#include "clock.hh"

void ClockBlock::update() {
  auto now = std::chrono::system_clock::now();
  time_t tt = std::chrono::system_clock::to_time_t(now);
  _time = localtime(&tt);
}

size_t ClockBlock::draw(ui::draw &draw, std::chrono::duration<double>) {
  return draw.text(0, draw.vcenter(), std::format("{:%B %d %H:%M:%S}", *_time));
}

std::string_view constexpr WEEKDAYS[] = {"Monday",   "Tuesday", "Wednesday",
                                         "Thursday", "Friday",  "Saturday",
                                         "Sunday"};
std::string_view constexpr MONTHS[] = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December",
};
// FIXME: leap years and shit
unsigned constexpr MONTH_LENGHTS[] = {31, 28, 31, 30, 31, 30,
                                      31, 31, 30, 31, 30, 31};

void ClockBlock::draw_tooltip(ui::draw &draw, std::chrono::duration<double>,
                              unsigned int) const {
  auto const width = 180;
  auto const calendar_cell_size = 24;

  auto title = std::format("{:%B %d %Y}", *_time);
  draw.text((width - draw.textw(title)) / 2, 9, title);

  auto subtitle = std::format("{:%H:%M:%S}", *_time);
  draw.text((width - draw.textw(subtitle)) / 2, 30, subtitle);

  auto const calendar_width = 7 * calendar_cell_size;
  auto const calendar_tmargin = 42;
  auto const calendar_lmargin = (width - calendar_width) / 2;

  for (size_t i = 0; i < 7; ++i) {
    auto t = std::format("{}", WEEKDAYS[i].substr(0, 2));
    draw.text(calendar_lmargin + i * calendar_cell_size +
                  (calendar_cell_size - draw.textw(t)) / 2,
              calendar_tmargin + 12, t);
  }

  auto is_leap_year = [](int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  };

  auto const mdays = (_time->tm_mon == 1)
                         ? (is_leap_year(_time->tm_year + 1900) ? 29 : 28)
                         : MONTH_LENGHTS[_time->tm_mon];
  auto initial_wday = (_time->tm_wday + 7 * 100 - _time->tm_mday) % 7;

  auto pos_for_cell = [](unsigned x, unsigned y) {
    return uvec2{calendar_lmargin + x * calendar_cell_size,
                 calendar_tmargin + 20 + y * calendar_cell_size};
  };

  auto wday = initial_wday;
  for (unsigned i = 1; i <= mdays; i++, wday = (wday + 1) % 7) {
    auto [x, y] = pos_for_cell(wday, (initial_wday + i - 1) / 7);
    auto t = std::to_string(i);

    if (i == (unsigned)_time->tm_mday)
      draw.fcircle(x + (calendar_cell_size - 21) / 2, y, 21, color(0x00AA00));

    draw.text(x + (calendar_cell_size - draw.textw(t)) / 2, 12 + y, t);
  }

  draw.hrect(0, 0, width, 0, 0);
}
