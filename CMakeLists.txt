
cmake_minimum_required(VERSION 3.15...4.0)

project(test)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(volk STATIC lib/volk.c)
target_include_directories(volk PUBLIC include)

if (WIN32)
  add_library(SDL3 SHARED IMPORTED)
  set_target_properties(SDL3 PROPERTIES
    IMPORTED_IMPLIB "${CMAKE_SOURCE_DIR}/lib/SDL3.lib"
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/lib/SDL3.dll"
  )
elseif(UNIX)
  find_package(SDL3 REQUIRED)
endif()

add_executable(
  main
  src/main.cpp
  src/vma.cpp
)

if (WIN32)
  target_compile_definitions(main PRIVATE
        "__FILE_NAME__=__FILE__"
    )
endif()  

target_include_directories(main PRIVATE include)

target_link_libraries(main PRIVATE SDL3 volk)

if(WIN32)
    add_custom_command(TARGET main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/lib/SDL3.dll"
            "$<TARGET_FILE_DIR:main>"
    )
endif()

