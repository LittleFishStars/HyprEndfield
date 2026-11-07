import QtQuick
import QtQuick.Shapes
import "Appearance.js" as Appearance

// HUDDesignRedaction.bar: fixed hatch colours in either theme, rdIn .35 s.
Item {
    id: root
    property bool labelled: width >= 70
    property bool reduceMotion: typeof settingsController === "undefined" || settingsController.reduceMotion
    property real reveal: 1
    height: 13
    onVisibleChanged: if (visible) { reveal = reduceMotion ? 1 : 0; if (!reduceMotion) entrance.restart() }
    NumberAnimation { id: entrance; target: root; property: "reveal"; to: 1; duration: 350; easing.type: Easing.BezierSpline; easing.bezierCurve: [.2, .8, .2, 1, 1, 1] }
    Item {
        width: root.width * root.reveal; height: root.height; clip: true
        Rectangle {
            width: root.width; height: root.height; color: "#141414"; border.color: "black"
            Shape {
                anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeWidth: -1; fillColor: "#2B2B2B"
                    PathSvg { path: { let p = "", h = root.height; for (let x = -h - 6 * Math.SQRT2; x < root.width + 6 * Math.SQRT2; x += 6 * Math.SQRT2) p += "M" + x + " " + h + "h" + 3 * Math.SQRT2 + "l" + h + " -" + h + "h-" + 3 * Math.SQRT2 + "Z"; return p } }
                }
            }
            Text { anchors.centerIn: parent; visible: root.labelled; text: "[ " + qsTr("已隐藏") + " ]"; color: Appearance.accent("#FFFA00"); font.family: harmonyBold.name; font.pixelSize: 8; font.weight: Font.Bold; font.letterSpacing: 1.6; renderType: Text.CurveRendering }
        }
    }
}
