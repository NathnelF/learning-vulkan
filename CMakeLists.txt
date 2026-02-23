
cmake_minimum_required(VERSION 3.15...4.0)

project(test)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED)
find_package(volk REQUIRED)

add_executable(
  main
  src/main.cpp
  src/vma.cpp
 # src/obj.cpp
)

target_include_directories(main PRIVATE include
/opt/slang/include)

target_link_libraries(main SDL3::SDL3 dl m volk::volk ktx
  /opt/slang/lib/libslang.so)
