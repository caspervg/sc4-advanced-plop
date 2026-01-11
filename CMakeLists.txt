cmake_minimum_required(VERSION 3.20)

# Force 32-bit build for SimCity 4 compatibility BEFORE project() call
# Set the platform regardless of current setting to ensure 32-bit
set(CMAKE_GENERATOR_PLATFORM "Win32")
message(STATUS "Forcing 32-bit build for SC4 compatibility")
#
## Force release runtime BEFORE project() call - this is critical
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
set(CMAKE_CXX_FLAGS_DEBUG "/MD /Zi /Ob0 /Od")
set(CMAKE_C_FLAGS_DEBUG "/MD /Zi /Ob0 /Od")

# Additional 32-bit enforcement
if(WIN32)
    set(CMAKE_SIZEOF_VOID_P 4)
    set(CMAKE_C_SIZEOF_DATA_PTR 4)
    set(CMAKE_CXX_SIZEOF_DATA_PTR 4)
endif()

project(SC4AdvancedLotPlop VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform specific settings
if(WIN32)
    add_definitions(-DWIN32 -D_WINDOWS -DUNICODE -D_UNICODE)
endif()

# MSVC specific settings
if(MSVC)
    add_compile_options(/W4 /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    
    # Force release runtime for all configurations
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    
    # Override debug flags to use release runtime
    set(CMAKE_CXX_FLAGS_DEBUG "/MD /Zi /Ob0 /Od /RTC1")
    set(CMAKE_C_FLAGS_DEBUG "/MD /Zi /Ob0 /Od /RTC1")
    
    # Enable debugging in release builds
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
endif()

add_subdirectory(vendor/spdlog)
add_subdirectory(vendor/mINI)

# ImGui library (vendor/imgui has no CMakeLists)
# DirectXTK (DirectX Tool Kit 11) - disable unnecessary features
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_XAUDIO_WIN10 OFF CACHE BOOL "" FORCE)
set(BUILD_XAUDIO_WIN8 OFF CACHE BOOL "" FORCE)
set(BUILD_XAUDIO_REDIST OFF CACHE BOOL "" FORCE)
set(BUILD_GAMEINPUT OFF CACHE BOOL "" FORCE)
set(BUILD_WGI OFF CACHE BOOL "" FORCE)
set(BUILD_XINPUT OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/DirectXTK)

include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

# SQLite3 for persistent caching
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3460000.zip
)
FetchContent_MakeAvailable(sqlite3)

# Build SQLite3 as a static library
if(NOT TARGET sqlite3)
    add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
    target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
    target_compile_definitions(sqlite3 PRIVATE
        SQLITE_THREADSAFE=1
        SQLITE_ENABLE_COLUMN_METADATA
        SQLITE_ENABLE_FTS5
        SQLITE_ENABLE_JSON1
        SQLITE_ENABLE_RTREE
    )
    # Explicitly set linker language to C
    set_target_properties(sqlite3 PROPERTIES LINKER_LANGUAGE C)
endif()

# ImGui service (DX7) headers and import library
set(IMGUI_SERVICE_ROOT "${CMAKE_SOURCE_DIR}/../sc4-imgui-backend" CACHE PATH "Path to sc4-imgui-backend")
set(IMGUI_PUBLIC_DIR "${IMGUI_SERVICE_ROOT}/src")
set(IMGUI_INCLUDE_DIR "${IMGUI_SERVICE_ROOT}/vendor/d3d7imgui/ImGui")
set(IMGUI_LIB_DIR "${IMGUI_SERVICE_ROOT}" CACHE PATH "Path containing imgui.lib from sc4-imgui-backend build")

# GZCOM-DLL (vendor/gzcom-dll has sources, build local static lib)
set(GZCOM_DIR ${CMAKE_SOURCE_DIR}/vendor/gzcom-dll/gzcom-dll)

