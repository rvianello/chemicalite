cmake_minimum_required(VERSION 3.14)

project(chemicalite)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CHEMICALITE_VERSION "2021.07.2")

add_compile_definitions("CHEMICALITE_VERSION=${CHEMICALITE_VERSION}")

if (MSVC)
    # warning level 4 and all warnings as errors
    add_compile_options(/W4 /WX)
else()
    # lots of warnings and all warnings as errors
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

find_package(SQLite3 REQUIRED)
include_directories(${SQLite3_INCLUDE_DIRS})

find_package(RDKit 2020.09.1 REQUIRED)
include_directories(${RDKit_INCLUDE_DIRS})

option(CHEMICALITE_ENABLE_OPTIMIZED_POPCNT
       "enable compiler optimization for the native architecture" ${RDKit_USE_OPTIMIZED_POPCNT})
option(CHEMICALITE_ENABLE_TESTS "enable tests" ON)

if (CHEMICALITE_ENABLE_OPTIMIZED_POPCNT)
  # this logic is copied from the RDKit's cmake scripts
  if (NOT MSVC)
    if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "aarch64")
      message("RDK_OPTIMIZE_POPCNT is not available on aarch64")
    elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(ppc64|powerpc64)")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mpopcntd")
    else()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mpopcnt")
    endif()
  endif()
endif (CHEMICALITE_ENABLE_OPTIMIZED_POPCNT)

if (CHEMICALITE_ENABLE_TESTS)
    find_program(MEMORYCHECK_COMMAND NAMES valgrind)
    set(MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full")
    include(CTest)
    enable_testing()
endif (CHEMICALITE_ENABLE_TESTS)

add_subdirectory(src)
