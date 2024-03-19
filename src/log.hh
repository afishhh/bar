#pragma once

#include <ostream>
#include <streambuf>
#include <string>
#include <string_view>

#include <fmt/core.h>
#include <fmt/ostream.h>

class Logger : public std::ostream, private std::streambuf {
  std::ostream &_out;
  std::string _buf;
  std::string _prefix;

public:
  Logger(std::ostream &out, const std::string &prefix) : std::ostream(this), _out(out), _prefix(prefix) {}
  ~Logger() override {
    if (!_buf.empty()) {
      _out << _prefix << _buf << std::flush;
    }
  }

  int overflow(int ch) override {
    if (ch == '\n') {
      _out << _prefix << _buf << '\n';
      _buf.clear();
    } else {
      _buf += ch;
    }
    return ch;
  }
};

namespace bar_logging_streams {
extern thread_local Logger debug;
extern thread_local Logger info;
extern thread_local Logger warn;
extern thread_local Logger error;
} // namespace bar_logging_streams

using namespace bar_logging_streams;
