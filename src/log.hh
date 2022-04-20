#include <ostream>
#include <streambuf>
#include <string>
#include <string_view>

class Logger : public std::ostream, private std::streambuf {
  std::ostream &_out;
  std::string _buf;
  std::string_view _prefix;

public:
  Logger(std::ostream &out, std::string_view prefix)
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

extern Logger info;
extern Logger warn;
extern Logger error;
