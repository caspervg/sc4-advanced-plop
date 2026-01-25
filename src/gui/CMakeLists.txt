set(GUI_NAME SC4AdvancedLotPlopGui)

# Collect sources
file(GLOB GUI_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

file(GLOB PANEL_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/*.cpp
)

# Vendor directories
set(RLIMGUI_DIR "${CMAKE_SOURCE_DIR}/vendor/rlImGui")
set(IMGUIFILEDIALOG_DIR "${CMAKE_SOURCE_DIR}/vendor/ImGuiFileDialog")

add_executable(${GUI_NAME}
    ${GUI_SOURCES}
    ${PANEL_SOURCES}
    ${RLIMGUI_DIR}/rlImGui.cpp
    ${IMGUIFILEDIALOG_DIR}/ImGuiFileDialog.cpp
)

target_compile_definitions(${GUI_NAME} PRIVATE
    NOMINMAX
    SC4_ADVANCED_LOT_PLOP_VERSION="${PROJECT_VERSION}"
)

target_include_directories(${GUI_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/app
    ${CMAKE_SOURCE_DIR}/src/app/services
    ${RLIMGUI_DIR}
    ${IMGUIFILEDIALOG_DIR}
    ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
)

target_link_libraries(${GUI_NAME} PRIVATE
    AppServices
    SC4AdvancedLotPlopCore
    imgui::imgui
    raylib
    spdlog::spdlog
    DBPFKitLib
    pugixml::pugixml
)

# Windows-specific: Link required libraries
if(WIN32)
    target_compile_definitions(${GUI_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        NOGDI
        NOUSER
        MMNOSOUND
    )

    target_link_options(${GUI_NAME} PRIVATE
        /FORCE:MULTIPLE
        /IGNORE:4006,4221
    )

    target_link_libraries(${GUI_NAME} PRIVATE opengl32 gdi32 user32 winmm)
endif()

# Copy DLLs to output directory for execution (only for dynamic triplets)
if(MSVC)
    set(VCPKG_RELEASE_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")

    if(EXISTS "${VCPKG_RELEASE_BIN_DIR}")
        set(VCPKG_DEBUG_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin")

        # Copy DLLs to executable directory based on build configuration
        add_custom_command(TARGET ${GUI_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<IF:$<CONFIG:Debug>,${VCPKG_DEBUG_BIN_DIR},${VCPKG_RELEASE_BIN_DIR}>"
                "$<TARGET_FILE_DIR:${GUI_NAME}>"
            COMMENT "Copying $<IF:$<CONFIG:Debug>,debug,release> DLLs to executable directory"
            VERBATIM
        )
    endif()
endif()

install(TARGETS ${GUI_NAME}
    RUNTIME DESTINATION bin
)
