add_library(testcommon testcommon.c)

macro(chemicalite_ctest test)
    add_executable(${test} ${test}.c)
    target_link_libraries(${test} testcommon ${SQLite3_LIBRARIES})
    add_test(${test} ${test} ${ARGN})
endmacro(chemicalite_ctest)

chemicalite_ctest(initialize)
chemicalite_ctest(smiles_roundtrip)
chemicalite_ctest(create_rdtree)
chemicalite_ctest(rdtree_1st_insert)
chemicalite_ctest(rdtree_single_select_by_id)
chemicalite_ctest(rdtree_single_select_by_subset)
chemicalite_ctest(versions)

