import QtQuick
import QtQuick.Dialogs
import QtCore
import HyprEndfield.Projection 1.0
import "Appearance.js" as Appearance

Item {
    id: workspace
    property bool opened: false
    property bool presented: false
    property bool dotted: true
    property real darkness: .63
    property bool confirmClear: false
    property alias ink: ink
    visible: opened && presented
    onVisibleChanged: if (visible) forceActiveFocus()
    onOpenedChanged: { if (!opened) { toolbar.popup = ""; confirmClear = false; } appController.setProjectionActive(opened); }
    function chooseFile() { mediaDialog.open() }
    function chooseMedia() { opened = true; toolbar.chooseMedia() }
    Settings { category: "projection"; property alias penColor: ink.penColor; property alias penWidth: ink.penWidth; property alias erasing: ink.erasing; property alias dotted: workspace.dotted; property alias darkness: workspace.darkness }
    Rectangle { anchors.fill: parent; color: Qt.rgba(0, 0, 0, workspace.darkness); visible: workspace.dotted }
    Canvas { anchors.fill: parent; visible: workspace.dotted; onWidthChanged: requestPaint(); onHeightChanged: requestPaint()
        onPaint: { const ctx = getContext("2d"); ctx.clearRect(0, 0, width, height); ctx.fillStyle = "#24ffffff"; for (let y = 12; y < height; y += 36) for (let x = 12; x < width; x += 36) ctx.fillRect(x, y, 2, 2); }
    }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton; cursorShape: Qt.CrossCursor
        onPressed: mouse => { toolbar.popup = ""; if (mouse.button === Qt.RightButton) ink.erasing = !ink.erasing; else ink.beginStroke(Qt.point(mouse.x, mouse.y)); }
        onPositionChanged: mouse => { if (pressedButtons & Qt.LeftButton) ink.moveStroke(Qt.point(mouse.x, mouse.y)); }
        onReleased: ink.endStroke()
        onWheel: wheel => { ink.penWidth = Math.max(1, Math.min(80, ink.penWidth + wheel.angleDelta.y / 120)); wheel.accepted = true; }
    }
    ListModel { id: mediaModel; dynamicRoles: true }
    function syncMedia() {
        const values = ink.media
        for (let i = mediaModel.count - 1; i >= 0; --i) if (!values.some(item => item.id === mediaModel.get(i).mediaId)) mediaModel.remove(i)
        for (const item of values) {
            let i = 0; while (i < mediaModel.count && mediaModel.get(i).mediaId !== item.id) ++i
            if (i === mediaModel.count) mediaModel.append({mediaId: item.id, entry: item})
            else mediaModel.setProperty(i, "entry", item)
        }
    }
    Repeater { model: mediaModel
        Rectangle {
            id: card; required property var entry
            x: entry.x; y: entry.y; width: entry.width; height: entry.height; z: 1
            color: "#181818"; border.color: "#555555"
            Rectangle { width: parent.width; height: 24; color: "#262626" }
            Text { x: 8; y: 7; width: parent.width - 40; height: 11; text: Appearance.recording() ? "REDACTED" : card.entry.name; elide: Text.ElideRight; color: "#CCCCCC"; font.family: harmonyRegular.name; font.pixelSize: Math.round(10.5); renderType: Text.CurveRendering }
            MouseArea { width: parent.width - 24; height: 24; drag.target: card; drag.minimumX: 0; drag.minimumY: 0; drag.maximumX: workspace.width - card.width; drag.maximumY: workspace.height - card.height; cursorShape: Qt.SizeAllCursor
                onPressed: card.z = 1.5
                onReleased: ink.placeMedia(card.entry.id, card.x, card.y, card.width, card.height)
            }
            CalendarButton { x: parent.width - 23; y: 1; width: 22; height: 22; kind: "icon"; glyph: "×"; onClicked: ink.removeMedia(card.entry.id) }
            MediaPreview { x: 5; y: 25; width: parent.width - 10; height: parent.height - 30; source: Appearance.recording() ? "" : card.entry.source; kind: card.entry.kind; presented: workspace.visible; projectionStyle: true }
            MouseArea { x: parent.width - 14; y: parent.height - 14; width: 14; height: 14; cursorShape: Qt.SizeFDiagCursor
                property point start; property size original
                onPressed: mouse => { start = mapToItem(workspace, mouse.x, mouse.y); original = Qt.size(card.width, card.height); }
                onPositionChanged: mouse => { if (pressed) { const p = mapToItem(workspace, mouse.x, mouse.y); card.width = Math.max(110, Math.min(workspace.width - card.x, original.width + p.x - start.x)); card.height = Math.max(100, Math.min(workspace.height - card.y, original.height + p.y - start.y)); } }
                onReleased: ink.placeMedia(card.entry.id, card.x, card.y, card.width, card.height)
                Text { anchors.centerIn: parent; text: "⌟"; color: "#888888" }
            }
        }
    }
    ProjectionCanvas { id: ink; objectName: "projectionInk"; anchors.fill: parent; z: 2; enabled: false; onMediaChanged: workspace.syncMedia() }
    DropArea { anchors.fill: parent; onEntered: drag => drag.accepted = drag.hasUrls && drag.urls.length === 1; onDropped: drop => { ink.importMedia(drop.urls[0]); drop.accepted = true; } }
    ProjectionTools { id: toolbar; workspace: parent; fullScreen: true; x: Math.max(6, (parent.width - width) / 2); y: Math.min(64, Math.max(6, parent.height - height - 8)); z: 100 }
    Text { x: Math.max(10, parent.width / 2 - 280); y: toolbar.y + toolbar.height + 18; width: Math.min(560, parent.width - 20); horizontalAlignment: Text.AlignHCenter; text: ink.message; color: "#DDDDDD"; font.family: harmonyRegular.name; font.pixelSize: 12; renderType: Text.CurveRendering; z: 99 }
    FileDialog { id: mediaDialog; title: qsTr("图片/视频"); nameFilters: [qsTr("图片与视频 (*.png *.jpg *.jpeg *.webp *.gif *.bmp *.mp4 *.mkv *.webm *.mov *.avi *.m4v *.ogv)"), qsTr("所有文件 (*)")]; onAccepted: ink.importMedia(selectedFile) }
    Rectangle { anchors.centerIn: parent; width: 360; height: 116; visible: workspace.confirmClear; color: "#242424"; border.color: "#777777"; z: 200
        MouseArea { anchors.fill: parent }
        Text { x: 16; y: 16; text: qsTr("清空投影画布？"); color: "#EEEEEE"; font.family: harmonyBold.name; font.pixelSize: 15; renderType: Text.CurveRendering }
        Text { x: 16; y: 44; text: qsTr("源文件保持不变。"); color: "#999999"; font.family: harmonyRegular.name; font.pixelSize: 12; renderType: Text.CurveRendering }
        CalendarButton { x: 190; y: 78; width: 70; title: qsTr("取消"); kind: "secondary"; onClicked: workspace.confirmClear = false }
        CalendarButton { x: 270; y: 78; width: 74; title: qsTr("清空"); onClicked: { ink.clearContent(); workspace.confirmClear = false; } }
    }
    Keys.onEscapePressed: event => { if (confirmClear) confirmClear = false; else if (toolbar.popup) toolbar.popup = ""; else opened = false; event.accepted = true; }
}
