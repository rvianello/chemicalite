cmake_minimum_required(VERSION 3.14)

project(chemicalite)

find_package(SQLite3 REQUIRED)
include_directories(${SQLite3_INCLUDE_DIRS})

find_package(RDKit REQUIRED)
include_directories(${RDKit_INCLUDE_DIRS})

add_library(chemicalite MODULE
 	    molecule.c
	    bfp_ops.c
 	    bitstring.c
	    rdtree.c
        chemicalite.c
        rdkit_adapter.cpp)

set_target_properties(chemicalite PROPERTIES PREFIX "")

target_link_libraries(chemicalite PUBLIC
                      RDKit::Descriptors RDKit::Fingerprints RDKit::GraphMol  ${SQLite3_LIBRARIES})

add_subdirectory(examples)

option(CHEMICALITE_ENABLE_NATIVE_OPT
       "enable compiler optimization for the native architecture" ON)
option(CHEMICALITE_ENABLE_TESTS "enable tests" ON)
option(CHEMICALITE_ENABLE_PYTHON_TESTS "enable python tests" OFF)

if (CHEMICALITE_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(test)
    if (CHEMICALITE_ENABLE_PYTHON_TESTS)
        find_package(PythonInterp REQUIRED)
    endif (CHEMICALITE_ENABLE_PYTHON_TESTS)
endif (CHEMICALITE_ENABLE_TESTS)

if (CHEMICALITE_ENABLE_NATIVE_OPT)
    if (CMAKE_COMPILER_IS_GNUCC)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    endif (CMAKE_COMPILER_IS_GNUCC)
endif (CHEMICALITE_ENABLE_NATIVE_OPT)
