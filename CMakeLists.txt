cmake_minimum_required(VERSION 3.18)

# Set the default before project() initializes this cache entry from the env.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands for clangd")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" COOMER_VERSION LIMIT_COUNT 1)
project(coomer VERSION "${COOMER_VERSION}" LANGUAGES C CXX)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(COOMER_X11 "Enable X11 capture and windows" ON)
option(COOMER_WAYLAND "Enable Wayland capture and windows" ON)
option(COOMER_PORTAL "Enable the Screenshot portal backend" ON)

include(GNUInstallDirs)
include(CTest)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(GRAPHICS REQUIRED IMPORTED_TARGET gl egl)

add_library(coomer_glad STATIC third_party/glad/gl.c)
target_include_directories(coomer_glad PUBLIC third_party)
target_link_libraries(coomer_glad PUBLIC ${CMAKE_DL_LIBS})

add_library(coomer_renderer STATIC src/render/RendererGL.cpp)
target_include_directories(coomer_renderer PUBLIC src)
target_link_libraries(coomer_renderer PUBLIC coomer_glad)

add_executable(coomer
    src/app/main.cpp
    src/app/cli.cpp
    src/capture/BackendAuto.cpp)
target_compile_definitions(coomer PRIVATE COOMER_VERSION="${PROJECT_VERSION}")
target_link_libraries(coomer PRIVATE
    coomer_renderer PkgConfig::GRAPHICS Threads::Threads m)

if(COOMER_X11)
    pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11 xrandr)
    target_sources(coomer PRIVATE
        src/capture/BackendX11.cpp src/window/X11WindowGlx.cpp)
    target_compile_definitions(coomer PRIVATE COOMER_HAS_X11)
    target_link_libraries(coomer PRIVATE PkgConfig::X11)
endif()

if(COOMER_WAYLAND)
    pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET
        wayland-client wayland-egl xkbcommon)
    find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
    set(PROTOCOL_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${PROTOCOL_DIR}")
    set(PROTOCOL_SOURCES)
    foreach(PROTOCOL IN ITEMS
        fractional-scale-v1
        viewporter
        wlr-layer-shell-unstable-v1
        wlr-screencopy-unstable-v1
        xdg-output-unstable-v1
        xdg-shell)
        set(XML "${CMAKE_CURRENT_SOURCE_DIR}/protocols/${PROTOCOL}.xml")
        set(CODE "${PROTOCOL_DIR}/${PROTOCOL}-protocol.c")
        set(HEADER "${PROTOCOL_DIR}/${PROTOCOL}-client-protocol.h")
        add_custom_command(
            OUTPUT "${CODE}" "${HEADER}"
            COMMAND "${WAYLAND_SCANNER}" private-code "${XML}" "${CODE}"
            COMMAND "${WAYLAND_SCANNER}" client-header "${XML}" "${HEADER}"
            DEPENDS "${XML}"
            VERBATIM)
        list(APPEND PROTOCOL_SOURCES "${CODE}" "${HEADER}")
    endforeach()
    add_library(coomer_protocols STATIC ${PROTOCOL_SOURCES})
    target_include_directories(coomer_protocols PUBLIC "${PROTOCOL_DIR}")
    target_link_libraries(coomer_protocols PUBLIC PkgConfig::WAYLAND)
    target_sources(coomer PRIVATE
        src/capture/BackendWlrScreencopy.cpp
        src/window/WaylandWindowXdgEgl.cpp
        src/window/WaylandWindowLayerShellEgl.cpp)
    target_compile_definitions(coomer PRIVATE COOMER_HAS_WAYLAND)
    target_link_libraries(coomer PRIVATE coomer_protocols)
    add_dependencies(coomer coomer_protocols)
endif()

if(COOMER_PORTAL)
    pkg_check_modules(DBUS REQUIRED IMPORTED_TARGET dbus-1)
    target_sources(coomer PRIVATE src/capture/BackendPortalScreenshot.cpp)
    target_compile_definitions(coomer PRIVATE COOMER_HAS_PORTAL)
    target_link_libraries(coomer PRIVATE PkgConfig::DBUS)
endif()

if(BUILD_TESTING)
    foreach(TEST IN ITEMS overview render clipboard)
        add_executable(test_${TEST} tests/${TEST}.cpp)
        target_include_directories(test_${TEST} PRIVATE src)
        # These tests use assertions for both checks and context setup, also
        # when the application is built with Release/RelWithDebInfo flags.
        target_compile_options(test_${TEST} PRIVATE -UNDEBUG)
        add_test(NAME ${TEST} COMMAND test_${TEST})
    endforeach()
    target_link_libraries(test_render PRIVATE coomer_renderer PkgConfig::GRAPHICS)
endif()

install(TARGETS coomer RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES completions/bash/coomer
    DESTINATION "${CMAKE_INSTALL_DATADIR}/bash-completion/completions")
install(FILES completions/zsh/_coomer
    DESTINATION "${CMAKE_INSTALL_DATADIR}/zsh/site-functions")
install(FILES completions/fish/coomer.fish
    DESTINATION "${CMAKE_INSTALL_DATADIR}/fish/vendor_completions.d")
install(FILES packaging/linux/coomer.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES packaging/linux/coomer.svg
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
install(FILES packaging/linux/io.github.yuzujr.coomer.metainfo.xml
    DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
