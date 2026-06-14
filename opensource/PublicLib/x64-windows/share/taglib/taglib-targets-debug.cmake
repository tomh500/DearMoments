#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "TagLib::tag" for configuration "Debug"
set_property(TARGET TagLib::tag APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(TagLib::tag PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/tag.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/tag.dll"
  )

list(APPEND _cmake_import_check_targets TagLib::tag )
list(APPEND _cmake_import_check_files_for_TagLib::tag "${_IMPORT_PREFIX}/debug/lib/tag.lib" "${_IMPORT_PREFIX}/debug/bin/tag.dll" )

# Import target "TagLib::tag_c" for configuration "Debug"
set_property(TARGET TagLib::tag_c APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(TagLib::tag_c PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/tag_c.lib"
  IMPORTED_LINK_DEPENDENT_LIBRARIES_DEBUG "TagLib::tag"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/tag_c.dll"
  )

list(APPEND _cmake_import_check_targets TagLib::tag_c )
list(APPEND _cmake_import_check_files_for_TagLib::tag_c "${_IMPORT_PREFIX}/debug/lib/tag_c.lib" "${_IMPORT_PREFIX}/debug/bin/tag_c.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
