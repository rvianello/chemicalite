include(GNUInstallDirs)

add_library(chemicalite MODULE
        settings.cpp
        logging.cpp
        #molecule.c
        #bfp_ops.c
        #bitstring.c
        #rdtree.c
        mol.cpp
        mol_formats.cpp
        chemicalite.cpp
        #rdkit_adapter.cpp
        )

set_target_properties(chemicalite PROPERTIES PREFIX "")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(chemicalite PROPERTIES SUFFIX .dylib)
endif()

set(CHEMICALITE_RDKIT_LIBRARIES
    RDKit::MolHash
    RDKit::Descriptors
    RDKit::Fingerprints
    RDKit::GraphMol
    )

target_link_libraries(chemicalite PUBLIC
    ${CHEMICALITE_RDKIT_LIBRARIES}
    ${SQLite3_LIBRARIES}
    )

install(TARGETS chemicalite LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if (CHEMICALITE_ENABLE_TESTS)
    add_subdirectory(tests)
    if (CHEMICALITE_ENABLE_PYTHON_TESTS)
        find_package (Python3 COMPONENTS Interpreter)
        add_subdirectory(pytests)
    endif (CHEMICALITE_ENABLE_PYTHON_TESTS)
endif (CHEMICALITE_ENABLE_TESTS)

