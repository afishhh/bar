cmake_minimum_required(VERSION 3.27)

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
  src/run.cc
  src/guard.cc
  src/util.cc
  src/log.cc
  src/block.cc
  src/bar.cc
  src/bufdraw.cc
  src/blocks/memory.cc
  src/blocks/battery.cc
  src/blocks/network.cc
  src/blocks/cpu.cc
  src/blocks/clock.cc
  src/blocks/script.cc
  src/blocks/disk.cc
  # src/blocks/systray.cc
  src/ui/window.cc
  src/ui/text.cc

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
  target_link_options(${PROJECT_NAME} PRIVATE -g -rdynamic)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE
    -Werror -flto -g
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

find_package(PkgConfig REQUIRED)
find_package(glfw3 REQUIRED)
pkg_check_modules(PANGO REQUIRED pango pangocairo)
pkg_check_modules(cairo REQUIRED cairo)
pkg_check_modules(libuv REQUIRED libuv)
pkg_check_modules(wayland-client REQUIRED wayland-client)

if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/glad-generated")
  execute_process(
    COMMAND glad --api gl:core=3.0 --reproducible --out-path "${CMAKE_CURRENT_BINARY_DIR}/glad-generated" c
  )
endif()

if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/glad.o")
  execute_process(
    COMMAND cc "-I${CMAKE_CURRENT_BINARY_DIR}/glad-generated/include" "${CMAKE_CURRENT_BINARY_DIR}/glad-generated/src/gl.c" -c -o "${CMAKE_CURRENT_BINARY_DIR}/glad.o"
  )
endif()

target_link_options("${PROJECT_NAME}" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/glad.o")

target_link_libraries(
  ${PROJECT_NAME}
  X11::X11
  X11::Xext
  X11::Xrandr
  fmt::fmt
  ${PANGO_LIBRARIES}
  ${cairo_LIBRARIES}
  ${DWMIPCPP_LIBRARIES}
  ${libuv_LIBRARIES}
  pthread
  glfw
  ${wayland-client_LIBRARIES}
)

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC
  ${PANGO_INCLUDE_DIRS}
  "${CMAKE_CURRENT_BINARY_DIR}/glad-generated/include"
  ${libuv_INCLUDE_DIRS}
  ${wayland-client_INCLUDE_DIRS}
)

target_compile_options(
  ${PROJECT_NAME}
  PUBLIC
  ${PANGO_CFLAGS_OTHER}
  ${libuv_CFLAGS_OTHER}
)

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
