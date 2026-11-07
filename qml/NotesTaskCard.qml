import QtQuick
import QtQuick.Shapes

Item {
    id: card
    required property var task
    property bool reduceMotion: false
    signal toggled()
    signal removed()
    signal flagged()
    signal moved(int offset)
    signal renamed(string text)
    width: 580; height: 60
    readonly property real sideWidth: 168
    readonly property real bodyWidth: width - sideWidth

    Rectangle {
        width: card.bodyWidth; height: parent.height
        gradient: Gradient {
            GradientStop { position: 0; color: card.task.checked ? "#5E5E5E" : "#E0E1DC" }
            GradientStop { position: 1; color: card.task.checked ? "#525252" : "#D0D1CC" }
        }
    }
    Shape {
        x: card.bodyWidth; width: card.sideWidth; height: parent.height
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: card.sideWidth; y2: card.height
                GradientStop { position: 0; color: card.task.checked ? "#FFFC6B" : "#BEBFBA" }
                GradientStop { position: 1; color: card.task.checked ? "#D9D300" : "#AFB0AB" }
            }
            PathSvg { path: "M0 0H" + (card.sideWidth - 10) + "L" + card.sideWidth + " 10V" + card.height + "H0Z" }
        }
    }
    Rectangle { x: 14; y: 4; width: 21; height: 8; color: card.task.checked ? "#222222" : "#F5F5F2" }
    TextInput {
        id: titleInput; x: 18; y: 13; width: card.bodyWidth - 40; height: 22
        text: card.task.text; color: card.task.checked ? "#F2F2F2" : "#151515"
        selectByMouse: true; maximumLength: 60
        font.family: harmonyMedium.name; font.pointSize: 14.5 * 0.75; font.weight: Font.Medium
        selectionColor: "#88FFFA00"; selectedTextColor: "#111111"
        renderType: Text.CurveRendering
        onAccepted: { card.renamed(text); focus = false }
        onActiveFocusChanged: if (!activeFocus && text.trim() && text.trim() !== card.task.text) card.renamed(text)
    }
    Rectangle { x: 18; y: 44; width: 150; height: 3; color: card.task.checked ? "#3F3F3F" : "#A9AAA5" }
    Rectangle { x: 18; y: 44; width: card.task.checked ? 150 : 1; height: 3; color: card.task.checked ? "#FFFA00" : "#222222"; Behavior on width { NumberAnimation { duration: card.reduceMotion ? 0 : 350 } } }
    Text { x: 178; y: 38; text: card.task.checked ? "1/1" : "0/1"; color: card.task.checked ? "#CFCFCF" : "#666666"; font.family: harmonyBold.name; font.pointSize: 10.5 * 0.75; font.weight: Font.Bold; renderType: Text.CurveRendering }
    Text { x: card.bodyWidth + 8; y: 22; text: "COMPLETED"; visible: card.task.checked; color: "#241F0022"; font.family: harmonyBold.name; font.pixelSize: 28; font.weight: Font.Black; renderType: Text.CurveRendering }
    Rectangle {
        id: status; x: card.width - width - 12; y: 17; width: statusText.width + (card.task.checked ? 43 : 26); height: 26; radius: 13
        color: card.task.checked ? "#282828" : statusPointer.containsMouse ? "#FFFFFF" : "#DCDCDA"
        NotesGlyph { x: 12; y: 7; width: 12; height: 12; kind: "check"; ink: "#FFFA00"; visible: card.task.checked }
        Text { id: statusText; x: card.task.checked ? 30 : 13; y: 6; text: card.task.checked ? "已完成" : "进行中"; color: card.task.checked ? "#FFFA00" : "#222222"; font.family: harmonyMedium.name; font.pointSize: 11.5 * 0.75; renderType: Text.CurveRendering }
        MouseArea { id: statusPointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: card.toggled() }
    }
    Shape {
        x: card.width - 7; y: -3; width: 10; height: 10; visible: card.task.flagged && !card.task.checked
        ShapePath { strokeWidth: -1; fillColor: "#F2A23A"; PathSvg { path: "M5 0L10 5L5 10L0 5Z" } }
    }
    Row {
        x: card.bodyWidth + 8; y: 3; spacing: 4; opacity: hover.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: card.reduceMotion ? 0 : 120 } }
        Repeater {
            model: [{kind: "flag", action: 0}, {kind: "up", action: -1}, {kind: "down", action: 1}, {kind: "close", action: 2}]
            delegate: Item {
                required property var modelData; width: 20; height: 20
                Rectangle { anchors.fill: parent; color: actionMouse.containsMouse ? "#33000000" : "transparent" }
                NotesGlyph { anchors.centerIn: parent; width: 11; height: 11; kind: modelData.kind; ink: "#333333" }
                MouseArea { id: actionMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { if (modelData.action === 0) card.flagged(); else if (modelData.action === 2) card.removed(); else card.moved(modelData.action) } }
            }
        }
    }
    HoverHandler { id: hover }
}
