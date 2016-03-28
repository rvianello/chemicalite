cmake_minimum_required(VERSION 3.0)

project(chemicalite)

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

find_package(Sqlite3 REQUIRED)
include_directories(${SQLITE3_INCLUDE_DIR})

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

target_link_libraries(chemicalite 
                      Descriptors Fingerprints GraphMol  ${SQLITE3_LIBRARY})

add_subdirectory(examples)

option(CHEMICALITE_ENABLE_TESTS "enable tests" ON)
option(CHEMICALITE_ENABLE_PYTHON_TESTS "enable python tests" ON)

if (CHEMICALITE_ENABLE_TESTS) 
    enable_testing()
    add_subdirectory(test)
    if (CHEMICALITE_ENABLE_PYTHON_TESTS)
        find_package(PythonInterp REQUIRED)
    endif (CHEMICALITE_ENABLE_PYTHON_TESTS)
endif (CHEMICALITE_ENABLE_TESTS) 

