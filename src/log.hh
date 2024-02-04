#pragma once

#include <ostream>
#include <streambuf>
#include <string>
#include <string_view>

class Logger : public std::ostream, private std::streambuf {
  std::ostream &_out;
  std::string _buf;
  std::string _prefix;

public:
  Logger(std::ostream &out, const std::string &prefix)
      : std::ostream(this), _out(out), _prefix(prefix) {}
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

extern thread_local Logger _bar_debug_stream;
extern thread_local Logger _bar_info_stream;
extern thread_local Logger _bar_warn_stream;
extern thread_local Logger _bar_error_stream;

#define debug _bar_debug_stream
#define info _bar_info_stream
#define warn _bar_warn_stream
#define error _bar_error_stream
