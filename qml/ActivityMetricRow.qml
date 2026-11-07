import QtQuick
import QtQuick.Shapes
import "ActivityFormat.js" as Format

// EndfieldHUD ActivityRow: all positions and graph geometry use source pixels.
Item {
    id: row
    required property int metric
    required property var history
    required property var latest
    property bool reduceMotion: false
    readonly property bool two: metric >= 3
    readonly property color accent: metric === 2 || metric === 3 ? "#22A6F2" : "#FFFA00"
    readonly property var lines: Format.readout(latest, metric)
    readonly property var traces: Format.series(history, metric)
    property real shift: hover.hovered ? 2 : 0
    property real brighten: hover.hovered ? 0.025 : 0
    property real bandX: -width * 1.1
    width: 590; height: 64
    transform: Translate { x: row.shift }
    Behavior on shift { NumberAnimation { duration: row.reduceMotion ? 0 : 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] } }
    Behavior on brighten { NumberAnimation { duration: row.reduceMotion ? 0 : 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1] } }
    HoverHandler {
        id: hover
        onHoveredChanged: { sweep.stop(); if (hovered && !row.reduceMotion) sweep.start(); }
    }
    NumberAnimation {
        id: sweep; target: row; property: "bandX"; from: -row.width * 1.1; to: row.width * 1.1
        duration: 700; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1]
    }
    Shape {
        anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: 0; y2: 64
                GradientStop { position: 0; color: "#232323" }
                GradientStop { position: 1; color: "#1A1A1A" }
            }
            PathSvg { path: "M0 0H579L590 11V64H0Z" }
        }
        ShapePath { strokeWidth: 1; strokeColor: "#343434"; fillColor: "transparent"; PathSvg { path: "M579 .5H.5V63.5H589.5V11" } }
        ShapePath { strokeWidth: -1; fillColor: Qt.rgba(1, 1, 1, row.brighten); PathSvg { path: "M0 0H579L590 11V64H0Z" } }
    }
    Shape {
        anchors.fill: parent; visible: sweep.running; preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1
            fillGradient: LinearGradient {
                x1: row.bandX; y1: 19.2; x2: row.bandX + 590; y2: 44.8
                GradientStop { position: 0.35; color: "#00FFFFFF" }
                GradientStop { position: 0.5; color: "#12FFFFFF" }
                GradientStop { position: 0.65; color: "#00FFFFFF" }
            }
            PathSvg { path: "M0 0H579L590 11V64H0Z" }
        }
    }
    Rectangle { x: 0; y: 10; width: 3; height: 44; color: row.accent }
    Text {
        x: 18; y: 15.5; width: 118; height: 17.25; text: ["GPU", "CPU", "RAM", "网络", "磁盘 I/O"][row.metric]
        color: "#F2F2F2"; font.family: harmonyMedium.name; font.pointSize: 15 * 0.75; font.weight: Font.Medium
        renderType: Text.CurveRendering
    }
    Text {
        x: 18; y: 37.75; width: 118; height: 9.5; text: ["总负载", "总负载", "已用 / 总计", "上传 / 下载", "读取 / 写入"][row.metric]
        color: "#8A8A8A"; font.family: harmonyRegular.name; font.pointSize: 9.5 * 0.75; font.letterSpacing: 0.95; renderType: Text.CurveRendering
    }
    Item {
        x: 148; y: 10; width: 296; height: 44
        Repeater { model: 4; Rectangle { required property int index; x: Math.round(296 * (index + 1) / 5) - 0.5; width: 1; height: 44; color: "#2E2E2E" } }
        Rectangle { y: 21.5; width: 296; height: 1; color: "#2E2E2E" }
        Repeater {
            model: row.traces.length
            Item {
                id: trace
                required property int index
                readonly property var geometry: Format.paths(row.traces[index])
                readonly property color ink: row.two && index === 0 ? (row.metric === 3 ? "#FFFA00" : "#22A6F2") : row.accent
                anchors.fill: parent
                Shape {
                    anchors.fill: parent; opacity: row.two && trace.index === 0 ? 0.25 : 0.35; preferredRendererType: Shape.CurveRenderer
                    ShapePath { strokeWidth: -1; fillColor: trace.ink; PathSvg { path: trace.geometry.area } }
                }
                Shape {
                    anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
                    ShapePath { strokeWidth: 1.6; strokeColor: trace.ink; fillColor: "transparent"; capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin; PathSvg { path: trace.geometry.line } }
                }
            }
        }
    }
    Text {
        x: 456; y: row.two ? 11.7 : 13.6; width: 120; height: row.two ? 20.3 : 20.9
        text: row.lines[0]; color: row.two ? "#FFFA00" : row.accent; horizontalAlignment: Text.AlignRight
        font.family: harmonyRegular.name; font.pointSize: (row.two ? 14 : 19) * 0.75; font.features: { "tnum": 1 }; renderType: Text.CurveRendering
    }
    Text {
        x: 456; y: row.two ? 32 : 34.5; width: 120; height: row.two ? 20.3 : 15.75
        text: row.lines[1]; color: row.two ? "#22A6F2" : "#8A8A8A"; horizontalAlignment: Text.AlignRight
        font.family: harmonyRegular.name; font.pointSize: (row.two ? 14 : 10.5) * 0.75; font.features: { "tnum": 1 }; renderType: Text.CurveRendering
    }
}
