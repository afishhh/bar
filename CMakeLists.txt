cmake_minimum_required(VERSION 3.1)

set(PROJECT_NAME bar)
project(${PROJECT_NAME})

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
endif()

add_executable(
  ${PROJECT_NAME}

  src/main.cc
  src/guard.cc
  src/draw.cc
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

find_package(X11 REQUIRED)

target_link_libraries(
  ${PROJECT_NAME}
  X11::X11
  X11::Xft
  ${DWMIPCPP_LIBRARIES}
  pthread
)
