########################################################################
# Build Soapy SDR support module for SDRPlay
########################################################################
cmake_minimum_required(VERSION 2.8.7)
project(SoapySDRPlay CXX)

find_package(SoapySDR NO_MODULE)
if (NOT SoapySDR_FOUND)
    message(FATAL_ERROR "Soapy SDR development files not found...")
endif ()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
find_package(LibSDRplay)

if (NOT LIBSDRPLAY_FOUND)
    message(FATAL_ERROR "SDRPlay development files not found...")
endif ()
message(STATUS "LIBSDRPLAY_INCLUDE_DIRS - ${LIBSDRPLAY_INCLUDE_DIRS}")
message(STATUS "LIBSDRPLAY_LIBRARIES - ${LIBSDRPLAY_LIBRARIES}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${LIBSDRPLAY_INCLUDE_DIRS})

SOAPY_SDR_MODULE_UTIL(
    TARGET sdrPlaySupport
    SOURCES
        SoapySDRPlay.hpp
        Registation.cpp
        Settings.cpp
        Streaming.cpp
    LIBRARIES
        ${LIBSDRPLAY_LIBRARIES}
)
