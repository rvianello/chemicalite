cmake_minimum_required(VERSION 3.14)

project(chemicalite)

set(CHEMICALITE_VERSION "2020.12.1")

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

add_subdirectory(src)

option(CHEMICALITE_ENABLE_NATIVE_OPT
       "enable compiler optimization for the native architecture" ${RDKit_USE_OPTIMIZED_POPCNT})
option(CHEMICALITE_ENABLE_TESTS "enable tests" ON)
option(CHEMICALITE_ENABLE_PYTHON_TESTS "enable python tests" ON)
option(CHEMICALITE_BUILD_EXAMPLES "build the example programs" ON)

if (CHEMICALITE_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
    if (CHEMICALITE_ENABLE_PYTHON_TESTS)
        find_package (Python3 COMPONENTS Interpreter)
        # FIXME should also check that APSW is available
        add_subdirectory(pytests)
    endif (CHEMICALITE_ENABLE_PYTHON_TESTS)
endif (CHEMICALITE_ENABLE_TESTS)

if (CHEMICALITE_ENABLE_NATIVE_OPT)
    if (CMAKE_COMPILER_IS_GNUCC)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    endif (CMAKE_COMPILER_IS_GNUCC)
endif (CHEMICALITE_ENABLE_NATIVE_OPT)

if (CHEMICALITE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif (CHEMICALITE_BUILD_EXAMPLES)
