#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SDL2_mixer::SDL2_mixer" for configuration "Debug"
set_property(TARGET SDL2_mixer::SDL2_mixer APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(SDL2_mixer::SDL2_mixer PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/SDL2_mixerd.lib"
  IMPORTED_LINK_DEPENDENT_LIBRARIES_DEBUG "SDL2::SDL2;Vorbis::vorbisfile;WavPack::WavPack"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/SDL2_mixerd.dll"
  )

list(APPEND _cmake_import_check_targets SDL2_mixer::SDL2_mixer )
list(APPEND _cmake_import_check_files_for_SDL2_mixer::SDL2_mixer "${_IMPORT_PREFIX}/debug/lib/SDL2_mixerd.lib" "${_IMPORT_PREFIX}/debug/bin/SDL2_mixerd.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
