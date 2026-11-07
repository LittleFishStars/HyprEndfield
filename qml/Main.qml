import QtQuick
import QtQuick.Window
import org.kde.layershell 1.0 as LayerShell

Window {
    id: root

    property real backdropOpacity: 0

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
        scale: Math.max(0.1, Math.min(root.width / 1100, root.height / 740, 1.5))
    }

    NumberAnimation {
        id: backdropEntrance
        target: root
        property: "backdropOpacity"
        to: 1
        duration: 133
        easing.type: Easing.InOutQuad
    }

    SequentialAnimation {
        id: backdropExit

        NumberAnimation {
            target: root
            property: "backdropOpacity"
            to: 0
            duration: 133
            easing.type: Easing.Linear
        }
    }

    Timer {
        id: concealTimer
        interval: 350
        repeat: false
        onTriggered: appController.completeHide()
    }

    Connections {
        target: appController

        function onHudOpenChanged() {
            if (appController.hudOpen) {
                concealTimer.stop()
                backdropExit.stop()
                backdropEntrance.restart()
                watchShell.animateEntrance()
                return
            }

            if (appController.hudVisible) {
                backdropEntrance.stop()
                backdropExit.restart()
                watchShell.animateExit()
                concealTimer.restart()
            }
        }
    }

}
