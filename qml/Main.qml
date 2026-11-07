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

    Settings {
        id: displaySettings
        category: "display"
        property real uiScale: 1.0
        property real hudOffsetX: 0.0
        property real hudOffsetY: 0.0
        property real parallaxIntensity: 1.0
        property real perspectiveIntensity: 1.0
        property real backgroundBrightness: 0.37
        property real blurAmount: 0.75
        property int themeIndex: 0
        property bool clock24Hour: true
        property int clockStyleIndex: 0
        property int centerLogoIndex: 0
        property int accentIndex: 0
        property bool ambientAnimation: true
        property bool reduceMotion: false
        property bool lowPowerVisualMode: false
        property bool recordingMode: false
        property bool dailyBootLog: true
        property bool facilityAlerts: true
    }

    Rectangle {
        id: backdrop
        anchors.fill: parent
        color: Qt.rgba(0.106, 0.110, 0.118,
                       Math.max(0, Math.min(1, 1 - displaySettings.backgroundBrightness)))
        opacity: root.backdropOpacity

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
        }
    }

    WatchShell {
        id: watchShell
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: root.width * displaySettings.hudOffsetX
        anchors.verticalCenterOffset: -30 * scale + root.height * displaySettings.hudOffsetY
        displaySettings: displaySettings
        scale: Math.max(0.1, Math.min(root.width / 1100, root.height / 740, 1.75)
                              * displaySettings.uiScale)
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
        interval: (displaySettings.reduceMotion ? 0
                                                : Math.ceil(sourceWatchAnimation.closingDuration * 1000)) + 20
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
