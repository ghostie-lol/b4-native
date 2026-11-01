cmake_minimum_required(VERSION 3.22)
project(b4menu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Picks up every .cpp in the repo root, src/ and source/ (edit if your layout differs)
file(GLOB B4_SOURCES
  ${CMAKE_SOURCE_DIR}/*.cpp
  ${CMAKE_SOURCE_DIR}/src/*.cpp
  ${CMAKE_SOURCE_DIR}/source/*.cpp)

add_library(b4menu SHARED ${B4_SOURCES})

target_include_directories(b4menu PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/source)

target_compile_options(b4menu PRIVATE -O2 -fvisibility=default -Wall)

find_library(log-lib log)
target_link_libraries(b4menu ${log-lib})
