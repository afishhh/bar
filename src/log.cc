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

thread_local Logger info(std::cerr, mk_prefix("\033[1;94m", "[info] "));
thread_local Logger warn(std::cerr, mk_prefix("\033[1;93m", "[warn] "));
thread_local Logger error(std::cerr, mk_prefix("\033[1;91m", "[error] "));
thread_local Logger debug(std::cerr, mk_prefix("\033[1;95m", "[debug] "));
