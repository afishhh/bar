#include <iostream>

#include "log.hh"

Logger info(std::cerr, "\033[1;94m[info]\033[0m ");
Logger warn(std::cerr, "\033[1;93m[warn]\033[0m ");
Logger error(std::cerr, "\033[1;91m[error]\033[0m ");
