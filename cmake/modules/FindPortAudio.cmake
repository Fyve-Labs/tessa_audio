# FindPortAudio.cmake - Find PortAudio library
#
# This module defines:
#  PORTAUDIO_FOUND - True if PortAudio is found
#  PORTAUDIO_INCLUDE_DIRS - The PortAudio include directories
#  PORTAUDIO_LIBRARIES - The PortAudio libraries to link against
#
# Based on FindPortAudio.cmake from JUCE

# Look for PortAudio includes
find_path(PORTAUDIO_INCLUDE_DIR
  NAMES portaudio.h
  PATHS
    /usr/include
    /usr/local/include
    /opt/local/include
    /sw/include
  DOC "The PortAudio include directory"
)

# Find the PortAudio library (with multiple name options)
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  # On macOS/iOS, prefer the framework version if available
  find_library(PORTAUDIO_LIBRARY 
    NAMES portaudio PortAudio
    PATHS
      /Library/Frameworks
      ~/Library/Frameworks
      /usr/local/lib
      /usr/lib
      /sw/lib
      /opt/local/lib
    DOC "The PortAudio library"
  )
else()
  # Linux, Windows, etc.
  find_library(PORTAUDIO_LIBRARY
    NAMES portaudio portaudio_static portaudio-2.0 portaudio19
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
    DOC "The PortAudio library"
  )
endif()

# Handle the QUIETLY and REQUIRED arguments and set PORTAUDIO_FOUND
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PortAudio 
  DEFAULT_MSG 
  PORTAUDIO_LIBRARY PORTAUDIO_INCLUDE_DIR
)

# Set output variables
if(PORTAUDIO_FOUND)
  set(PORTAUDIO_LIBRARIES ${PORTAUDIO_LIBRARY})
  set(PORTAUDIO_INCLUDE_DIRS ${PORTAUDIO_INCLUDE_DIR})
endif()

# Hide these variables in GUIs
mark_as_advanced(PORTAUDIO_INCLUDE_DIR PORTAUDIO_LIBRARY) 