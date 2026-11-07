import QtQuick
import QtQuick.Shapes

Item {
    id: button
    property string title: ""
    property string kind: "text"
    property bool reduceMotion: false
    signal clicked()
    implicitHeight: 40

    transform: [
        Translate { y: pointer.containsMouse && button.enabled ? -2 : 0 },
        Scale {
            origin.x: button.width / 2; origin.y: button.height / 2
            xScale: pointer.pressed ? 0.975 : 1
            yScale: xScale
        }
    ]
    Behavior on opacity { NumberAnimation { duration: button.reduceMotion ? 0 : 120 } }
    opacity: enabled ? 1 : 0.35

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: button.width; y2: button.height
                GradientStop { position: 0; color: "#E4E5E0" }
                GradientStop { position: 1; color: "#C7C8C3" }
            }
            PathSvg { path: "M0 0H" + (button.width - 8) + "L" + button.width + " 8V" + button.height + "H0Z" }
        }
        ShapePath {
            strokeWidth: -1
            fillColor: Qt.rgba(1, 1, 1, pointer.containsMouse ? 0.10 : 0)
            PathSvg { path: "M0 0H" + (button.width - 8) + "L" + button.width + " 8V" + button.height + "H0Z" }
        }
    }
    Row {
        anchors.centerIn: parent
        spacing: 8
        NotesGlyph { width: 16; height: 16; kind: button.kind; ink: "#181818" }
        Text {
            text: button.title; color: "#181818"
            font.family: harmonyMedium.name; font.pixelSize: 13; font.weight: Font.Medium
            renderType: Text.CurveRendering
        }
    }
    Rectangle { x: button.width - 10; y: button.height - 9; width: 4; height: 4; color: "#1A1A1A" }
    HoverHandler { id: hover }
    MouseArea {
        id: pointer; anchors.fill: parent; hoverEnabled: true
        enabled: button.enabled; cursorShape: Qt.PointingHandCursor
        onClicked: button.clicked()
    }
}
