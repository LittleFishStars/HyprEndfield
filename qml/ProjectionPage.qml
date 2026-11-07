import QtQuick
import QtQuick.Shapes
import "Appearance.js" as Appearance

Item {
    id: page; width: 590; height: 510
    required property var workspace
    SettingsHeader { title: qsTr("投影"); english: "PROJECTION"; code: "MOD-09"; count: page.workspace.dotted ? "BG ON" : "BG OFF" }
    Canvas { id: preview; y: 81; width: 590; height: 150
        onPaint: {
            const c = getContext("2d"); c.fillStyle = page.workspace.dotted ? "#141414" : "#1C1C1C"; c.fillRect(0, 0, width, height)
            if (page.workspace.dotted) { c.fillStyle = "#555555"; for (let y = 7; y < height; y += 14) for (let x = 7; x < width; x += 14) { c.beginPath(); c.arc(x, y, 1.25, 0, Math.PI * 2); c.fill(); } }
            else { c.fillStyle = "#232323"; for (let y = 0; y < height; y += 20) for (let x = 0; x < width; x += 20) { c.fillRect(x, y, 10, 10); c.fillRect(x + 10, y + 10, 10, 10); } }
            c.beginPath(); c.moveTo(40, 110); c.bezierCurveTo(120, 20, 200, 140, 290, 60); c.bezierCurveTo(380, -20, 450, 40, 540, 100); c.strokeStyle = page.workspace.ink.penColor; c.lineWidth = page.workspace.ink.penWidth / 4 + 1; c.lineCap = "round"; c.stroke()
            c.strokeStyle = "#555555"; c.lineWidth = 1; c.strokeRect(.5, .5, width - 1, height - 1)
        }
        Connections { target: page.workspace; function onDottedChanged() { preview.requestPaint() } }
        Connections { target: page.workspace.ink; function onPenChanged() { preview.requestPaint() } }
    }
    ProjectionTools { y: 241; workspace: page.workspace; z: 20 }
    HudSettingsRow { y: 293; title: qsTr("画笔粗细"); mode: "slider"; sliderMinimum: 4; sliderMaximum: 48; sliderValue: page.workspace.ink.penWidth; sliderStep: 1; sliderFormat: "pixels"; onSliderMoved: (value, final) => page.workspace.ink.penWidth = value }
    HudSettingsRow { y: 349; title: qsTr("点阵背景"); mode: "toggle"; switched: page.workspace.dotted; onToggleRequested: value => page.workspace.dotted = value }
    CalendarButton { x: (590 - width) / 2; y: 411; width: 140; height: 34; title: qsTr("打开投影"); onClicked: page.workspace.opened = true }
}
