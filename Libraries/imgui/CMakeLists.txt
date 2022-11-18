add_library(
        imgui
        imgui.cpp
        imgui_demo.cpp
        imgui_draw.cpp
        imgui_widgets.cpp
        imgui_tables.cpp
        backends/imgui_impl_vulkan.cpp
        backends/imgui_impl_glfw.cpp
)

target_link_libraries(imgui PUBLIC glfw ${VULKAN_LIBRARY})

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

target_include_directories(
        imgui
        PUBLIC
        imgui
        imgui/backends
)