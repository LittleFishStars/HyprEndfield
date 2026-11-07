import QtQuick
import "Appearance.js" as Appearance
import QtQuick.Shapes

// FileShelfCanvas.buildCard, 186 px wide, 12 px padding and an 84 px preview.
Item {
    id: card
    required property var entry
    property var details: ({})
    property bool reduceMotion: false
    property real entrance: 1
    signal opened()
    signal revealed()
    signal removed()
    width: 186; height: actions.y + 42
    opacity: Math.min(1, entrance / 0.6)
    transform: Translate { x: 24 * (1 - card.entrance) }
    function enter() { entranceMotion.stop(); entrance = reduceMotion ? 1 : 0; if (!reduceMotion) entranceMotion.restart() }
    Component.onCompleted: enter()
    onEntryChanged: enter()
    NumberAnimation { id: entranceMotion; target: card; property: "entrance"; to: 1; duration: 300; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] }
    Shape {
        anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1
            fillGradient: LinearGradient { x1: 0; y1: 0; x2: 0; y2: card.height; GradientStop { position: 0; color: "#E4E5E0" } GradientStop { position: 1; color: "#CFD0CB" } }
            PathSvg { path: "M0 0H174L186 12V" + card.height + "H0Z" }
        }
    }
    Rectangle {
        x: 12; y: 12; width: 162; height: 84
        gradient: Gradient { GradientStop { position: 0; color: "#8A8885" } GradientStop { position: 1; color: "#C9C9C7" } }
        ShelfDocument { anchors.centerIn: parent; kind: card.entry.kind; label: card.entry.extension; reduceMotion: card.reduceMotion }
    }
    Text {
        id: name; x: 12; y: 106; width: 162; text: card.entry.name; maximumLineCount: 3; wrapMode: Text.WrapAnywhere; elide: Text.ElideRight
        visible: !Appearance.recording()
        color: "#111111"; font.family: harmonyBold.name; font.pixelSize: 13; font.weight: Font.Bold; lineHeight: 1.25; renderType: Text.CurveRendering
    }
    Redaction { x: 12; y: 107; width: 150; height: 14; visible: Appearance.recording() }
    Column {
        id: rows; x: 12; y: name.y + (Appearance.recording() ? 16.25 : name.height) + 8; spacing: 5
        Repeater {
            model: [
                {term: qsTr("类型"), value: card.entry.kindText},
                {term: card.entry.directory ? qsTr("项目") : qsTr("大小"), value: card.entry.directory ? (card.details.count || "—") : card.entry.size},
                {term: qsTr("位置"), value: Appearance.recording() ? "" : card.entry.location, multiline: true},
                {term: qsTr("创建"), value: card.details.created || "—"},
                {term: qsTr("修改"), value: card.details.modified || "—"}
            ]
            Item {
                required property var modelData; width: 162; height: Math.max(14.4, value.height)
                Redaction { x: 32; y: 1; width: 96; height: 12; visible: modelData.multiline === true && Appearance.recording() }
                Text { width: 24; height: 14.4; text: modelData.term; color: "#666666"; font.family: harmonyBold.name; font.pixelSize: 9; font.weight: Font.Bold; font.letterSpacing: 0.72; renderType: Text.CurveRendering }
                Text { id: value; x: 32; width: 130; text: modelData.value; color: "#111111"; font.family: harmonyRegular.name; font.pixelSize: 11; wrapMode: Text.WrapAnywhere; maximumLineCount: modelData.multiline ? 3 : 1; elide: Text.ElideRight; lineHeight: 1.3; renderType: Text.CurveRendering }
            }
        }
    }
    Row {
        id: actions; x: 12; y: rows.y + rows.height + 10; spacing: 6
        CalendarButton { objectName: "shelfOpen"; width: 62; height: 26; title: qsTr("打开"); enabled: card.entry.available; reduceMotion: card.reduceMotion; onClicked: card.opened() }
        Item {
            width: 30; height: 30; enabled: card.entry.available; opacity: enabled ? 1 : 0.35
            Rectangle { anchors.fill: parent; color: revealMouse.containsMouse ? "#2E2E2E" : "#202020"; border.color: "#555555" }
            ClipboardGlyph { anchors.centerIn: parent; width: 14; height: 14; kind: "folder"; ink: "#EDEDED" }
            MouseArea { id: revealMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: card.revealed() }
        }
        CalendarButton { objectName: "shelfRemove"; width: 30; height: 30; kind: "icon"; glyph: "×"; reduceMotion: card.reduceMotion; onClicked: card.removed() }
    }
}
