import QtQuick
import QtQuick.Shapes

Item {
    id: plate
    required property var entry
    property bool selected: false
    property bool reduceMotion: false
    signal clicked()
    width: 248
    height: Math.ceil(16 + titleText.implicitHeight + (entry.details ? detailText.implicitHeight : 0))
    transform: Translate { x: hover.hovered ? 3 : 0 }

    Shape {
        anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: 0; y2: plate.height
                GradientStop { position: 0; color: plate.selected ? "#242424" : "#E2E3DE" }
                GradientStop { position: 1; color: plate.selected ? "#242424" : "#CFD0CB" }
            }
            PathSvg { path: "M0 0H" + (plate.width - 9) + "L" + plate.width + " 9V" + plate.height + "H0Z" }
        }
        ShapePath { strokeWidth: -1; fillColor: Qt.rgba(1, 1, 1, hover.hovered ? 0.07 : 0); PathSvg { path: "M0 0H" + (plate.width - 9) + "L" + plate.width + " 9V" + plate.height + "H0Z" } }
    }
    Rectangle { x: 0; y: 8; width: 3; height: parent.height - 16; color: "#F2A23A" }
    Text {
        id: titleText; x: 16; y: 8; width: parent.width - 26; maximumLineCount: 2; wrapMode: Text.Wrap; elide: Text.ElideRight
        text: plate.entry.title; color: plate.selected ? "#F2F2F2" : "#111111"
        font.family: harmonyMedium.name; font.pointSize: 13.5 * 0.75; font.weight: Font.Medium; lineHeight: 1.3
        renderType: Text.CurveRendering
    }
    Text {
        id: detailText; x: 16; y: 8 + titleText.implicitHeight; width: parent.width - 26; maximumLineCount: 2; wrapMode: Text.Wrap; elide: Text.ElideRight
        text: String(plate.entry.details || "").replace(/\n/g, " "); color: plate.selected ? "#AAAAAA" : "#555555"
        font.family: harmonyRegular.name; font.pixelSize: 11; lineHeight: 1.3; visible: text !== ""
        renderType: Text.CurveRendering
    }
    HoverHandler { id: hover }
    MouseArea { anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: plate.clicked() }
}
