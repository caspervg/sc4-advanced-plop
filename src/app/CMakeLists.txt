set(APP_NAME SC4AdvancedLotPlopCli)
set(SERVICES_LIB_NAME AppServices)

# Collect service files
file(GLOB SERVICE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/services/*.cpp
)

# Embed PropertyMapper XML
include(${CMAKE_SOURCE_DIR}/cmake/EmbedFile.cmake)
set(PROPERTY_MAPPER_XML "${CMAKE_SOURCE_DIR}/vendor/sc4-properties/new_properties.xml")
set(EMBEDDED_PROPERTY_MAPPER_H "${CMAKE_CURRENT_BINARY_DIR}/EmbeddedPropertyMapper.hpp")
embed_file("${PROPERTY_MAPPER_XML}" "${EMBEDDED_PROPERTY_MAPPER_H}" "PROPERTY_MAPPER_XML")

# Create AppServices library
add_library(${SERVICES_LIB_NAME} STATIC ${SERVICE_SOURCES})

target_compile_definitions(${SERVICES_LIB_NAME} PUBLIC
    NOMINMAX
)

if(WIN32)
    target_compile_definitions(${SERVICES_LIB_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        NOGDI
        NOUSER
        MMNOSOUND
    )
endif()

target_include_directories(${SERVICES_LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/services
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
)

target_link_libraries(${SERVICES_LIB_NAME} PUBLIC
    spdlog::spdlog
    SC4AdvancedLotPlopCore
    DBPFKitLib
    pugixml::pugixml
    raylib
)

# Collect source files for CLI executable (excluding services)
file(GLOB APP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(${APP_NAME} ${APP_SOURCES})

target_compile_definitions(${APP_NAME} PRIVATE NOMINMAX)
target_compile_definitions(${APP_NAME}
    PRIVATE
        SC4_ADVANCED_LOT_PLOP_VERSION="${PROJECT_VERSION}"
)

target_include_directories(${APP_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
)

target_link_libraries(${APP_NAME} PRIVATE
    ${SERVICES_LIB_NAME}
    spdlog::spdlog
    SC4AdvancedLotPlopCore
    taywee::args
    DBPFKitLib
    pugixml::pugixml
    jsoncons::jsoncons
    raylib
    glfw
)

if (WIN32)
    target_compile_definitions(${APP_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        NOGDI
        NOUSER
        MMNOSOUND
    )

    target_link_options(${APP_NAME} PRIVATE
        /FORCE:MULTIPLE
        /IGNORE:4006,4221
    )

    target_link_libraries(${APP_NAME} PRIVATE winmm opengl32 gdi32 user32)
endif()

# Copy DLLs to output directory for execution (only for dynamic triplets)
if(MSVC)
    # vcpkg stores debug DLLs in debug/bin and release DLLs in bin
    # Static triplets (like x86-windows-static-md) don't have a bin directory
    set(VCPKG_RELEASE_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")

    if(EXISTS "${VCPKG_RELEASE_BIN_DIR}")
        set(VCPKG_DEBUG_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin")

        # Copy DLLs to executable directory based on build configuration
        add_custom_command(TARGET ${APP_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<IF:$<CONFIG:Debug>,${VCPKG_DEBUG_BIN_DIR},${VCPKG_RELEASE_BIN_DIR}>"
                "$<TARGET_FILE_DIR:${APP_NAME}>"
            COMMENT "Copying $<IF:$<CONFIG:Debug>,debug,release> DLLs to executable directory"
            VERBATIM
        )
    endif()
endif()

install(TARGETS ${APP_NAME}
    RUNTIME DESTINATION bin
)
