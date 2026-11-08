import QtQuick
import QtQuick.Window
import QtCore
import org.kde.layershell 1.0 as LayerShell

Window {
    id: root

    readonly property real backdropOpacity: watchShell.backdropOpacity

    width: 0
    height: 0
    visible: false
    color: "transparent"
    title: "HyprEndfield"
    flags: Qt.FramelessWindowHint

    LayerShell.Window.anchors: LayerShell.Window.AnchorTop
                               | LayerShell.Window.AnchorBottom
                               | LayerShell.Window.AnchorLeft
                               | LayerShell.Window.AnchorRight
    LayerShell.Window.layer: LayerShell.Window.LayerOverlay
    // ApplicationController acquires focus on opening, then restores OnDemand.
    LayerShell.Window.exclusionZone: -1
    LayerShell.Window.scope: appController.layerScope
    LayerShell.Window.wantsToBeOnActiveScreen: true

    FontLoader {
        id: harmonyRegular
        source: "qrc:/assets/fonts/HarmonyOS_Sans_Regular.ttf"
    }

    FontLoader {
        id: harmonyMedium
        source: "qrc:/assets/fonts/HarmonyOS_Sans_Medium.ttf"
    }

    FontLoader {
        id: harmonyBold
        source: "qrc:/assets/fonts/HarmonyOS_Sans_Bold.ttf"
    }

    Shortcut {
        sequence: "Meta+Escape"
        onActivated: appController.toggleHud()
    }

    readonly property var displaySettings: settingsController
    BatteryAlertWindow { }
    HudSounds {
        id: hudSounds
        volume: settingsController.soundVolume
        hudOpen: appController.hudOpen
        interactive: appController.hudOpen && !watchShell.transitioning && !fullProjection.opened
    }

    Rectangle {
        id: backdrop
        visible: !fullProjection.opened
        anchors.fill: parent
        color: Qt.rgba(settingsController.light ? .90 : .106, settingsController.light ? .90 : .110, settingsController.light ? .90 : .118,
                       Math.max(0, Math.min(1, 1 - displaySettings.backgroundBrightness)))
        opacity: root.backdropOpacity

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
        }
    }

    WatchShell {
        id: watchShell
        soundEffects: hudSounds
        projectionWorkspace: fullProjection
        visible: !fullProjection.opened
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: root.width * displaySettings.hudOffsetX
        anchors.verticalCenterOffset: -30 * scale + root.height * displaySettings.hudOffsetY
        displaySettings: root.displaySettings
        scale: Math.max(0.1, Math.min(root.width / 1100, root.height / 740, 1.75)
                              * displaySettings.uiScale)
    }
    FacilityBanner {
        x: (root.width - width * scale) / 2; y: 14; z: 1500
        scale: watchShell.scale * 1000 / 1920 * 1.12; transformOrigin: Item.TopLeft
        presented: appController.hudOpen && !fullProjection.opened && !watchShell.transitioning && !watchShell.bootLogActive
        onOpenModule: module => watchShell.selectedModule = module
    }
    ProjectionWorkspace { id: fullProjection; anchors.fill: parent; presented: appController.hudVisible; z: 2000 }
    Item {
        id: batteryPositionEditor; anchors.fill: parent; z: 1600; visible: settingsController.editingBatteryPosition
        onVisibleChanged: if (visible) { preview.x = width * settingsController.batteryX - preview.width / 2; preview.y = settingsController.batteryPlacement === 0 ? 12 : height * settingsController.batteryY - preview.height / 2; forceActiveFocus(); }
        Keys.onEscapePressed: event => { settingsController.cancelBatteryPosition(); event.accepted = true; }
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onClicked: settingsController.cancelBatteryPosition() }
        Item {
            id: preview; width: 300 * settingsController.batteryScale; height: 84 * settingsController.batteryScale
            BatteryBadge { scale: settingsController.batteryScale; transformOrigin: Item.TopLeft; alwaysExpanded: true; manageMonitoring: false; presented: batteryPositionEditor.visible; reading: batteryController.state }
            MouseArea {
                anchors.fill: parent; cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                drag.target: preview; drag.minimumX: 0; drag.minimumY: 0; drag.maximumX: batteryPositionEditor.width - preview.width; drag.maximumY: batteryPositionEditor.height - preview.height
                onReleased: settingsController.placeBattery((preview.x + preview.width / 2) / batteryPositionEditor.width, (preview.y + preview.height / 2) / batteryPositionEditor.height)
            }
        }
    }

    HoverHandler {
        enabled: !fullProjection.opened
        onPointChanged: {
            const normalizedX = root.width > 0
                    ? 2 * point.position.x / root.width - 1 : 0
            const normalizedY = root.height > 0
                    ? 1 - 2 * point.position.y / root.height : 0
            watchShell.retargetPointer(normalizedX, normalizedY)
        }

        onHoveredChanged: {
            if (!hovered) {
                watchShell.retargetPointer(0, 0)
            }
        }
    }

    Timer {
        id: concealTimer
        interval: (displaySettings.reduceMotion ? 0
                                                : Math.ceil(watchShell.closingSequenceDuration * 1000)) + 20
        repeat: false
        onTriggered: appController.completeHide()
    }

    Connections {
        target: appController

        function onHudOpenChanged() {
            if (appController.hudOpen) {
                concealTimer.stop()
                watchShell.animateEntrance()
                return
            }

            if (appController.hudVisible) {
                watchShell.animateExit()
                concealTimer.restart()
            }
        }
    }

}
