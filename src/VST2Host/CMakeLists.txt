# Get FST sdk
FetchContent_Declare(fst 
  GIT_REPOSITORY  https://git.iem.at/zmoelnig/FST.git
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
)
FetchContent_GetProperties(fst)
if(NOT fst)
  FetchContent_Populate(fst)
  include_directories(${fst_SOURCE_DIR}) #EXCLUDE_FROM_ALL
endif()

