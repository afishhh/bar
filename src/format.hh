#if !defined(HAVE_STD_FORMAT) || !defined(HAVE_STD_FORMAT_TO) ||               \
    !defined(HAVE_STD_PRINT)
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>

namespace std {
#ifndef HAVE_STD_FORMAT
using fmt::format;
#endif
#ifndef HAVE_STD_FORMAT_TO
using fmt::format_to;
#endif
#ifndef HAVE_STD_PRINT
using fmt::print;
#endif
} // namespace std
#else
#include <format>
#endif
