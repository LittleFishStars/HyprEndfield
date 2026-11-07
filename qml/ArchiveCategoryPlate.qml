import QtQuick
import QtQuick.Shapes

// EndfieldHUD ArchiveCategoryPlate: 48 px chamfered category tab.
Item {
    id: plate
    required property var entry
    property bool selected: false
    property bool reduceMotion: false
    signal clicked()
    signal renameRequested()
    width: 150; height: 48
    transform: Translate { x: pointer.containsMouse && plate.enabled ? 3 : 0 }

    Shape {
        anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: plate.width; y2: plate.height
                GradientStop { position: 0; color: plate.selected ? "#262626" : pointer.containsMouse ? "#E9EAE5" : "#DCDDD8" }
                GradientStop { position: 1; color: plate.selected ? "#262626" : pointer.containsMouse ? "#D7D8D3" : "#CBCCC7" }
            }
            PathSvg { path: "M0 0H" + (plate.width - 10) + "L" + plate.width + " 10V" + plate.height + "H0Z" }
        }
    }
    Rectangle { visible: plate.selected; x: 6; y: 10; width: 3; height: 28; color: "#FFFA00" }
    Text {
        x: 16; y: 7; width: plate.width - 28; height: 18; text: plate.entry.name
        color: plate.selected ? "#F2F2F2" : "#191919"; elide: Text.ElideRight
        font.family: harmonyMedium.name; font.pixelSize: 14; font.weight: Font.Medium
        renderType: Text.CurveRendering
    }
    Text {
        x: 16; y: 26; text: String(plate.entry.count).padStart(2, "0")
        color: plate.selected ? "#9A9A9A" : "#666666"
        font.family: harmonyBold.name; font.pointSize: 9.5 * 0.75; font.weight: Font.Bold; font.letterSpacing: 0.57
        renderType: Text.CurveRendering
    }
    Rectangle {
        visible: plate.entry.isNew; x: plate.width - 39; y: 5; width: 25; height: 13; color: "#FFFA00"
        Text { anchors.centerIn: parent; text: "NEW"; color: "#171717"; font.family: harmonyBold.name; font.pixelSize: 7; font.weight: Font.Bold; renderType: Text.CurveRendering }
    }
    Behavior on opacity { NumberAnimation { duration: plate.reduceMotion ? 0 : 140 } }
    MouseArea {
        id: pointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
        onClicked: plate.clicked(); onDoubleClicked: plate.renameRequested()
    }
}
