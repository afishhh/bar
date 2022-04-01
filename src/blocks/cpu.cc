#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstring>

#include "cpu.hh"

CpuBlock::AllTimes CpuBlock::read_cpu_times() {
  std::ifstream stat("/proc/stat");

  AllTimes all;
  std::string name;
  stat >> name;
  stat >> all.total.user;
  stat >> all.total.nice;
  stat >> all.total.system;
  stat >> all.total.idle;
  stat >> all.total.iowait;
  stat >> all.total.irq;
  stat >> all.total.softirq;
  stat >> all.total.steal;
  stat >> all.total.guest;
  stat >> all.total.guest_nice;

  while (stat >> name && name.starts_with("cpu")) {
    Times t;
    stat >> t.user;
    stat >> t.nice;
    stat >> t.system;
    stat >> t.idle;
    stat >> t.iowait;
    stat >> t.irq;
    stat >> t.softirq;
    stat >> t.steal;
    stat >> t.guest;
    stat >> t.guest_nice;
    all.percore.push_back(std::move(t));
  }

  return all;
}

void CpuBlock::update() {
  if (initialised) {
    this->_previous = this->_current;
    this->_current = this->read_cpu_times();
  } else {
    this->_current = this->read_cpu_times();
    // Zero out this->_previous
    std::memset(&this->_previous.total, 0, sizeof(this->_previous.total));
    this->_previous.percore.resize(this->_current.percore.size());
    for (auto& t : this->_previous.percore) {
      std::memset(&t, 0, sizeof(t));
    }

    this->initialised = true;
  }

  _diff = this->_current - this->_previous;
}

size_t CpuBlock::draw(Draw &draw) {
  size_t y = draw.vcenter();
  size_t x = 0;
  auto percentage = 100.0 * _diff.total.busy() / _diff.total.total();

  std::stringstream ss;
  ss << std::setw(5) << std::right << std::fixed << std::setprecision(1)
     << std::fixed << percentage << "%";

  x += draw.text(x, y, "CPU: ");
  x += draw.text(x, y, ss.str());

  x += 5;

  for (size_t i = 0; i < _diff.percore.size(); ++i) {
    auto left = x;
    auto right = x += 8;
    auto top = 0;
    auto bottom = draw.height() - 1;

    draw.line(left, top, right, top);
    draw.line(left, bottom, right, bottom);
    draw.line(left, top, left, bottom);
    draw.line(right, top, right, bottom);

    auto maxfill = bottom - top - 1;
    double percentage =
        (double)_diff.percore[i].busy() / _diff.percore[i].total();
    size_t fill = maxfill * percentage;

    unsigned long color;
    if (percentage > 0.80)
      color = 0xFF0000;
    else if (percentage > 0.60)
      color = 0xFFA500;
    else if (percentage > 0.40)
      color = 0xFFFF00;
    else if (percentage > 0.20)
      color = 0x00FF00;
    else
      color = 0x00FFFF;

    draw.rect(left + 1, top + (maxfill - fill) + 1, right - left - 1, fill,
              color);
  }

  return x;
}
