cmake_minimum_required(VERSION 3.13)

project(ShowtimePluginAudio VERSION 0.0.1)

# Use C14
set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED)

message(STATUS "Configuring audio plugin")

set(AUDIO_PLUGIN_TARGET "ShowtimePluginAudio")

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

# Build options
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
  list(APPEND BOOST_COMPONENTS unit_test_framework)
  add_subdirectory(tests)
endif()

# Get dependencies
find_package(Showtime CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# Get boost
include(FetchContent)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(build_boost
    GIT_REPOSITORY https://github.com/boostorg/boost.git
    GIT_TAG boost-1.79.0.beta1
)
FetchContent_GetProperties(build_boost)
if(NOT build_boost_POPULATED)
    FetchContent_Populate(build_boost)
    add_subdirectory(${build_boost_SOURCE_DIR} ${build_boost_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Create Library
add_library(${AUDIO_PLUGIN_TARGET} MODULE)

# Set linked libs
set(PLUGIN_LINK_LIBS 
    Boost::thread
    Boost::dll
    Boost::bimap
    Boost::circular_buffer
    Showtime::Showtime
)

# Plugin source files
set(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/src")
set(ZST_AUDIO_PLUGIN_HEADERS
  "${SOURCE_DIR}/plugin.h"
  "${SOURCE_DIR}/AudioSpinMutex.h"
  "${SOURCE_DIR}/AudioComponentBase.h"
  "${SOURCE_DIR}/ringbuffer.h"
)
set(ZST_AUDIO_PLUGIN_SRC
  "${SOURCE_DIR}/plugin.cpp"
  "${SOURCE_DIR}/AudioComponentBase.cpp"
  "${SOURCE_DIR}/ringbuffer.cpp"
)

# Plugin compile defs
set(PLUGIN_DEFINITIONS_PRIVATE "-DZST_EXPORT_PLUGIN_API")
set(PLUGIN_DEFINITIONS_INTERFACE "-DZST_IMPORT_PLUGIN_API")

# Audio library implementations
add_subdirectory(src/AudioDevices)
add_subdirectory(src/VST3Host)
add_subdirectory(src/VST2Host)

# Include files in target
target_sources(${AUDIO_PLUGIN_TARGET} PRIVATE 
    ${ZST_AUDIO_PLUGIN_HEADERS}
    ${ZST_AUDIO_PLUGIN_SRC}
)

# Set compiler definitions
if(WIN32)
    target_compile_definitions(${AUDIO_PLUGIN_TARGET} PRIVATE ${PLUGIN_DEFINITIONS_PRIVATE})
    target_compile_definitions(${AUDIO_PLUGIN_TARGET} INTERFACE ${PLUGIN_DEFINITIONS_INTERFACE})
endif()

# Set plugin output dir
set_target_properties(${AUDIO_PLUGIN_TARGET} PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${PLUGIN_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${PLUGIN_OUTPUT_DIR}
)

# Link libraries
target_link_libraries(${AUDIO_PLUGIN_TARGET} PRIVATE ${PLUGIN_LINK_LIBS})
add_dependencies(${AUDIO_PLUGIN_TARGET} ${PLUGIN_LINK_LIBS})

#Apps
option(BUILD_LOOPER_APP "Build Looper app")
if(BUILD_LOOPER_APP)
  add_executable(Looper "${CMAKE_CURRENT_LIST_DIR}/apps/Looper.cpp")
  
  target_link_libraries(Looper 
    Showtime::Showtime
    Boost::thread
  )

  if(WIN32)
    set_target_properties(Looper PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
  endif()

  add_dependencies(Looper ${AUDIO_PLUGIN_TARGET})
endif()
