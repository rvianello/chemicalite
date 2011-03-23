# - Find RDKit
# Find the RDKit includes and libraries
#
#  RDKit_INCLUDE_DIR - where to find RDGeneral/version.h and the other headers
#  RDKit_LIBRARIES   - List of libraries when using RDKit
#  RDKit_FOUND       - True if RDKit was found.

if(RDKit_INCLUDE_DIR)
  # Already in cache, be silent
  set(RDKit_FIND_QUIETLY TRUE)
endif(RDKit_INCLUDE_DIR)

if(RDBASE)
  set(RDKit_INCLUDE_HINT_PATH ${RDBASE}/Code)
  set(RDKit_LIBRARY_HINT_PATH ${RDBASE}/lib)
endif(RDBASE)

find_path(RDKit_INCLUDE_DIR RDGeneral
          HINTS ${RDKit_INCLUDE_HINT_PATH})

foreach (_LIBTAG
         ChemicalFeatures 
         RDGeneral SimDivPickers EigenSolvers Catalogs DataStructs 
         RDGeometryLib Optimizer ForceField Alignment GraphMol
         SubstructMatch Depictor SLNParse SmilesParse FileParsers
         ChemReactions ChemTransforms PartialCharges Subgraphs
         Descriptors DistGeometry ForceFieldHelpers DistGeomHelpers
         Fingerprints FragCatalog MolTransforms ShapeHelpers MolAlign
         MolCatalog MolChemicalFeatures RDBoost)

    string(TOUPPER ${_LIBTAG} _LIBTAG_UPPER)
    set(_LIBVAR RDKit_${_LIBTAG_UPPER}_LIBRARY)
    find_library(${_LIBVAR} ${_LIBTAG} 
                 HINTS ${RDKit_LIBRARY_HINT_PATH})
    set(RDKit_LIBRARY_VARS ${RDKit_LIBRARY_VARS} ${_LIBVAR}) 

endforeach (_LIBTAG)

# handle the QUIETLY and REQUIRED arguments and set RDKIT_FOUND to TRUE if 
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RDKit DEFAULT_MSG ${RDKit_LIBRARY_VARS})

if (RDKIT_FOUND)
    foreach (_LIBVAR ${RDKit_LIBRARY_VARS})
      set(RDKit_LIBRARIES ${RDKit_LIBRARIES} ${${_LIBVAR}})
    endforeach (_LIBVAR)
else (RDKIT_FOUND)
  set( RDKit_LIBRARIES )
endif (RDKIT_FOUND)

mark_as_advanced( RDKit_LIBRARIES ${RDKit_LIBRARIES} RDKit_INCLUDE_DIR )
