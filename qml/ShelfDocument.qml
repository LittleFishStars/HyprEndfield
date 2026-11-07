import QtQuick
import QtQuick.Shapes
import QtQuick.Effects

// HUDArchiveTile.docGlyph: 40 × 50, four ruled lines, folded corner, −6°.
Item {
    id: document
    width: 40; height: 50
    property string kind: "txt"
    property string label: "FILE"
    property bool hovered: false
    property bool reduceMotion: false
    rotation: hovered ? -2 : -6
    transform: Translate {
        y: document.hovered ? -3 : 0
        Behavior on y { NumberAnimation { duration: document.reduceMotion ? 0 : 300; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] } }
    }
    Behavior on rotation { NumberAnimation { duration: document.reduceMotion ? 0 : 300; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] } }
    Shape {
        anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
        layer.enabled: GraphicsInfo.api !== GraphicsInfo.Software; layer.textureSize: Qt.size(160, 200)
        layer.effect: MultiEffect { shadowEnabled: true; shadowColor: "black"; shadowOpacity: 0.25; shadowVerticalOffset: 6; shadowBlur: 1; blurMax: 6 }
        ShapePath { strokeWidth: -1; fillColor: "#F4F4F2"; PathSvg { path: "M0 0H28L40 11V50H0Z" } }
    }
    Repeater {
        model: 4
        Rectangle { required property int index; x: 6; y: 10 + index * 4; width: 24; height: 1; color: "#9B9B9B" }
    }
    Text {
        x: 0; y: 34; width: 40; height: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        text: document.label; elide: Text.ElideRight
        color: ({pdf: "#E5383B", img: "#22A6F2", zip: "#8A6D00", vid: "#7A3CC9", txt: "#333333"})[document.kind] || "#333333"
        font.family: harmonyBold.name; font.pixelSize: 8; font.weight: Font.Black; font.letterSpacing: 0.48
        renderType: Text.CurveRendering
    }
}
