import QtQuick
import QtQuick.Shapes

Item {
    id: hazard
    property real period: 12 * Math.SQRT2
    property int cycle: 1200
    property bool moving: visible && !settingsController.reduceMotion && !settingsController.lowPowerVisualMode
    property real progress: 0
    clip: true
    NumberAnimation on progress { from: 0; to: 1; duration: hazard.cycle; loops: Animation.Infinite; running: hazard.moving }
    Shape {
        width: hazard.width + hazard.period; height: hazard.height; x: -hazard.period * hazard.progress; preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1; fillColor: "#F97501"
            PathSvg { path: { let p = "", h = hazard.height; for (let x = -h - hazard.period; x < hazard.width + hazard.period * 2; x += hazard.period) p += "M" + x + " " + h + "h" + hazard.period / 2 + "l" + h + " -" + h + "h-" + hazard.period / 2 + "Z"; return p } }
        }
    }
}
