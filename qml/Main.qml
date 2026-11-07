import QtQuick
import QtQuick.Window
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
    LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityOnDemand
    LayerShell.Window.exclusionZone: -1
    LayerShell.Window.scope: "hyprendfield"
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
        sequence: "Escape"
        onActivated: appController.hideHud()
    }

    Rectangle {
        id: backdrop
        anchors.fill: parent
        color: "#d91b1c1e"
        opacity: root.backdropOpacity

        MouseArea {
            anchors.fill: parent
            onClicked: appController.hideHud()
        }
    }

    WatchShell {
        id: watchShell
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -30 * scale
        scale: Math.max(0.1, Math.min(root.width / 1100, root.height / 740, 1.75))
    }

    HoverHandler {
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
        interval: Math.ceil(sourceWatchAnimation.closingDuration * 1000) + 20
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
