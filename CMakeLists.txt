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

add_executable(submarine_noir
  src/main.cpp
)

target_include_directories(submarine_noir PRIVATE src)

target_compile_options(submarine_noir PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

if(TARGET raylib)
  target_link_libraries(submarine_noir PRIVATE raylib)
elseif(TARGET raylib::raylib)
  target_link_libraries(submarine_noir PRIVATE raylib::raylib)
else()
  message(WARNING "raylib target not found. Building with local headless compatibility backend.")
  target_sources(submarine_noir PRIVATE src/raylib_compat.cpp)
endif()

if ((TARGET raylib OR TARGET raylib::raylib) AND UNIX AND NOT APPLE)
  target_link_libraries(submarine_noir PRIVATE m pthread dl rt X11)
endif()
