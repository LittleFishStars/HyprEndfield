cmake_minimum_required(VERSION 3.25)

project(HyprEndfield VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Qml Quick ShaderTools)
find_package(LayerShellQt REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(hyprendfield
    src/main.cpp
    src/applicationcontroller.cpp
    src/applicationcontroller.hpp
    src/watchanimationcontroller.cpp
    src/watchanimationcontroller.hpp
)

qt_add_qml_module(hyprendfield
    URI HyprEndfield
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/WatchShell.qml
        qml/SystemPage.qml
        qml/DisplayPage.qml
        qml/HudSettingsRow.qml
        qml/HudSlider.qml
        qml/HudGlyph.qml
        qml/MenuArt.qml
        qml/MenuPlane.qml
        qml/MenuCard.qml
        qml/MenuTile.qml
        qml/MenuClock.qml
        qml/MenuBadge.qml
        qml/MenuScene.qml
        qml/MenuGeometry.js
)

file(GLOB MENU_VECTOR_ASSETS CONFIGURE_DEPENDS "assets/menu/*.svg" "assets/menu/*.png")
qt_add_resources(hyprendfield "menu_vectors"
    PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${MENU_VECTOR_ASSETS})
qt_add_shaders(hyprendfield "menu_shaders" PREFIX "/"
    FILES shaders/viewport-mask.frag)

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
        assets/watch/clips.json
        assets/watch/watch-blur.json
        assets/watch/runtime-root-camera.json
        assets/watch/scene.json
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
include(CTest)
if(BUILD_TESTING)
    qt_add_executable(hyprendfield_visual_tests
        tests/scene_projection.cpp src/watchanimationcontroller.cpp src/watchanimationcontroller.hpp)
    target_include_directories(hyprendfield_visual_tests PRIVATE src)
    target_link_libraries(hyprendfield_visual_tests PRIVATE Qt6::Core Qt6::Gui)
    qt_add_resources(hyprendfield_visual_tests "test_watch_data" PREFIX "/" FILES
        assets/watch/clips.json assets/watch/watch-blur.json
        assets/watch/runtime-root-camera.json assets/watch/scene.json)
    add_test(NAME scene_projection COMMAND hyprendfield_visual_tests)
endif()

install(TARGETS hyprendfield
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
