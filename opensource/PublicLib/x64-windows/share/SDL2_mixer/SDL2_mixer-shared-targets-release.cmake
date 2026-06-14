#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SDL2_mixer::SDL2_mixer" for configuration "Release"
set_property(TARGET SDL2_mixer::SDL2_mixer APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(SDL2_mixer::SDL2_mixer PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/SDL2_mixer.lib"
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "SDL2::SDL2;Vorbis::vorbisfile;WavPack::WavPack"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/SDL2_mixer.dll"
  )

list(APPEND _cmake_import_check_targets SDL2_mixer::SDL2_mixer )
list(APPEND _cmake_import_check_files_for_SDL2_mixer::SDL2_mixer "${_IMPORT_PREFIX}/lib/SDL2_mixer.lib" "${_IMPORT_PREFIX}/bin/SDL2_mixer.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
