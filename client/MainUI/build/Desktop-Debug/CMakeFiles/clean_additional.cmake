# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/MainUI_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/MainUI_autogen.dir/ParseCache.txt"
  "MainUI_autogen"
  )
endif()
