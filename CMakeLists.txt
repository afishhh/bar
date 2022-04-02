cmake_minimum_required(VERSION 3.1)

set(PROJECT_NAME bar)
project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-fsanitize=address -g ${CMAKE_CXX_FLAGS}")

add_executable(
  ${PROJECT_NAME}

  src/main.cc
  src/draw.cc
  src/blocks/memory.cc
  src/blocks/battery.cc
  src/blocks/network.cc
  src/blocks/cpu.cc
  src/blocks/clock.cc
  src/blocks/script.cc
)

find_package(X11 REQUIRED)

target_link_libraries(
  ${PROJECT_NAME}
  X11::X11
  X11::Xft
)
