#include <iostream>
#include <string_view>
#include <unistd.h>

#include "log.hh"
#include "util.hh"

std::string mk_prefix(std::string_view color, std::string_view prefix) {
  static bool is_tty = isatty(STDOUT_FILENO);
  if (is_tty) {
    return concatenate(color, prefix, "\033[0m");
  } else {
    return std::string(prefix);
  }
}

Logger info(std::cerr, mk_prefix("\033[1;94m", "[info] "));
Logger warn(std::cerr, mk_prefix("\033[1;93m", "[warn] "));
Logger error(std::cerr, mk_prefix("\033[1;91m", "[error] "));
Logger debug(std::cerr, mk_prefix("\033[1;95m", "[debug] "));
