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

find_library(RDKit_RDGENERAL_LIBRARY RDGeneral
             HINTS ${RDKit_LIBRARY_HINT_PATH})

# handle the QUIETLY and REQUIRED arguments and set RDKit_FOUND to TRUE if 
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RDKit DEFAULT_MSG 
                                  RDKit_RDGENERAL_LIBRARY 
                                  RDKit_INCLUDE_DIR)

if(RDKit_FOUND)
  set( RDKit_LIBRARIES ${RDKit_RDGENERAL_LIBRARY} )
else(RDKit_FOUND)
  set( RDKit_LIBRARIES )
endif(RDKit_FOUND)

mark_as_advanced( RDKit_RDGENERAL_LIBRARY RDKit_INCLUDE_DIR )
