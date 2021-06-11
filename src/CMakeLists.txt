include(GNUInstallDirs)

add_library(chemicalite MODULE
        settings.cpp
        logging.cpp
        mol.cpp
        bfp.cpp
        mol_fmcs.cpp
        mol_formats.cpp
        mol_chemtransforms.cpp
        mol_compare.cpp
        mol_descriptors.cpp
        mol_hash.cpp
        mol_props.cpp
        #mol_standardize.cpp
        mol_to_bfp.cpp
        bfp_ops.cpp
        bfp_compare.cpp
        bfp_descriptors.cpp
        periodic_table.cpp
        rdtree.cpp
        rdtree_vtab.cpp
        rdtree_node.cpp
        rdtree_item.cpp
        rdtree_strategy.cpp
        rdtree_constraint.cpp
        rdtree_constraint_subset.cpp
        rdtree_constraint_tanimoto.cpp
        sdf_io.cpp
        versions.cpp
        chemicalite.cpp
        )

set_target_properties(chemicalite PROPERTIES PREFIX "")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(chemicalite PROPERTIES SUFFIX .dylib)
endif()

set(CHEMICALITE_RDKIT_LIBRARIES
    RDKit::MolHash
    RDKit::Descriptors
    RDKit::Fingerprints
    RDKit::FileParsers
    RDKit::ChemTransforms
    RDKit::FMCS
    RDKit::MolStandardize
    RDKit::GraphMol
    )

target_link_libraries(chemicalite PUBLIC
    ${CHEMICALITE_RDKIT_LIBRARIES}
    ${SQLite3_LIBRARIES}
    )

install(TARGETS chemicalite LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if (CHEMICALITE_ENABLE_TESTS)
    add_subdirectory(tests)
endif (CHEMICALITE_ENABLE_TESTS)

