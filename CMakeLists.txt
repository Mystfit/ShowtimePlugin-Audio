cmake_minimum_required(VERSION 3.13)

project(ShowtimePluginAudio VERSION 0.0.1)

# Use C14
set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED)

message(STATUS "Configuring audio plugin")

set(RTAUDIO_PLUGIN_TARGET "ShowtimePluginAudio")

# Setup Cmake paths
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake;${CMAKE_MODULE_PATH}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_SYSTEM_NAME})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/${CMAKE_SYSTEM_NAME})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/${CMAKE_SYSTEM_NAME})
if(NOT DEFINED PLUGIN_OUTPUT_DIR)
    set(PLUGIN_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins)
endif()

# Detect 64bit
if(${CMAKE_SIZEOF_VOID_P} STREQUAL "8")
  set(64bit TRUE)
elseif(${CMAKE_SIZEOF_VOID_P} STREQUAL "8")
  set(64bit FALSE)
endif()

# Set boost libraries
set(BOOST_COMPONENTS 
  filesystem
  thread
)

# Build options
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
  list(APPEND BOOST_COMPONENTS unit_test_framework)
endif()

# Get dependencies
find_package(Showtime CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# Get RtAudio package
include(FetchContent)
FetchContent_Declare(rtaudio
  GIT_REPOSITORY https://github.com/thestk/rtaudio
  GIT_TAG 5.1.0)
FetchContent_GetProperties(rtaudio)
if(NOT rtaudio_POPULATED)
  FetchContent_Populate(rtaudio)
  add_subdirectory(${rtaudio_SOURCE_DIR} ${rtaudio_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Get FST sdk
FetchContent_Declare(fst 
  GIT_REPOSITORY  https://git.iem.at/zmoelnig/FST.git
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
)
FetchContent_GetProperties(fst)
if(NOT fst)
  FetchContent_Populate(fst)

  include_directories(${fst_SOURCE_DIR} EXCLUDE_FROM_ALL)
endif()

# Get VST3 sdk
FetchContent_Declare(vst3sdk
  GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk
  GIT_TAG v3.7.3_build_20)
FetchContent_GetProperties(rtaudio)
if(NOT vst3sdk_POPULATED)
  FetchContent_Populate(vst3sdk)
  add_subdirectory(${vst3sdk_SOURCE_DIR} ${vst3sdk_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()



# Get Boost
find_package(Boost COMPONENTS ${BOOST_COMPONENTS} CONFIG)
if(NOT Boost_FOUND)
    message(STATUS "Searching for Boost using cmake's FindBoost")
    find_package(Boost COMPONENTS ${BOOST_COMPONENTS} REQUIRED)
endif()

# Set linked libs
set(RTAUDIO_PLUGIN_LIBS 
    Boost::boost
    Boost::thread
    rtaudio
    sdk_hosting
    sdk_common
    base
    pluginterfaces
    Showtime::Showtime
)

message(STATUS "RtAudio include dir: ${rtaudio_SOURCE_DIR}/include")

# Plugin source files
set(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/src")
set(ZST_RTAUDIO_PLUGIN_HEADERS
  "${SOURCE_DIR}/plugin.h"
  "${SOURCE_DIR}/AudioFactory.h"
  "${SOURCE_DIR}/AudioDevice.h"
  "${SOURCE_DIR}/AudioVSTHost.h"
  "${SOURCE_DIR}/AudioVSTFactory.h"
  "${SOURCE_DIR}/WindowController.h"
  "${SOURCE_DIR}/VSTPlugProvider.h"
  "${SOURCE_DIR}/platform/iwindow.h"
  "${SOURCE_DIR}/platform/iplatform.h"
  "${SOURCE_DIR}/platform/iapplication.h"
)
set(ZST_RTAUDIO_PLUGIN_SRC
  "${SOURCE_DIR}/plugin.cpp"
  "${SOURCE_DIR}/AudioFactory.cpp"
  "${SOURCE_DIR}/AudioDevice.cpp"
  "${SOURCE_DIR}/AudioVSTHost.cpp"
  "${SOURCE_DIR}/AudioVSTFactory.cpp"
  "${SOURCE_DIR}/VSTPlugProvider.cpp"
  "${SOURCE_DIR}/WindowController.cpp"
  "${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/plugprovider.cpp"
)

if(WIN32)
  list(APPEND ZST_RTAUDIO_PLUGIN_SRC 
    "${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/module_win32.cpp"
    "${SOURCE_DIR}/platform/win32/window.h"
    "${SOURCE_DIR}/platform/win32/window.cpp"
    "${SOURCE_DIR}/platform/win32/platform.cpp"
  )
endif()

# Plugin compile defs
set(PLUGIN_DEFINITIONS_PRIVATE "-DZST_EXPORT_PLUGIN_API")
set(PLUGIN_DEFINITIONS_INTERFACE "-DZST_IMPORT_PLUGIN_API")

# Create Library
add_library(${RTAUDIO_PLUGIN_TARGET} MODULE)

# Include files in target
target_sources(${RTAUDIO_PLUGIN_TARGET} PRIVATE 
    ${ZST_RTAUDIO_PLUGIN_HEADERS}
    ${ZST_RTAUDIO_PLUGIN_SRC}
)
target_include_directories(${RTAUDIO_PLUGIN_TARGET} PRIVATE ${rtaudio_SOURCE_DIR})


# Set compiler definitions
if(WIN32)
    target_compile_definitions(${RTAUDIO_PLUGIN_TARGET} PRIVATE ${PLUGIN_DEFINITIONS_PRIVATE})
    target_compile_definitions(${RTAUDIO_PLUGIN_TARGET} INTERFACE ${PLUGIN_DEFINITIONS_INTERFACE})
endif()

# Set plugin output dir
set_target_properties(${RTAUDIO_PLUGIN_TARGET} PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${PLUGIN_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${PLUGIN_OUTPUT_DIR}
)

# Link libraries
target_link_libraries(${RTAUDIO_PLUGIN_TARGET} PRIVATE ${RTAUDIO_PLUGIN_LIBS})
smtg_setup_universal_binary(${RTAUDIO_PLUGIN_TARGET})

#Apps
option(BUILD_LOOPER_APP "Build Looper app")
if(BUILD_LOOPER_APP)
  add_executable(Looper "${SOURCE_DIR}/Looper.cpp")
  
  target_link_libraries(Looper 
    Showtime::Showtime
    Boost::thread
  )

  if(WIN32)
    set_target_properties(Looper PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
  endif()

  add_dependencies(Looper ${RTAUDIO_PLUGIN_TARGET})
endif()
