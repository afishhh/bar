#include <bits/types/time_t.h>
#include <chrono>
#include <string>

#include "clock.hh"

std::string month_to_string(int mon) {
  switch (mon) {
    case 0:
      return "January";
    case 1:
      return "February";
    case 2:
      return "March";
    case 3:
      return "April";
    case 4:
      return "May";
    case 5:
      return "June";
    case 6:
      return "July";
    case 7:
      return "August";
    case 8:
      return "September";
    case 9:
      return "October";
    case 10:
      return "November";
    case 11:
      return "December";
    default:
      return "Invalid Month";
  }
}

size_t ClockBlock::draw(Draw &draw, std::chrono::duration<double>) {
  size_t x = 0;
  auto now = std::chrono::system_clock::now();
  time_t tt = std::chrono::system_clock::to_time_t(now);
  struct tm *tm = localtime(&tt);
  
  auto left_pad = [](std::string s, size_t n) {
    while (s.size() < n) {
      s.insert(0, "0");
    }
    return s;
  };

  x += draw.text(x, draw.vcenter(), std::to_string(tm->tm_year + 1900));
  x += 5;
  x += draw.text(x, draw.vcenter(), month_to_string(tm->tm_mon));
  x += 5;
  x += draw.text(x, draw.vcenter(), std::to_string(tm->tm_mday));
  x += 8;
  x += draw.text(x, draw.vcenter(), left_pad(std::to_string(tm->tm_hour), 2));
  x += draw.text(x, draw.vcenter(), ":");
  x += draw.text(x, draw.vcenter(), left_pad(std::to_string(tm->tm_min), 2));
  x += draw.text(x, draw.vcenter(), ":");
  x += draw.text(x, draw.vcenter(), left_pad(std::to_string(tm->tm_sec), 2));
  return x;
}
