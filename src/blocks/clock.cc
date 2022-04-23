#include <bits/types/time_t.h>
#include <chrono>
#include <ctime>
#include <string>

#include "clock.hh"

void ClockBlock::animate(EventLoop::duration) {
  auto now = std::chrono::system_clock::now();
  time_t tt = std::chrono::system_clock::to_time_t(now);
  struct tm *tm = localtime(&tt);
  
  std::strftime(_text.data(), _text.size(), "%Y %B %d %H:%M:%S", tm);
}

size_t ClockBlock::draw(Draw &draw, std::chrono::duration<double>) {
  return draw.text(0, draw.vcenter(), _text.data());
}
