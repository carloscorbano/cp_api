cmake_minimum_required(VERSION 3.10.0)
project(cp_api VERSION 0.1.0 LANGUAGES C CXX)

add_library(cp_api STATIC 
    src/framework.cpp
    src/core/debug.cpp    
    src/core/diagnostics.cpp
    src/core/events.cpp
    src/core/math.cpp
    src/core/delegate.cpp
    src/core/hybridEvents.cpp
)

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

target_include_directories(cp_api PUBLIC include)

find_package(fmt CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

target_link_libraries(cp_api 
    PUBLIC
        fmt::fmt
        glm::glm
)