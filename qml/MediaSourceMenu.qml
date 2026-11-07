import QtQuick
import "Appearance.js" as Appearance

Rectangle {
    id: menu
    property var choices: []
    signal chooseFile()
    signal chosen(url source)
    width: 260; height: 34 + Math.min(5, choices.length) * 30
    color: Appearance.tone("#F51B1B1B"); border.color: Appearance.tone("#555555"); z: 250
    onVisibleChanged: if (visible) choices = fileShelfController.mediaChoices()
    Column {
        width: parent.width
        CalendarButton { width: 260; height: 34; title: qsTr("从文件管理器选择"); kind: "secondary"; onClicked: { menu.visible = false; menu.chooseFile(); } }
        Repeater {
            model: menu.choices.slice(0, 5)
            Rectangle {
                required property var modelData; width: 260; height: 30; color: pointer.containsMouse ? Appearance.tone("#343434") : "transparent"
                Text { x: 10; width: 240; height: 30; verticalAlignment: Text.AlignVCenter; text: modelData.name; elide: Text.ElideMiddle; color: Appearance.tone("#EEEEEE"); font.family: harmonyRegular.name; font.pixelSize: 12; renderType: Text.CurveRendering }
                MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { menu.visible = false; menu.chosen(fileShelfController.mediaUrl(modelData.id)); } }
            }
        }
    }
}
