# Get RtAudio package
include(FetchContent)
FetchContent_Declare(rtaudio
    GIT_REPOSITORY https://github.com/thestk/rtaudio
    GIT_TAG 5.1.0)
set(RTAUDIO_BUILD_SHARED_LIBS OFF)
set(RTAUDIO_BUILD_STATIC_LIBS ON)
FetchContent_MakeAvailable(rtaudio)

set(ZST_AUDIO_PLUGIN_HEADERS
  "${CMAKE_CURRENT_LIST_DIR}/AudioFactory.h"
  "${CMAKE_CURRENT_LIST_DIR}/AudioDevice.h"
)

set(ZST_AUDIO_PLUGIN_SRC
  "${CMAKE_CURRENT_LIST_DIR}/AudioFactory.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/AudioDevice.cpp"
)

target_sources(${AUDIO_PLUGIN_TARGET} PRIVATE 
    ${ZST_AUDIO_PLUGIN_HEADERS}
    ${ZST_AUDIO_PLUGIN_SRC}
)
target_include_directories(${AUDIO_PLUGIN_TARGET} PRIVATE ${rtaudio_SOURCE_DIR})

target_link_libraries(${AUDIO_PLUGIN_TARGET} PRIVATE rtaudio)
