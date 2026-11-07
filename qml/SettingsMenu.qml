import QtQuick
import "Appearance.js" as Appearance

Item {
    id: popup
    property var entries: []
    property int selected: -1
    signal chosen(int index)
    width: 178; height: entries.length * 30 + 8
    Rectangle { anchors.fill: parent; color: Appearance.tone("#1A1A1A"); border.width: 1; border.color: Appearance.tone("#3D3D3D") }
    Rectangle { width: 10; height: 2; color: Appearance.accent("#D7BC18") }
    Rectangle { width: 2; height: 10; color: Appearance.accent("#D7BC18") }
    Repeater {
        model: popup.entries
        delegate: Item {
            required property int index
            required property var modelData
            x: 4; y: 4 + index * 30; width: popup.width - 8; height: 30
            Rectangle { anchors.fill: parent; color: pointer.containsMouse ? Appearance.tone("#D3D4CF") : "transparent" }
            Rectangle { x: 9; y: 11; width: 8; height: 8; color: index === popup.selected ? Appearance.accent("#FFFA00") : "transparent"; border.width: index === popup.selected ? 0 : 1; border.color: Appearance.tone("#555555") }
            Text { x: 27; width: parent.width - 32; height: 30; text: typeof modelData === "string" ? modelData : modelData.title; color: pointer.containsMouse ? Appearance.tone("#111111") : Appearance.tone("#D9D9D9"); elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; font.family: harmonyMedium.name; font.pixelSize: 13; renderType: Text.CurveRendering }
            MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: popup.chosen(index) }
        }
    }
}
