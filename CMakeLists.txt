# Set versions
cmake_minimum_required(VERSION 3.1)
set(CMAKE_CXX_STANDARD 17)

# Set project name
project(rtx)

IF(CMAKE_BUILD_TYPE STREQUAL "RELEASE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG -O3")
ELSE()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
ENDIF()

set(SOURCES Source/main.cpp Source/SauronLT.h Source/SauronLT.cpp)

set(GLFW_DIR Libraries/glfw)
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
option(GLFW_DOCUMENT_INTERNALS "Include internals in documentation" OFF)
add_subdirectory(${GLFW_DIR})
include_directories(${GLFW_DIR}/include)
include_directories(Libraries/glm/)
include_directories(Libraries/stb/)
include_directories(Source)

set(IMGUI_DIR Libraries/imgui)
include_directories(${IMGUI_DIR} ${IMGUI_DIR}/backends)

find_package(Vulkan REQUIRED)
set(LIBRARIES "glfw;${Vulkan_LIBRARY}")

include_directories(${Vulkan_INCLUDE_DIR})

set(IMGUI_SOURCES ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_demo.cpp ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp Source/SauronLT.h Source/SauronIMG.cpp Source/SauronIMG.h)

add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES})
target_link_libraries(${PROJECT_NAME} ${LIBRARIES})

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU") # GCC / MinGW
    target_link_libraries(${PROJECT_NAME} -static-libgcc -static-libstdc++)
endif()

IF (WIN32)
    target_link_libraries(${PROJECT_NAME} -static winpthread)
ENDIF()