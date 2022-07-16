#include <chrono>
#include <ctime>
#include <iterator>
#include <string>

#include "../format.hh"
#include "clock.hh"

void ClockBlock::animate(EventLoop::duration) {
  auto now = std::chrono::system_clock::now();
  time_t tt = std::chrono::system_clock::to_time_t(now);
  struct tm *tm = localtime(&tt);

  _text.clear();
  std::format_to(std::back_inserter(_text), "{:%Y %B %d %H:%M:%S}", *tm);
}

size_t ClockBlock::draw(Draw &draw, std::chrono::duration<double>) {
  return draw.text(0, draw.vcenter(), _text.data());
}
