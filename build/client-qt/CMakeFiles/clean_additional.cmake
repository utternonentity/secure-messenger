# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "CMakeFiles/sm_client_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/sm_client_autogen.dir/ParseCache.txt"
  "sm_client_autogen"
  )
endif()
