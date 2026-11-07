import QtQuick
import "Appearance.js" as Appearance
import QtQuick.Shapes

// EndfieldHUD HUDArchiveTile with the 96 px well and 24 px label strip.
Item {
    id: tile
    required property var entry
    required property int number
    property bool reduceMotion: false
    signal clicked()
    width: 132; height: 120
    readonly property bool classified: entry.classified

    Rectangle {
        id: well; width: parent.width; height: 96; clip: true
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: tile.classified ? "#2A2A2A" : "#8A8885" }
            GradientStop { position: 0.55; color: tile.classified ? "#202020" : "#BDBDBB" }
            GradientStop { position: 1; color: tile.classified ? "#151515" : "#D9D9D8" }
        }
        transform: Translate { y: pointer.containsMouse ? -2 : 0 }
        Behavior on transform { enabled: false }
        Image {
            id: poster
            anchors.fill: parent; source: tile.entry.thumbnail || ""; fillMode: Image.PreserveAspectCrop
            visible: !tile.classified && status === Image.Ready; asynchronous: true; cache: false
        }
        Rectangle { width: 34; height: 4; color: Appearance.accent("#FFFA00") }
        Text { x: 38; y: -1; text: "#" + String(tile.number).padStart(2, "0"); color: "#222222"; font.family: harmonyBold.name; font.pixelSize: 6; font.weight: Font.Bold; renderType: Text.CurveRendering }

        Item {
            id: documentGlyph; visible: !tile.classified && poster.status !== Image.Ready
            width: 40; height: 50; x: (well.width - width) / 2; y: (well.height - height) / 2
            rotation: pointer.containsMouse ? -2 : -6
            Behavior on rotation { NumberAnimation { duration: tile.reduceMotion ? 0 : 300 } }
            Shape {
                anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
                ShapePath { strokeWidth: -1; fillColor: "#F4F4F2"; PathSvg { path: "M0 0H28L40 11V50H0Z" } }
                ShapePath { strokeWidth: -1; fillColor: "#D0D0CE"; PathSvg { path: "M28 0V11H40Z" } }
                ShapePath { strokeWidth: 1; strokeColor: "#9B9B9B"; fillColor: "transparent"; PathSvg { path: "M6 10H30M6 14H30M6 18H30M6 22H30" } }
            }
            Text { width: 40; y: 36; horizontalAlignment: Text.AlignHCenter; text: "DOC"; color: "#333333"; font.family: harmonyBold.name; font.pixelSize: 8; font.weight: Font.Black; font.letterSpacing: 0.48; renderType: Text.CurveRendering }
        }
        Item {
            visible: tile.classified; anchors.centerIn: parent; width: 72; height: 25; rotation: -10
            Rectangle { anchors.fill: parent; color: "transparent"; border.width: 2; border.color: "#D63235" }
            Text { anchors.centerIn: parent; text: qsTr("机密"); color: "#D63235"; font.family: harmonyBold.name; font.pixelSize: 10; font.weight: Font.Black; font.letterSpacing: 1.8; renderType: Text.CurveRendering }
        }
        Text {
            x: 6; y: 83; text: tile.entry.date; color: tile.classified ? "#777777" : "#222222"
            font.family: harmonyBold.name; font.pointSize: 8.5 * 0.75; font.weight: Font.Bold; renderType: Text.CurveRendering
        }
        Rectangle { anchors.fill: parent; color: "#1FFFFFFF"; opacity: pointer.containsMouse ? 1 : 0; Behavior on opacity { NumberAnimation { duration: tile.reduceMotion ? 0 : 180 } } }
    }
    Rectangle { y: 96; width: parent.width; height: 24; color: tile.classified ? "#262626" : "#C4C4C6" }
    Text {
        x: 6; y: 102.25; width: parent.width - 12; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight
        text: tile.entry.title && tile.entry.title.length ? tile.entry.title : qsTr("未命名")
        color: tile.classified ? "#D63235" : "#191919"
        font.family: tile.classified ? harmonyBold.name : harmonyMedium.name; font.pixelSize: tile.classified ? 10 : 11.5
        font.weight: tile.classified ? Font.Bold : Font.Medium; font.letterSpacing: tile.classified ? 1.2 : 0
        renderType: Text.CurveRendering
    }
    MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: tile.clicked() }
}
