import QtQuick
import "Appearance.js" as Appearance
import QtQuick.Shapes

Item {
    id: control
    property string title: ""
    property string kind: "primary"
    property string icon: ""
    property bool reduceMotion: false
    signal triggered()
    implicitWidth: kind === "primary" ? 150 : 92
    implicitHeight: 40
    opacity: enabled ? 1 : 0.35
    scale: pointer.pressed ? 0.965 : pointer.containsMouse && enabled ? 1.02 : 1
    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.onPressAction: if (enabled) triggered()

    Behavior on scale {
        NumberAnimation {
            duration: control.reduceMotion ? 0 : 150
            easing.type: Easing.OutCubic
        }
    }

    Shape {
        anchors.fill: parent
        visible: control.kind === "primary"
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: control.width; y2: control.height
                GradientStop { position: 0; color: Appearance.accent("#FFFC6B") }
                GradientStop { position: 0.42; color: Appearance.accent("#FFFA00") }
                GradientStop { position: 1; color: "#D9D300" }
            }
            PathSvg { path: "M0 0H" + (control.width - 12) + "L" + control.width + " 12V" + control.height + "H0Z" }
        }
        ShapePath {
            strokeWidth: -1
            fillColor: Qt.rgba(1, 1, 1, pointer.containsMouse ? 0.07 : 0)
            PathSvg { path: "M0 0H" + (control.width - 12) + "L" + control.width + " 12V" + control.height + "H0Z" }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: control.kind === "secondary"
        color: pointer.containsMouse ? "#343434" : "#242424"
        border.color: pointer.containsMouse ? "#575757" : "#393939"
    }
    Rectangle {
        visible: control.kind === "secondary"
        x: 9; y: 12; width: pointer.containsMouse ? 7 : 4; height: 16
        color: Appearance.accent("#FFFA00")
        Behavior on width { NumberAnimation { duration: control.reduceMotion ? 0 : 180; easing.type: Easing.OutCubic } }
    }

    Row {
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: control.kind === "secondary" ? 4 : 0
        spacing: 7
        Item {
            width: 16; height: 16
            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeWidth: -1
                    fillColor: control.kind === "primary" ? "#151515" : "#EDEDED"
                    PathSvg {
                        path: control.icon === "play" ? "M3 1L15 8L3 15Z"
                            : control.icon === "pause" ? "M2 1H6V15H2ZM10 1H14V15H10Z"
                            : control.icon === "stop" ? "M2 2H14V14H2Z"
                            : control.icon === "plus" ? "M6 1H10V6H15V10H10V15H6V10H1V6H6Z"
                            : ""
                    }
                }
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: control.kind === "primary" ? "#151515" : "#EDEDED"
                    strokeWidth: 1.8
                    capStyle: ShapePath.FlatCap
                    joinStyle: ShapePath.MiterJoin
                    PathSvg { path: control.icon === "reset" ? "M13.5 5V1L11.5 3A6 6 0 1 0 14 8" : "" }
                }
            }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.title
            color: control.kind === "primary" ? "#151515" : "#F0F0F0"
            font.family: harmonyBold.name
            font.pixelSize: 13
            font.weight: Font.Bold
            renderType: Text.CurveRendering
        }
    }

    Rectangle {
        visible: control.kind === "primary"
        x: control.width - 11; y: control.height - 10; width: 4; height: 4
        color: "#161616"
    }
    HoverHandler { id: hover; cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
    MouseArea {
        id: pointer
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        enabled: control.enabled
        onClicked: control.triggered()
    }
}
