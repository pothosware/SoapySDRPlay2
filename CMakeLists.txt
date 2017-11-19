########################################################################
# Build Soapy SDR support module for SDRPlay
########################################################################
cmake_minimum_required(VERSION 2.8.7)
project(SoapySDRPlay CXX)

find_package(SoapySDR "0.4.0" NO_MODULE REQUIRED)
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

#enable c++11 features
if(CMAKE_COMPILER_IS_GNUCXX)

    #C++11 is a required language feature for this project
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("-std=c++11" HAS_STD_CXX11)
    if(HAS_STD_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    else(HAS_STD_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
    endif()

    #disable warnings for unused parameters
    add_definitions(-Wno-unused-parameter)

endif(CMAKE_COMPILER_IS_GNUCXX)

# Configurable feature set 
SET (RF_GAIN_IN_MENU ON CACHE BOOL "Add Rf gain as a setting, additionally to the IFGR gain control")

IF(RF_GAIN_IN_MENU) 
    ADD_DEFINITIONS( -DRF_GAIN_IN_MENU=1 )
ENDIF()

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
