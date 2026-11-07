import QtQuick
import "Appearance.js" as Appearance
import QtQuick.Shapes

Item {
    id: control
    property string title: ""
    property string kind: "primary"
    property string glyph: ""
    property bool reduceMotion: false
    signal clicked()
    implicitWidth: 64
    implicitHeight: kind === "icon" ? 30 : 26
    opacity: enabled ? 1 : 0.35
    transform: [Translate { y: pointer.containsMouse && control.enabled ? -1 : 0 }, Scale {
        origin.x: control.width / 2; origin.y: control.height / 2
        xScale: pointer.pressed ? 0.965 : control.kind === "icon" && pointer.containsMouse ? 1.06 : 1
        yScale: xScale
    }]
    Behavior on opacity { NumberAnimation { duration: control.reduceMotion ? 0 : 120 } }

    Shape {
        anchors.fill: parent; visible: control.kind === "primary"; preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: control.width; y2: control.height
                GradientStop { position: 0; color: Appearance.accent("#FFFC6B") }
                GradientStop { position: 0.38; color: Appearance.accent("#FFFA00") }
                GradientStop { position: 1; color: "#D9D300" }
            }
            PathSvg { path: "M0 0H" + (control.width - 9) + "L" + control.width + " 9V" + control.height + "H0Z" }
        }
        ShapePath { strokeWidth: -1; fillColor: Qt.rgba(1, 1, 1, pointer.containsMouse ? 0.07 : 0); PathSvg { path: "M0 0H" + (control.width - 9) + "L" + control.width + " 9V" + control.height + "H0Z" } }
    }
    Rectangle {
        anchors.fill: parent; visible: control.kind === "secondary"
        color: pointer.containsMouse ? "#343434" : "#242424"
    }
    Rectangle {
        visible: control.kind === "secondary"; x: 8; y: 7; width: pointer.containsMouse ? 7 : 4; height: 12; color: Appearance.accent("#FFFA00")
        Behavior on width { NumberAnimation { duration: control.reduceMotion ? 0 : 200; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1] } }
    }
    Rectangle {
        anchors.fill: parent; visible: control.kind === "icon"
        color: pointer.containsMouse ? "#2E2E2E" : "#202020"; border.color: pointer.containsMouse ? "#888888" : "#555555"
    }
    Text {
        anchors.centerIn: parent; visible: control.kind === "icon"; text: control.glyph
        color: "#EDEDED"; font.family: harmonyBold.name; font.pixelSize: 15; font.weight: Font.Bold
        renderType: Text.CurveRendering
    }
    Text {
        anchors.centerIn: parent; anchors.horizontalCenterOffset: control.kind === "secondary" ? 5 : 0
        visible: control.kind !== "icon"; text: control.title
        color: control.kind === "primary" ? "#111111" : "#EDEDED"
        font.family: harmonyBold.name; font.pointSize: 10.5 * 0.75; font.weight: Font.Bold
        renderType: Text.CurveRendering
    }
    Rectangle { visible: control.kind === "primary"; x: control.width - 10; y: control.height - 9; width: 4; height: 4; color: "#161616" }
    HoverHandler { id: hover }
    MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; enabled: control.enabled; onClicked: control.clicked() }
}
