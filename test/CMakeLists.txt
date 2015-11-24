add_library(testcommon testcommon.c)

file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/chemicalite_location.h"
     CONTENT
"#ifndef CHEMICALITE_LOCATION_INCLUDED
#define CHEMICALITE_LOCATION_INCLUDED

const char CHEMICALITE_LOCATION[] = \"$<TARGET_FILE:chemicalite>\";

#endif
"
     )
include_directories(${CMAKE_CURRENT_BINARY_DIR})
     
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/chemicalite_location.py"
     CONTENT
"CHEMICALITE_LOCATION = \"$<TARGET_FILE:chemicalite>\"
"
     )

macro(chemicalite_ctest test)
    add_executable(${test} ${test}.c)
    target_link_libraries(${test} testcommon ${SQLITE3_LIBRARY})
    add_test(${test} ${test} ${ARGN})
endmacro(chemicalite_ctest)

macro(chemicalite_pytest module)
    add_test(${module} 
             python ${CMAKE_CURRENT_SOURCE_DIR}/${module}.py)
endmacro(chemicalite_pytest)

chemicalite_pytest(load_extension)
chemicalite_pytest(molecule)
chemicalite_pytest(descriptors)
chemicalite_pytest(bitstring)

chemicalite_ctest(initialize)
chemicalite_ctest(create_rdtree)
chemicalite_ctest(rdtree_1st_insert)
chemicalite_ctest(rdtree_single_select_by_id)
chemicalite_ctest(rdtree_single_select_by_subset)

chemicalite_pytest(rdtree)

set_tests_properties(load_extension molecule descriptors bitstring rdtree
                     PROPERTIES ENVIRONMENT
		     "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}")
		     
