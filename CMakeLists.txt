cmake_minimum_required(VERSION 2.6)

project(chemicalite)

find_package(RDKit REQUIRED)
include_directories(${RDKit_INCLUDE_DIRS})

add_library(chemicalite MODULE 
            object.c
 	    molecule.c
	    bfp_ops.c
 	    bitstring.c
            chemicalite.c
	    rdtree.c
            rdkit_adapter.cpp)
target_link_libraries(chemicalite 
                      Descriptors Fingerprints)

get_target_property(CHEMICALITE_LOCATION chemicalite LOCATION)

enable_testing()
add_subdirectory(test)

