import QtQuick
import QtQuick.Shapes
import HyprEndfield.Terminal 1.0

// Keep the original HUD header and central plane; only module content is new.
Item {
    id: page
    width: 590; height: 510
    property bool presented: false
    property real rasterScale: 2
    onPresentedChanged: {
        if (presented) { terminalSession.startDefault(); terminal.activate(); }
        else terminal.deactivate();
    }
    Item {
        width: 590; height: 81
        Row {
            y: 1.75; spacing: 6
            Repeater {
                model: ["// HUD", "/", "TERMINAL"]
                Text {
                    required property string modelData; required property int index
                    text: modelData; color: index === 2 ? "#E6E6E6" : "#8A8A8A"
                    font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; font.letterSpacing: 0.63
                    renderType: Text.CurveRendering
                }
            }
        }
        Text {
            x: 549 - width; y: 1.75; text: "SYS-03"; color: "#8A8A8A"
            font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; font.letterSpacing: 0.63; renderType: Text.CurveRendering
        }
        Item {
            x: 556; y: 3.5; width: 34; height: 7; clip: true
            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                ShapePath { strokeWidth: -1; fillColor: Qt.rgba(1, 1, 1, 0.14); PathSvg { path: { let p = ""; for (let x = -7; x < 41; x += 3.5 * Math.SQRT2) p += "M" + x + " 7h1.414l7 -7h-1.414Z"; return p; } } }
            }
        }
        Text {
            id: title; y: 20; height: 30
            text: "快捷终端"; color: "#F4F4F4"; font.family: harmonyBold.name; font.pixelSize: 26; font.weight: Font.Bold; font.letterSpacing: 2.08
            renderType: Text.CurveRendering
        }
        Text {
            x: title.width + 12; y: title.y + title.baselineOffset - baselineOffset
            text: "TERMINAL"; color: "#8A8A8A"; font.family: harmonyMedium.name; font.pixelSize: 13; font.letterSpacing: 2.6; font.weight: Font.Medium
            renderType: Text.CurveRendering
        }
        Item {
            y: 60; width: 590; height: 9
            Rectangle { y: 4; width: 590; height: 1; color: "#38FFFFFF" }
            Rectangle { y: 3; width: 56; height: 3; color: "#FFFA00" }
            Repeater { model: 10; Rectangle { required property int index; x: 454 + index * 9; width: 1; height: 9; color: "#59FFFFFF" } }
            Rectangle { x: 590 - width; y: -2; width: counter.width + 7; height: 12; color: "#151515" }
            Text {
                id: counter; x: 590 - width; y: -1.5; text: "PTY"
                color: "#FFFA00"; font.family: harmonyBold.name; font.pixelSize: 11; font.weight: Font.Bold; renderType: Text.CurveRendering
            }
        }
    }
    Rectangle {
        x: 0; y: 81; width: 590; height: 399
        color: "#171717"; border.color: "#3A3A3A"; border.width: 1
        TerminalView {
            id: terminal; objectName: "terminalView"
            anchors.fill: parent; anchors.margins: 12
            session: terminalSession
            renderScale: page.rasterScale
            enabled: page.presented
        }
    }
    Text {
        x: 0; y: 492; width: 290; elide: Text.ElideRight
        text: terminalSession.status || "BASH"; color: "#8A8A8A"
        font.family: harmonyRegular.name; font.pixelSize: 10; renderType: Text.CurveRendering
    }
    Text {
        x: 296; y: 492; width: 294; horizontalAlignment: Text.AlignRight
        text: "Ctrl+Shift+C / V  复制 / 粘贴"; color: "#8A8A8A"
        font.family: harmonyRegular.name; font.pixelSize: 10; renderType: Text.CurveRendering
    }
}
