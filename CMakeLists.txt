cmake_minimum_required(VERSION 3.16)
project(SubmarineNoirPrototype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_FETCHCONTENT_RAYLIB "Download raylib automatically if not found" ON)

find_package(raylib QUIET)

if(NOT TARGET raylib AND NOT TARGET raylib::raylib AND USE_FETCHCONTENT_RAYLIB)
  include(FetchContent)
  FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
  )
  FetchContent_MakeAvailable(raylib)
endif()

if(TARGET raylib)
  set(RAYLIB_TARGET raylib)
elseif(TARGET raylib::raylib)
  set(RAYLIB_TARGET raylib::raylib)
else()
  message(FATAL_ERROR "raylib target not found. Install raylib or enable USE_FETCHCONTENT_RAYLIB with network access.")
endif()

add_executable(submarine_noir
  src/main.cpp
)

target_include_directories(submarine_noir PRIVATE src)

target_compile_options(submarine_noir PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(submarine_noir PRIVATE ${RAYLIB_TARGET})

if (UNIX AND NOT APPLE)
  target_link_libraries(submarine_noir PRIVATE m pthread dl rt X11)
endif()
