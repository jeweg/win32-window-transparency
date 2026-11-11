cmake_minimum_required(VERSION 3.20)
project(win32_window_demos)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

add_compile_definitions(UNICODE _UNICODE)

add_executable(01_basic_opaque           WIN32 01_basic_opaque.cpp)
add_executable(02_popup_no_decorations   WIN32 02_popup_no_decorations.cpp)
add_executable(03_win11_backdrop         WIN32 03_win11_backdrop.cpp)
add_executable(04_layered_uniform_alpha  WIN32 04_layered_uniform_alpha.cpp)
add_executable(05_perpixel_alpha         WIN32 05_perpixel_alpha.cpp)
add_executable(06_dwm_opt_in             WIN32 06_dwm_opt_in.cpp)
add_executable(07_gdi_alpha                    07_gdi_alpha.cpp)
add_executable(08_perpixel_alpha_patblt  WIN32 08_perpixel_alpha_patblt.cpp)
add_executable(09_nothing_paints               09_nothing_paints.cpp)

add_executable(10_glfw_transparency_bug 10_glfw_transparency_bug.cpp)
target_link_libraries(10_glfw_transparency_bug glfw)

add_executable(11_premultiplied_alpha    WIN32 11_premultiplied_alpha.cpp)
add_executable(12_dxgi_present                 12_dxgi_present.cpp)

find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    add_executable(13_vulkan_perpixel_alpha WIN32 13_vulkan_perpixel_alpha.cpp)
    target_link_libraries(13_vulkan_perpixel_alpha Vulkan::Vulkan)
    target_compile_definitions(13_vulkan_perpixel_alpha PRIVATE VK_USE_PLATFORM_WIN32_KHR)
else()
    message(STATUS "Vulkan SDK not found -- skipping 13_vulkan_perpixel_alpha")
endif()

add_executable(14_opengl_perpixel_alpha 14_opengl_perpixel_alpha.cpp)
