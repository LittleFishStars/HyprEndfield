import QtQuick
import QtQuick.Window
import org.kde.layershell 1.0 as LayerShell

Window {
    id: root

    width: 1180
    height: 760
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

    property string selectedModule: "系统概览"
    property string clockText: "00:00"
    property string dateText: ""

    function updateClock() {
        const now = new Date()
        clockText = Qt.formatTime(now, "HH:mm")
        dateText = Qt.formatDate(now, "yyyy.MM.dd")
    }

    FontLoader {
        id: harmonyRegular
        source: "qrc:/assets/fonts/HarmonyOS_Sans_Regular.ttf"
    }

    FontLoader {
        id: harmonyMedium
        source: "qrc:/assets/fonts/HarmonyOS_Sans_Medium.ttf"
    }

    Timer {
        interval: 1000
        running: root.visible
        repeat: true
        triggeredOnStart: true
        onTriggered: root.updateClock()
    }

    Shortcut {
        sequence: "Escape"
        onActivated: appController.hideHud()
    }

    component NavButton: Rectangle {
        id: navButton

        required property string label
        property bool selected: root.selectedModule === label
        signal triggered()

        width: 164
        height: 40
        color: selected ? "#e7cf42" : hovered ? "#d9dce1" : "#aee7edf2"
        border.width: 1
        border.color: selected ? "#f8ec8b" : "#66747d"
        radius: 2

        property bool hovered: mouseArea.containsMouse

        Text {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            text: navButton.label
            color: navButton.selected ? "#151719" : "#20262b"
            font.family: harmonyMedium.name
            font.pixelSize: 14
            font.letterSpacing: 1.1
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Rectangle {
            width: 4
            height: parent.height
            anchors.left: parent.left
            color: navButton.selected ? "#151719" : "#89959d"
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: navButton.triggered()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#a60b0f13"

        MouseArea {
            anchors.fill: parent
            onClicked: appController.hideHud()
        }
    }

    Item {
        id: shell

        width: 1080
        height: 690
        anchors.centerIn: parent
        opacity: root.visible ? 1 : 0
        scale: root.visible ? 1 : 0.97

        Behavior on opacity {
            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
        }

        Behavior on scale {
            NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onClicked: mouse => mouse.accepted = true
        }

        Image {
            id: ring
            width: 650
            height: 650
            anchors.centerIn: parent
            source: "qrc:/assets/watch/ui_mid_ring.png"
            fillMode: Image.PreserveAspectFit
            opacity: 0.5
            smooth: true

            RotationAnimator on rotation {
                from: 0
                to: 360
                duration: 150000
                loops: Animation.Infinite
                running: root.visible
            }
        }

        Image {
            width: 88
            height: 76
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 5
            source: "qrc:/assets/watch/ui_triangle_fx.png"
            fillMode: Image.PreserveAspectFit
            opacity: 0.82
        }

        Rectangle {
            id: centrePanel
            width: 590
            height: 360
            anchors.centerIn: parent
            color: "#e8edf0"
            border.width: 1
            border.color: "#9ba8b0"
            radius: 4

            Rectangle {
                width: parent.width
                height: 8
                color: "#1c2328"
            }

            Rectangle {
                width: parent.width - 32
                height: parent.height - 32
                anchors.centerIn: parent
                color: "transparent"
                border.width: 1
                border.color: "#b9c2c7"
            }

            Column {
                anchors.centerIn: parent
                spacing: 14

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.selectedModule
                    color: "#1a2024"
                    font.family: harmonyMedium.name
                    font.pixelSize: 30
                    font.letterSpacing: 4
                }

                Rectangle {
                    width: 220
                    height: 2
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: "#d1b828"
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "模块将在后续组件中接入"
                    color: "#536169"
                    font.family: harmonyRegular.name
                    font.pixelSize: 15
                    font.letterSpacing: 1
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "HYPRLAND // LOCAL TERMINAL"
                    color: "#7e898f"
                    font.family: harmonyRegular.name
                    font.pixelSize: 10
                    font.letterSpacing: 2
                }
            }

            Rectangle {
                width: 108
                height: 24
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                color: "#20272c"

                Text {
                    anchors.centerIn: parent
                    text: "SHELL 001"
                    color: "#e7cf42"
                    font.family: harmonyMedium.name
                    font.pixelSize: 10
                    font.letterSpacing: 1.5
                }
            }
        }

        Column {
            width: 164
            spacing: 8
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter

            Repeater {
                model: ["系统概览", "显示设置", "快捷键", "关于"]

                delegate: NavButton {
                    required property string modelData
                    label: modelData
                    onTriggered: root.selectedModule = label
                }
            }
        }

        Column {
            width: 164
            spacing: 8
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            Repeater {
                model: ["便笺", "文件暂存架", "剪贴板", "档案库", "应用快捷方式", "阅读器", "当前播放", "工作模式", "日历", "地图", "事件日志", "个人名片"]

                delegate: NavButton {
                    required property string modelData
                    label: modelData
                    onTriggered: root.selectedModule = label
                }
            }
        }

        Column {
            anchors.left: centrePanel.left
            anchors.leftMargin: 18
            anchors.top: centrePanel.bottom
            anchors.topMargin: 18
            spacing: 2

            Text {
                text: root.clockText
                color: "#e9eef1"
                font.family: harmonyMedium.name
                font.pixelSize: 38
                font.letterSpacing: 2
            }

            Text {
                text: root.dateText
                color: "#8f9ca4"
                font.family: harmonyRegular.name
                font.pixelSize: 12
                font.letterSpacing: 2
            }
        }

        Rectangle {
            width: 72
            height: 72
            radius: 36
            anchors.right: centrePanel.right
            anchors.rightMargin: 8
            anchors.top: centrePanel.bottom
            anchors.topMargin: 20
            color: powerMouse.containsMouse ? "#e24a3a" : "#c62e28"
            border.width: 2
            border.color: "#f28a79"

            Text {
                anchors.centerIn: parent
                text: "⏻"
                color: "white"
                font.pixelSize: 30
            }

            MouseArea {
                id: powerMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: appController.quitApplication()
            }
        }
    }

    Component.onCompleted: updateClock()
}
