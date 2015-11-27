add_library(testcommon testcommon.c)

macro(chemicalite_ctest test)
    add_executable(${test} ${test}.c)
    target_link_libraries(${test} testcommon ${SQLITE3_LIBRARY})
    add_test(${test} ${test} ${ARGN})
endmacro(chemicalite_ctest)

chemicalite_ctest(initialize)
chemicalite_ctest(create_rdtree)
chemicalite_ctest(rdtree_1st_insert)
chemicalite_ctest(rdtree_single_select_by_id)
chemicalite_ctest(rdtree_single_select_by_subset)

find_package(PythonInterp)

if (PYTHONINTERP_FOUND)
    macro(chemicalite_pytest module)
        add_test(${module}
                 python ${CMAKE_CURRENT_SOURCE_DIR}/${module}.py)
    endmacro(chemicalite_pytest)

    chemicalite_pytest(load_extension)
    chemicalite_pytest(molecule)
    chemicalite_pytest(descriptors)
    chemicalite_pytest(bitstring)
    chemicalite_pytest(rdtree)
endif (PYTHONINTERP_FOUND)

