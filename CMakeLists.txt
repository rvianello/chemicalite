cmake_minimum_required(VERSION 2.6)

project(chemicalite)

set(CMAKE_MODULE_PATH 
    ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

find_package(RDKit REQUIRED)
include_directories(${RDKit_INCLUDE_DIR})

add_library(chemicalite MODULE 
            chemicalite.cpp
            rdkit_adapter.cpp)
target_link_libraries(chemicalite ${RDKit_DESCRIPTORS_LIBRARY})
