find_package(Catch2 REQUIRED)
include(Catch)

set(TEST_CHEMICALITE_SRC_FILES
    test_main.cpp
    test_version_info.cpp
)

add_executable(test_chemicalite ${TEST_CHEMICALITE_SRC_FILES})

target_link_libraries(
    test_chemicalite
    Catch2::Catch2
    ${CHEMICALITE_RDKIT_LIBRARIES}
    ${SQLite3_LIBRARIES}
    )

catch_discover_tests(test_chemicalite)

#add_library(testcommon testcommon.c)

#macro(chemicalite_ctest test)
#    add_executable(${test} ${test}.c)
#    target_link_libraries(${test} testcommon ${SQLite3_LIBRARIES})
#    add_test(${test} ${test} ${ARGN})
#endmacro(chemicalite_ctest)

#chemicalite_ctest(initialize)
#chemicalite_ctest(smiles_roundtrip)
#chemicalite_ctest(create_rdtree)
#chemicalite_ctest(rdtree_1st_insert)
#chemicalite_ctest(rdtree_single_select_by_id)
#chemicalite_ctest(rdtree_single_select_by_subset)
#chemicalite_ctest(versions)

