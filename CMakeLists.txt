cmake_minimum_required(VERSION 3.1)

set(PROJECT_NAME bar)
project(${PROJECT_NAME} CXX)

option(DWMIPC "Enable linking with dwmipcpp, enabling the DWM block." OFF)
if (DWMIPC)
  include(FetchContent)
  FetchContent_Declare(
    dwmipcpp
    GIT_REPOSITORY https://github.com/mihirlad55/dwmipcpp
    GIT_TAG        6b6947fd63845c8239f0a895be695bf206eaae6d
  )
  FetchContent_MakeAvailable(dwmipcpp)

  list(APPEND EXTRA_SOURCES
    src/blocks/dwm.cc
  )

  add_definitions(-DHAVE_DWMIPCPP)
endif()

add_executable(
  ${PROJECT_NAME}

  src/main.cc
  src/loop.cc
  src/guard.cc
  src/util.cc
  src/log.cc
  src/xdraw.cc
  src/bufdraw.cc
  src/blocks/memory.cc
  src/blocks/battery.cc
  src/blocks/network.cc
  src/blocks/cpu.cc
  src/blocks/clock.cc
  src/blocks/script.cc
  src/blocks/disk.cc

  ${EXTRA_SOURCES}
)

set_target_properties(
  ${PROJECT_NAME} PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)

target_compile_options(${PROJECT_NAME} PRIVATE
  -Wall -Wextra -Werror -pg
)

find_package(X11 REQUIRED)

include(CheckCXXSymbolExists)

check_cxx_symbol_exists(std::format    "format" HAVE_STD_FORMAT)
check_cxx_symbol_exists(std::format_to "format" HAVE_STD_FORMAT_TO)
check_cxx_symbol_exists(std::print     "format" HAVE_STD_PRINT)

if(NOT HAVE_STD_FORMAT OR NOT HAVE_STD_FORMAT_TO OR NOT HAVE_STD_PRINT)
  message(WARNING "std::format, std::format_to, or std::print is not available.  Using libfmt as polyfill.")
  find_package(fmt REQUIRED)

  target_link_libraries(
    ${PROJECT_NAME}
    fmt::fmt
  )
endif()

target_link_libraries(
  ${PROJECT_NAME}
  X11::X11
  X11::Xft
  X11::Xext
  ${DWMIPCPP_LIBRARIES}
  pthread
)

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
