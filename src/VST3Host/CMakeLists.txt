# Get VST3 sdk
FetchContent_Declare(vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk
    GIT_TAG v3.7.3_build_20
)
FetchContent_MakeAvailable(vst3sdk)

set(ZST_AUDIO_PLUGIN_HEADERS
  "${CMAKE_CURRENT_LIST_DIR}/AudioVSTHost.h"
  "${CMAKE_CURRENT_LIST_DIR}/AudioVSTFactory.h"
  "${CMAKE_CURRENT_LIST_DIR}/WindowController.h"
  "${CMAKE_CURRENT_LIST_DIR}/VSTPlugProvider.h"
  "${CMAKE_CURRENT_LIST_DIR}/platform/iwindow.h"
  "${CMAKE_CURRENT_LIST_DIR}/platform/iplatform.h"
  "${CMAKE_CURRENT_LIST_DIR}/platform/iapplication.h"
)

set(ZST_AUDIO_PLUGIN_SRC
  "${CMAKE_CURRENT_LIST_DIR}/AudioVSTHost.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/AudioVSTFactory.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/VSTPlugProvider.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/WindowController.cpp"
  "${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/plugprovider.cpp"
)

if(WIN32)
  list(APPEND ZST_AUDIO_PLUGIN_SRC 
    "${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/module_win32.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/platform/win32/window.h"
    "${CMAKE_CURRENT_LIST_DIR}/platform/win32/window.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/platform/win32/platform.cpp"
  )
endif()

target_sources(${AUDIO_PLUGIN_TARGET} PRIVATE 
    ${ZST_AUDIO_PLUGIN_HEADERS}
    ${ZST_AUDIO_PLUGIN_SRC}
)
# target_include_directories(${AUDIO_PLUGIN_TARGET} PRIVATE ${vst3sdk_INCLUDE_DIRS})

target_link_libraries(${AUDIO_PLUGIN_TARGET} PRIVATE 
    sdk_hosting
    sdk_common
    base
    pluginterfaces
)

smtg_setup_universal_binary(${AUDIO_PLUGIN_TARGET})
