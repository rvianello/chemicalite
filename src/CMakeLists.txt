include(GNUInstallDirs)

add_library(chemicalite MODULE
 	    molecule.c
	    bfp_ops.c
 	    bitstring.c
	    rdtree.c
        chemicalite.c
        rdkit_adapter.cpp)

set_target_properties(chemicalite PROPERTIES PREFIX "")

target_link_libraries(chemicalite PUBLIC
                      RDKit::Descriptors RDKit::Fingerprints RDKit::GraphMol  ${SQLite3_LIBRARIES})

install(TARGETS chemicalite LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
