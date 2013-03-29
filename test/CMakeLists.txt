macro(chemicalite_pytest module)
    add_test(${module} 
             python ${CMAKE_CURRENT_SOURCE_DIR}/${module}.py 
	     ${CHEMICALITE_LOCATION})
endmacro(chemicalite_pytest)

chemicalite_pytest(load_extension)
chemicalite_pytest(molecule)
chemicalite_pytest(descriptors)
chemicalite_pytest(bitstring)

add_library(testcommon testcommon.c)

macro(chemicalite_ctest test)
    add_executable(${test} ${test}.c)
    target_link_libraries(${test} testcommon sqlite3)
    add_test(${test} ${test} ${CHEMICALITE_LOCATION} ${ARGN})
endmacro(chemicalite_ctest)

chemicalite_ctest(initialize)
chemicalite_ctest(create_rdtree)
chemicalite_ctest(rdtree_1st_insert)
chemicalite_ctest(rdtree_single_select_by_id)
