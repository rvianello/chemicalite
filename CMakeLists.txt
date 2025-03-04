cmake_minimum_required(VERSION 3.14)

project(chemicalite)

set(CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD ${CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CHEMICALITE_VERSION "2024.05.1")

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

find_package(Boost 1.58.0 COMPONENTS system serialization iostreams REQUIRED)

find_package(RDKit 2023.09.1 REQUIRED)
include_directories(${RDKit_INCLUDE_DIRS})

option(CHEMICALITE_ENABLE_OPTIMIZED_POPCNT
       "enable compiler optimization for the native architecture" ${RDKit_USE_OPTIMIZED_POPCNT})
option(CHEMICALITE_ENABLE_TESTS "enable tests" ON)
option(CHEMICALITE_ENABLE_CPPCHECK "enable static analysis with CppCheck" ON)

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

if (CHEMICALITE_ENABLE_CPPCHECK)
    # Define the CppCheck target
    find_program(CPPCHECK cppcheck)
    if(CPPCHECK)
        add_custom_target(
            cppcheck
            COMMAND ${CPPCHECK}
              --enable=all
              --inconclusive
              --quiet
              --std=c++${CXX_STANDARD}
              --cppcheck-build-dir=${CMAKE_BINARY_DIR}/cppcheck
              --inline-suppr
              -I ${CMAKE_SOURCE_DIR}/include
              ${CMAKE_SOURCE_DIR}/src
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running CppCheck"
        )
    else()
        message(WARNING "CppCheck not found. Please install CppCheck to enable static analysis.")
    endif()
endif (CHEMICALITE_ENABLE_CPPCHECK)
