import QtQuick
import "Appearance.js" as Appearance

// ProjectionCanvas toolbar and ProjectionControls.swift floating toolbar.
Rectangle {
    id: bar
    required property var workspace
    property bool fullScreen: false
    property var ink: workspace.ink
    property string popup: ""
    readonly property var penColors: [Appearance.accent("#FFFA00"), "#FAFAFA", "#22A6F2", "#E5383B", "#8CE03C", "#111111"]
    width: fullScreen ? tools.width + 24 : 590; height: fullScreen ? 46 : 42
    color: Appearance.tone("#202020"); border.color: Appearance.tone("#444444")
    function chooseMedia() { mediaSources.visible = !mediaSources.visible }
    function act(action) {
        if (action === "pen") ink.erasing = false
        else if (action === "eraser") ink.erasing = true
        else if (action === "background") workspace.dotted = !workspace.dotted
        else if (action === "media") workspace.chooseMedia()
        else popup = popup === action ? "" : action
    }
    Row {
        id: tools; x: bar.fullScreen ? 12 : 6; anchors.verticalCenter: parent.verticalCenter; spacing: bar.fullScreen ? 8 : 6
        Repeater { model: 7
            Rectangle { required property int index; width: 22; height: 22; anchors.verticalCenter: parent.verticalCenter; color: index < 6 ? bar.penColors[index] : bar.ink.penColor; border.width: 2; border.color: !bar.ink.erasing && String(bar.ink.penColor).toLowerCase() === String(color).toLowerCase() ? "white" : "#666666"
                Text { anchors.centerIn: parent; visible: index === 6; text: "+"; color: "white"; font.pixelSize: 16 }
                MouseArea { anchors.fill: parent; onClicked: { if (index < 6) { bar.ink.penColor = bar.penColors[index]; bar.ink.erasing = false; } else bar.act("color"); } }
            }
        }
        Item { width: 7; height: 22; anchors.verticalCenter: parent.verticalCenter; Rectangle { x: 3; width: 1; height: 22; color: Appearance.tone("#555555") } }
        Repeater { model: bar.fullScreen ? ["pen", "eraser", "brush", "background", "appearance", "media"] : ["pen", "eraser", "background", "media"]
            Rectangle {
                required property string modelData
                width: modelData === "brush" ? 84 : 30; height: 30; anchors.verticalCenter: parent.verticalCenter
                readonly property bool chosen: modelData === "pen" && !bar.ink.erasing || modelData === "eraser" && bar.ink.erasing || modelData === "background" && bar.workspace.dotted
                color: chosen ? Appearance.accent("#FFFA00") : Appearance.tone("#202020"); border.color: Appearance.tone("#555555")
                SourceGlyph { anchors.centerIn: parent; width: 16; height: 16; kind: modelData === "media" ? "image" : modelData === "background" ? "grid" : modelData === "appearance" ? "eye" : modelData; ink: parent.chosen ? "#111111" : "#EEEEEE" }
                Text { anchors.centerIn: parent; text: modelData === "brush" ? Math.round(bar.ink.penWidth) + " PX ▾" : ""; color: parent.chosen ? "#111111" : "#EEEEEE"; font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering }
                MouseArea { anchors.fill: parent; onClicked: bar.act(modelData) }
            }
        }
        CalendarButton { visible: bar.fullScreen; width: 64; height: 26; anchors.verticalCenter: parent.verticalCenter; title: qsTr("清空"); kind: "secondary"; enabled: bar.ink.strokeCount > 0 || bar.ink.media.length > 0; onClicked: bar.workspace.confirmClear = true }
        CalendarButton { visible: bar.fullScreen; width: 64; height: 26; anchors.verticalCenter: parent.verticalCenter; title: qsTr("返回"); onClicked: bar.workspace.opened = false }
    }
    CalendarButton { visible: !bar.fullScreen; x: parent.width - width - 6; anchors.verticalCenter: parent.verticalCenter; width: 96; title: qsTr("全部清除"); kind: "secondary"; enabled: bar.ink.strokeCount > 0 || bar.ink.media.length > 0; onClicked: bar.popup = "clear" }
    ClipboardConfirmBar { visible: !bar.fullScreen && bar.popup === "clear"; x: -15; y: bar.height + 6; title: qsTr("清空投影画布？"); message: qsTr("源文件保持不变。"); onAccepted: { bar.ink.clearContent(); bar.popup = ""; } onCancelled: bar.popup = ""; z: 35 }
    Rectangle { x: 174; y: parent.height + 4; width: 220; height: 248; visible: bar.popup === "color"; color: Appearance.tone("#202020"); border.color: Appearance.tone("#555555"); z: 30
        ColorWheel { x: 14; y: 12; selectedColor: bar.ink.penColor; onChosen: color => { bar.ink.penColor = color; bar.ink.erasing = false; } }
    }
    Rectangle { x: 220; y: parent.height + 4; width: 260; height: bar.popup === "brush" ? 70 : 108; visible: bar.popup === "brush" || bar.popup === "appearance"; color: Appearance.tone("#202020"); border.color: Appearance.tone("#555555"); z: 30
        Text { x: 12; y: 10; text: bar.popup === "brush" ? qsTr("画笔粗细") : qsTr("背景亮度"); color: "#EEEEEE"; font.family: harmonyRegular.name; font.pixelSize: 12; renderType: Text.CurveRendering }
        HudSlider { x: 12; y: 32; width: 236; height: 24; minimum: bar.popup === "brush" ? 1 : 0; maximum: bar.popup === "brush" ? 80 : 100; stepSize: 1; valueFormat: bar.popup === "brush" ? "pixels" : "percent"; value: bar.popup === "brush" ? bar.ink.penWidth : (1 - bar.workspace.darkness) * 100; onValueModified: (value, final) => { if (bar.popup === "brush") bar.ink.penWidth = value; else bar.workspace.darkness = 1 - value / 100; } }
        Text { x: 12; y: 70; width: 236; wrapMode: Text.Wrap; visible: bar.popup === "appearance"; text: qsTr("模糊程度：暂不支持单独调整"); color: "#999999"; font.family: harmonyRegular.name; font.pixelSize: 10; renderType: Text.CurveRendering }
    }
    MediaSourceMenu { id: mediaSources; x: Math.min(300, bar.width - width); y: parent.height + 4; visible: false; onChooseFile: bar.workspace.chooseFile(); onChosen: source => bar.ink.importMedia(source) }
}
