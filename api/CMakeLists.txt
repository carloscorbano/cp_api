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
    src/core/algorithm.cpp
    src/core/security.cpp
    src/core/compression.cpp
    src/core/filesystem.cpp
    src/core/threadPool.cpp
    src/core/serializable.cpp
    src/core/stb.inc.cpp

    src/window/glfw.inc.cpp
    src/window/window.cpp
    src/window/inputManager.cpp
    src/window/vulkan.cpp
    src/window/vma.inc.cpp
    src/window/vkBuffer.cpp
    src/window/vkImage.cpp
    src/window/vkTexture.cpp
    src/window/imgui.inc.cpp
    src/window/renderer.cpp
    
    src/containers/ntree.cpp
    src/containers/spatialTree.cpp

    src/physics/spatialTree2D.cpp
    src/physics/spatialTree3D.cpp
    src/physics/terrainCollision.cpp

    src/world/world.cpp
)

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

target_include_directories(cp_api PUBLIC include)

find_package(fmt CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(MbedTLS CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Vulkan)
find_package(VulkanHeaders CONFIG)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

# find_package(OpenGL REQUIRED) #for test usage only

target_link_libraries(cp_api 
    PRIVATE
        MbedTLS::mbedtls
        nlohmann_json::nlohmann_json
        Vulkan::Vulkan Vulkan::Headers GPUOpen::VulkanMemoryAllocator
        ZLIB::ZLIB
        EnTT::EnTT
        glfw
        imgui::imgui
        # OpenGL::GL
    PUBLIC
        fmt::fmt
        glm::glm
)

target_include_directories(cp_api PRIVATE ${Stb_INCLUDE_DIR})