# cmake/ArxSoundConfig.cmake.in

####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was ArxSoundConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

include(CMakeFindDependencyMacro)

# Check for required dependencies
set(AS_HAS_ARXGLUE ON)

if(AS_HAS_ARXGLUE)
    find_dependency(ArxGlue REQUIRED)
endif()

# Platform-specific dependencies
if(WIN32)
    # Windows libraries are system libraries, no need to find
elseif(LINUX)
    if(ON)
        find_dependency(ALSA)
    endif()
    if(ON)
        find_dependency(PulseAudio)
    endif()
    if(ON)
        find_dependency(JACK)
    endif()
elseif(APPLE)
    # Frameworks are system frameworks, no need to find
endif()

# Include the targets file
include("${CMAKE_CURRENT_LIST_DIR}/ArxSoundTargets.cmake")

# Check that all required components are found
check_required_components(ArxSound)

# Provide some useful variables
set(ARXSOUND_INCLUDE_DIRS "")
set(ARXSOUND_LIBRARIES ArxSound::ArxSound)
set(ARXSOUND_VERSION "0.1.0")
set(ARXSOUND_VERSION_MAJOR "0")
set(ARXSOUND_VERSION_MINOR "1")
set(ARXSOUND_VERSION_REVISION "")

# Backend availability
set(ARXSOUND_HAS_WASAPI ON)
set(ARXSOUND_HAS_DSOUND ON)
set(ARXSOUND_HAS_WINMM ON)
set(ARXSOUND_HAS_ALSA ON)
set(ARXSOUND_HAS_PULSEAUDIO ON)
set(ARXSOUND_HAS_JACK ON)
set(ARXSOUND_HAS_COREAUDIO ON)
set(ARXSOUND_HAS_AAUDIO ON)
set(ARXSOUND_HAS_OPENSL ON)
set(ARXSOUND_HAS_WEBAUDIO ON)
set(ARXSOUND_HAS_ARXGLUE ON)
