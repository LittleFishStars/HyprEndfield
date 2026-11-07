import QtQuick
import QtQuick.Window
import org.kde.layershell 1.0 as LayerShell

Window {
    id: root

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
        anchors.fill: parent
        color: "#d91b1c1e"

        MouseArea {
            anchors.fill: parent
            onClicked: appController.hideHud()
        }
    }

    WatchShell {
        id: watchShell
        anchors.centerIn: parent
        scale: Math.min(root.width / width, root.height / height)
        opacity: root.visible ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            propagateComposedEvents: true
            onPressed: mouse => mouse.accepted = false
        }
    }
}
