find_package(Catch2 REQUIRED)
include(Catch)

string(REGEX MATCH "clang" CMAKE_COMPILER_IS_CLANG "${CMAKE_C_COMPILER}")
if (CMAKE_COMPILER_IS_CLANG)
    # https://github.com/catchorg/Catch2/issues/1218
    add_compile_definitions("CATCH_CONFIG_NO_CPP17_UNCAUGHT_EXCEPTIONS")
endif()

set(TEST_CHEMICALITE_SRC_FILES
    test_main.cpp
    test_common.cpp
    test_version_info.cpp
    test_mol_chemtransforms.cpp
    test_mol_find_mcs.cpp
    test_mol_formats.cpp
    test_mol_descriptors.cpp
    test_mol_compare.cpp
    test_mol_hash.cpp
    test_mol_standardize.cpp
    test_bfp.cpp
    test_periodic_table.cpp
    test_rdtree_create.cpp
    test_rdtree_insert.cpp
    test_rdtree_select.cpp
    test_rdtree_update.cpp
    test_sdf_reader.cpp
    test_sdf_writer.cpp
    test_smi_reader.cpp
    test_smi_writer.cpp
)

add_executable(test_chemicalite ${TEST_CHEMICALITE_SRC_FILES})

target_link_libraries(
    test_chemicalite
    Catch2::Catch2
    ${CHEMICALITE_RDKIT_LIBRARIES}
    ${SQLite3_LIBRARIES}
    )

catch_discover_tests(
    test_chemicalite
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data)

