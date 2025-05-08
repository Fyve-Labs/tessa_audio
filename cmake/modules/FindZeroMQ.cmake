# FindZeroMQ.cmake - Find ZeroMQ library
#
# This module defines:
#  ZeroMQ_FOUND - True if ZeroMQ is found
#  ZeroMQ_INCLUDE_DIRS - The ZeroMQ include directories
#  ZeroMQ_LIBRARIES - The ZeroMQ libraries to link against

# Look for ZeroMQ includes
find_path(ZeroMQ_INCLUDE_DIR
  NAMES zmq.h
  PATHS
    /usr/include
    /usr/local/include
    /opt/local/include
    /sw/include
  DOC "The ZeroMQ include directory"
)

# Find the ZeroMQ library
find_library(ZeroMQ_LIBRARY
  NAMES zmq libzmq
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
    /sw/lib
  DOC "The ZeroMQ library"
)

# Handle the QUIETLY and REQUIRED arguments and set ZeroMQ_FOUND
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZeroMQ
  DEFAULT_MSG
  ZeroMQ_LIBRARY ZeroMQ_INCLUDE_DIR
)

# Set output variables
if(ZeroMQ_FOUND)
  set(ZeroMQ_LIBRARIES ${ZeroMQ_LIBRARY})
  set(ZeroMQ_INCLUDE_DIRS ${ZeroMQ_INCLUDE_DIR})
endif()

# Hide these variables in GUIs
mark_as_advanced(ZeroMQ_INCLUDE_DIR ZeroMQ_LIBRARY) 