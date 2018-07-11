cmake_minimum_required(VERSION 2.8)

project(WakeyWakey C)

SET(LIBSUPERDERPY_GAMENAME "wakeywakey" CACHE INTERNAL "")
SET(LIBSUPERDERPY_GAMENAME_PRETTY "Wakey Wakey" CACHE INTERNAL "")
SET(LIBSUPERDERPY_ORIENTATION "LANDSCAPE" CACHE INTERNAL "")
SET(LIBSUPERDERPY_VERSION "1.0" CACHE INTERNAL "")
#add_definitions(-DLIBSUPERDERPY_SINGLE_THREAD=1)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_SOURCE_DIR}/libsuperderpy/cmake")

include(libsuperderpy)
include(SetPaths)

set(EMSCRIPTEN_TOTAL_MEMORY "512")

add_subdirectory(libsuperderpy)
add_subdirectory(src)
add_subdirectory(data)

# uninstall target
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libsuperderpy/cmake/cmake_uninstall.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" IMMEDIATE @ONLY)
add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