# Collect all public headers and sources
file(GLOB_RECURSE GZCOM_HEADERS
    ${GZCOM_DIR}/include/*.h
    ${GZCOM_DIR}/include/*.hpp
)
file(GLOB_RECURSE GZCOM_SOURCES
    ${GZCOM_DIR}/src/*.cpp
)

# Exclude vendor app entry and example directors from gzcom sources
list(REMOVE_ITEM GZCOM_SOURCES
    ${GZCOM_DIR}/src/main.cpp
    ${GZCOM_DIR}/src/cGZExampleDllDirector.cpp
    ${GZCOM_DIR}/src/cGZExtraCheatsDllDirector.cpp
    ${GZCOM_DIR}/src/cGZExtraExtraCheatsDllDirector.cpp
    ${GZCOM_DIR}/src/cGZCityHallUpgradeDllDirector.cpp
)

# Build gzcom2 as a static library to match existing linkage
add_library(gzcom2 STATIC ${GZCOM_SOURCES} ${GZCOM_HEADERS})

# Expose include path to dependents
target_include_directories(gzcom2 PUBLIC
    ${GZCOM_DIR}/include
)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/vendor/DirectXTK/Inc
    ${IMGUI_PUBLIC_DIR}
    ${IMGUI_INCLUDE_DIR}
)

# Our source files
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)
list(APPEND PROJECT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/lots/LotPlopPanel.cpp
    ${CMAKE_SOURCE_DIR}/src/props/PropPainterPanel.cpp
)
list(REMOVE_ITEM PROJECT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/utils/D3D11Hook.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/ImGuiLifecycleManager.cpp
)

# Create the main DLL with explicit exports
add_library(${PROJECT_NAME} SHARED ${PROJECT_SOURCES} ${HEADERS} SC4AdvancedLotPlop.def)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    spdlog
    mINI
    gzcom2
    nlohmann_json
    DirectXTK
    sqlite3
)

file(GLOB_RECURSE IMGUI_LIB_CANDIDATES
    "${IMGUI_LIB_DIR}/imgui.lib"
    "${IMGUI_LIB_DIR}/*/imgui.lib"
    "${IMGUI_LIB_DIR}/*/*/imgui.lib"
    "${IMGUI_LIB_DIR}/*/*/*/imgui.lib"
)
list(LENGTH IMGUI_LIB_CANDIDATES IMGUI_LIB_COUNT)
if(IMGUI_LIB_COUNT GREATER 0)
    list(GET IMGUI_LIB_CANDIDATES 0 IMGUI_LIB)
    target_link_libraries(${PROJECT_NAME} PRIVATE "${IMGUI_LIB}")
    message(STATUS "Using imgui.lib: ${IMGUI_LIB}")
else()
    message(FATAL_ERROR "imgui.lib not found. Set IMGUI_LIB_DIR to the directory containing imgui.lib from sc4-imgui-backend.")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/vendor/wil/include)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE kernel32 user32 Version d3d11 dxgi windowscodecs ddraw dxguid)
endif()

# Set output properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "SC4AdvancedLotPlop"
    SUFFIX ".dll"
    PREFIX ""
)

# Force Release runtime even in Debug builds to avoid debug runtime dependency issues
if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
    )
endif()

# Automatic deployment to SC4 directories
if(WIN32)
    # Try to find SC4 installation directory
    set(SC4_INSTALL_PATHS
        "C:/Program Files (x86)/SimCity 4 Deluxe Edition"
        "C:/Program Files (x86)/Maxis/SimCity 4 Deluxe"
        "C:/Program Files/Maxis/SimCity 4 Deluxe"
        "C:/Program Files (x86)/Steam/steamapps/common/SimCity 4 Deluxe"
        "C:/Program Files/Steam/steamapps/common/SimCity 4 Deluxe"
    )
    
    foreach(SC4_PATH ${SC4_INSTALL_PATHS})
        if(EXISTS "${SC4_PATH}/Apps/SimCity 4.exe")
            set(SC4_INSTALL_DIR "${SC4_PATH}")
            break()
        endif()
    endforeach()
    
    # Get user's Documents folder
    set(USERPROFILE_DIR "$ENV{USERPROFILE}")
    set(SC4_PLUGINS_DIR "${USERPROFILE_DIR}/Documents/SimCity 4/Plugins")
    
    if(SC4_INSTALL_DIR)
        message(STATUS "Found SC4 installation at: ${SC4_INSTALL_DIR}")
        
        # Copy main DLL to user's Plugins folder
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${SC4_PLUGINS_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${PROJECT_NAME}>
            "${SC4_PLUGINS_DIR}/"
            COMMENT "Deploying SC4AdvancedLotPlop.dll to Plugins folder"
        )
        
        message(STATUS "Deployment targets configured:")
        message(STATUS "  - SC4AdvancedLotPlop.dll -> ${SC4_PLUGINS_DIR}/")
    else()
        message(WARNING "SC4 installation not found. Manual deployment required.")
    endif()
endif()

# Install targets
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY docs/
    DESTINATION docs
    FILES_MATCHING PATTERN "*"
)
