import QtQuick
import "Appearance.js" as Appearance

// NotesDesktopPopup: 200px font menu, 96px size menu and 20px trait toggles.
Rectangle {
    id: menu
    property string kind: ""
    property var style: ({})
    signal changed(string key, var value)
    property var sizes: [8,10,11,12,14,16,18,20,24,28,32,40,48,64,72,96,144]
    property var families: ["HarmonyOS Sans", "Noto Sans CJK SC", "Noto Sans", "DejaVu Sans", "Liberation Sans", "Monospace"]
    width: kind === "font" ? 200 : kind === "size" ? 96 : kind === "color" ? 220 : 100
    height: kind === "special" ? 28 : kind === "color" ? 248 : Math.min(224, list.contentHeight)
    visible: kind !== ""; color: Appearance.tone("#1B1B1B"); border.color: Appearance.tone("#555555"); z: 200
    ListView {
        id: list; anchors.fill: parent; clip: true; visible: menu.kind === "font" || menu.kind === "size"
        model: menu.kind === "font" ? menu.families : menu.sizes
        delegate: Rectangle {
            required property var modelData; width: list.width; height: 28
            color: pointer.containsMouse ? Appearance.tone("#343434") : "transparent"
            Text { x: 12; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 18; text: modelData; elide: Text.ElideRight; color: Appearance.tone("#EEEEEE"); font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering }
            MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { menu.changed(menu.kind === "font" ? "family" : "size", modelData); menu.kind = ""; } }
        }
    }
    Row {
        x: 4; y: 4; spacing: 4; visible: menu.kind === "special"
        Repeater {
            model: ["bold", "italic", "underline", "strikeout"]
            Rectangle {
                required property string modelData; required property int index
                width: 20; height: 20; color: menu.style[modelData] ? Appearance.accent("#FFFA00") : Appearance.tone("#262626"); border.color: Appearance.tone("#3A3A3A")
                Text { anchors.centerIn: parent; text: ["B", "I", "U", "S"][index]; font.pixelSize: 10; font.bold: true; font.italic: index === 1; font.underline: index === 2; font.strikeout: index === 3; color: menu.style[modelData] ? "#111111" : Appearance.tone("#DDDDDD") }
                MouseArea { anchors.fill: parent; onClicked: menu.changed(modelData, !menu.style[modelData]) }
            }
        }
    }
    ColorWheel { x: 14; y: 12; visible: menu.kind === "color"; selectedColor: menu.style.color || "#EDEDED"; onChosen: color => menu.changed("color", color) }
}
