cmake_minimum_required(VERSION 2.6)

project(chemicalite)

find_package(RDKit REQUIRED)
include_directories(${RDKit_INCLUDE_DIRS})

add_library(chemicalite MODULE 
            object.c
	    molecule.c
            chemicalite.c
            rdkit_adapter.cpp)
target_link_libraries(chemicalite 
                      Descriptors Fingerprints)
