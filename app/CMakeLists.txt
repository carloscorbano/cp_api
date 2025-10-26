cmake_minimum_required(VERSION 3.10.0)
project(app VERSION 0.1.0 LANGUAGES C CXX)

add_executable(app main.cpp)

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
include(${CMAKE_SOURCE_DIR}/cmake/copy_dlls.cmake)
copy_dlls(app)

set_target_properties(app PROPERTIES
    OUTPUT_NAME_DEBUG   "game_d"
    OUTPUT_NAME_RELEASE "game"
)

target_link_libraries(app PRIVATE cp_api)