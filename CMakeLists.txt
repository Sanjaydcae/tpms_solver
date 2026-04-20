cmake_minimum_required(VERSION 3.25)
project(tpms_solver VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies via FetchContent (tarball — no git required) ─────────────────

include(FetchContent)

# GLFW — prefer system package (install via install_deps.sh), source as fallback
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(GLFW glfw3 QUIET)
endif()
find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND AND NOT GLFW_FOUND)
    message(STATUS "System GLFW not found — downloading source tarball...")
    FetchContent_Declare(glfw
        URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
    )
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND  OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11      ON  CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

# Dear ImGui — docking branch (docking, DockBuilder, etc. live here, not in release tags)
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/heads/docking.tar.gz
)
FetchContent_MakeAvailable(imgui)

# ImGui static library (OpenGL3 + GLFW backends)
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw)

# nlohmann/json — header-only JSON library for project save/load
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install   OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# OpenGL
find_package(OpenGL REQUIRED)

# MPI — VTK-targets.cmake requires MPI::MPI_C to exist; create a stub if not found
find_package(MPI QUIET COMPONENTS C)
if(NOT MPI_FOUND)
    add_library(MPI::MPI_C INTERFACE IMPORTED GLOBAL)
    target_include_directories(MPI::MPI_C INTERFACE
        /usr/lib/x86_64-linux-gnu/openmpi/include)
    target_link_libraries(MPI::MPI_C INTERFACE
        /usr/lib/x86_64-linux-gnu/libmpi.so)
endif()

# VTK 9.1 — system package (sudo apt install libvtk9-dev)
find_package(VTK 9.1 REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    FiltersGeometry
    FiltersCore
    FiltersSources
    RenderingCore
    RenderingOpenGL2
    RenderingAnnotation
    InteractionStyle
    InteractionWidgets
    IOXML
)

# ── Main executable ───────────────────────────────────────────────────────────

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(tpms_solver ${SOURCES})

target_include_directories(tpms_solver PRIVATE src /usr/include/vtk-9.1)

target_link_libraries(tpms_solver PRIVATE
    imgui_lib
    OpenGL::GL
    nlohmann_json::nlohmann_json
    ${VTK_LIBRARIES}
)

vtk_module_autoinit(
    TARGETS tpms_solver
    MODULES ${VTK_MODULES_USED}
)

if(MSVC)
    target_compile_options(tpms_solver PRIVATE /W4)
else()
    target_compile_options(tpms_solver PRIVATE -Wall -Wextra -O2)
endif()

# Copy assets to build directory
add_custom_command(TARGET tpms_solver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:tpms_solver>/assets
    COMMENT "Copying assets..."
)
