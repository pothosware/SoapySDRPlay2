if(NOT LIBSDRPLAY_FOUND)
  find_package(PkgConfig)
  pkg_check_modules (LIBSDRPLAY_PKG libsdrplay)
  find_path(LIBSDRPLAY_INCLUDE_DIRS NAMES mirsdrapi-rsp.h
    PATHS
    ${LIBSDRPLAY_PKG_INCLUDE_DIRS}
    /usr/include
    /usr/local/include
  )

  find_library(LIBSDRPLAY_LIBRARIES NAMES mirsdrapi-rsp
    PATHS
    ${LIBSDRPLAY_PKG_LIBRARY_DIRS}
    /usr/lib
    /usr/local/lib
  )

if(LIBSDRPLAY_INCLUDE_DIRS AND LIBSDRPLAY_LIBRARIES)
  set(LIBSDRPLAY_FOUND TRUE CACHE INTERNAL "libsdrplay found")
  message(STATUS "Found libsdrplay: ${LIBSDRPLAY_INCLUDE_DIRS}, ${LIBSDRPLAY_LIBRARIES}")
else(LIBSDRPLAY_INCLUDE_DIRS AND LIBSDRPLAY_LIBRARIES)
  set(LIBSDRPLAY_FOUND FALSE CACHE INTERNAL "libsdrplay found")
  message(STATUS "libsdrplay not found.")
endif(LIBSDRPLAY_INCLUDE_DIRS AND LIBSDRPLAY_LIBRARIES)

mark_as_advanced(LIBSDRPLAY_LIBRARIES LIBSDRPLAY_INCLUDE_DIRS)

endif(NOT LIBSDRPLAY_FOUND)
