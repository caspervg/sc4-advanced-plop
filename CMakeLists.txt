cmake_minimum_required(VERSION 3.20)

# Enable vcpkg manifest mode BEFORE project()
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
set(VCPKG_MANIFEST_MODE ON CACHE BOOL "Enable vcpkg manifest mode")
set(VCPKG_MANIFEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE STRING "vcpkg manifest directory")
set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed" CACHE STRING "vcpkg installed directory")

# Set MSVC runtime BEFORE project() to ensure it applies to all targets
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

# Allow proper runtime library selection per configuration
# Debug builds use /MDd (with debug DLLs from vcpkg debug/bin)
# Release builds use /MD (with release DLLs from vcpkg bin)
# This is handled automatically by CMAKE_MSVC_RUNTIME_LIBRARY per config

project(SC4AdvancedLotPlop VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

if(WIN32)
    add_definitions(-DWIN32 -D_WINDOWS -DUNICODE -D_UNICODE)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

    # Enable debugging in release builds
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
endif()

find_package(spdlog CONFIG REQUIRED)
find_package(args CONFIG REQUIRED)
find_package(reflectcpp CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(jsoncons CONFIG REQUIRED)
find_package(raylib CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

if (WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

include(FetchContent)

# Fetch compatible imgui version (docking branch)
set(IMGUI_VERSION 1.92.4-docking)
set(IMGUI_TAG e7d2d636affeeac1fc7175fb13472ca02d1bc0d1)
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG ${IMGUI_TAG}
)
FetchContent_MakeAvailable(imgui)
add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
add_library(imgui::imgui ALIAS imgui)
target_include_directories(imgui INTERFACE ${imgui_SOURCE_DIR})

# Fetch compatible rlImGui version
set(RLIMGUI_TAG 4d8a61842903978bc42adf3347cd34f4e6524efc)
if (NOT TARGET rlimgui)
    FetchContent_Declare(
            rlimgui
            GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
            GIT_TAG ${RLIMGUI_TAG}
    )
    FetchContent_MakeAvailable(rlimgui)
    add_library(rlimgui STATIC ${rlimgui_SOURCE_DIR}/rlImGui.cpp)
    target_include_directories(rlimgui PUBLIC ${rlimgui_SOURCE_DIR} PRIVATE ${raylib_SOURCE_DIR} ${imgui_SOURCE_DIR})
    target_link_libraries(rlimgui PUBLIC imgui raylib)
endif()
add_subdirectory(vendor/DBPFKit)
add_subdirectory(src/shared)
add_subdirectory(src/dll)
add_subdirectory(src/app)
add_subdirectory(src/gui)
add_subdirectory(tools)

if(EXISTS "${CMAKE_SOURCE_DIR}/docs")
    install(DIRECTORY docs/
        DESTINATION docs
        FILES_MATCHING PATTERN "*"
    )
endif()
