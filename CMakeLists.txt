cmake_minimum_required(VERSION 2.6)

project(chemicalite)

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

find_package(Sqlite3 REQUIRED)

find_package(RDKit REQUIRED)
include_directories(${RDKit_INCLUDE_DIRS})

add_library(chemicalite MODULE 
 	    molecule.c
	    bfp_ops.c
 	    bitstring.c
	    rdtree.c
            chemicalite.c
            rdkit_adapter.cpp)
target_link_libraries(chemicalite 
                      Descriptors Fingerprints)

get_target_property(CHEMICALITE_LOCATION chemicalite LOCATION)

enable_testing()
add_subdirectory(test)

