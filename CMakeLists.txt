cmake_minimum_required(VERSION 3.1...3.27)

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
  src/bufdraw.cc
  src/blocks/memory.cc
  src/blocks/battery.cc
  src/blocks/network.cc
  src/blocks/cpu.cc
  src/blocks/clock.cc
  src/blocks/script.cc
  src/blocks/disk.cc
  src/blocks/systray.cc
  src/ui/connection.cc
  src/ui/x11/connection.cc
  src/ui/x11/draw.cc
  src/ui/x11/window.cc

  ${EXTRA_SOURCES}
)

set_target_properties(
  ${PROJECT_NAME} PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)

target_compile_options(${PROJECT_NAME} PRIVATE
  -Wall -Wextra
)

set(ASAN_DEFUALT OFF)
if(CMAKE_BUILD_TYPE STREQUAL Debug)
  set(ASAN_DEFUALT ON)
  target_compile_options(${PROJECT_NAME} PRIVATE
    -g
  )
else()
  target_compile_options(${PROJECT_NAME} PRIVATE
    -Werror -flto
  )
  target_link_options(${PROJECT_NAME} PRIVATE -flto)
endif()

option(ASAN "Enable AddressSanitizer" ${ASAN_DEFAULT})
if(ASAN)
  target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined)
  target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined)
endif()

find_package(X11 REQUIRED)
find_package(fmt REQUIRED)

target_link_libraries(
  ${PROJECT_NAME}
  X11::X11
  X11::Xft
  X11::Xext
  fmt::fmt
  ${DWMIPCPP_LIBRARIES}
  pthread
)

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
