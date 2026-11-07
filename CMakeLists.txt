cmake_minimum_required(VERSION 3.25)

project(HyprEndfield VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Qml Quick)
find_package(LayerShellQt REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(hyprendfield
    src/main.cpp
    src/applicationcontroller.cpp
    src/applicationcontroller.hpp
)

qt_add_qml_module(hyprendfield
    URI HyprEndfield
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/WatchShell.qml
        qml/SystemPage.qml
        qml/HudSettingsRow.qml
        qml/HudNavButton.qml
        qml/HudBottomButton.qml
        qml/HudGlyph.qml
)

qt_add_resources(hyprendfield "hyprendfield_assets"
    PREFIX "/"
    FILES
        assets/watch/ui_main_loop_bg.png
        assets/watch/ui_out_ring.png
        assets/watch/ui_out1_ring.png
        assets/watch/ui_mid_ring.png
        assets/watch/ui_triangle_fx.png
        assets/watch/deco_04.png
        assets/watch/endfield_industries.png
        assets/watch/SOURCES.json
        assets/icons/notes.png
        assets/icons/file-shelf.png
        assets/icons/clipboard.png
        assets/icons/archive.png
        assets/icons/reader.png
        assets/icons/work-mode.png
        assets/icons/calendar.png
        assets/icons/map.png
        assets/icons/event-log.png
        assets/icons/SOURCES.json
        assets/fonts/HarmonyOS_Sans_Regular.ttf
        assets/fonts/HarmonyOS_Sans_Medium.ttf
        assets/fonts/HarmonyOS_Sans_Bold.ttf
        assets/fonts/HarmonyOS-Sans-LICENSE.txt
)

target_link_libraries(hyprendfield PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    LayerShellQt::Interface
)

target_compile_options(hyprendfield PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

set_target_properties(hyprendfield PROPERTIES
    OUTPUT_NAME HyprEndfield
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

include(GNUInstallDirs)
install(TARGETS hyprendfield
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
