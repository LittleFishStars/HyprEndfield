import QtQuick
import QtQuick.Shapes
import "MenuGeometry.js" as Geometry

Item {
    id: card
    required property int cardIndex
    required property string label
    required property string glyph
    property real leanX: 0
    property real leanY: 0
    readonly property var spec: Geometry.data.cards[cardIndex]
    property real hoverLift: pointer.containsMouse ? 1 : 0
    Behavior on hoverLift {
        NumberAnimation {
            duration: 200
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [.22, 1, .36, 1, 1, 1]
        }
    }
    signal triggered
    width: 440
    height: 172
    x: spec.x
    y: spec.y

    Item {
        anchors.fill: parent
        scale: pointer.pressed ? 0.97 : 1
        transform: Translate {
            x: -2 * card.hoverLift
            y: -3 * card.hoverLift
        }
        Behavior on scale {
            NumberAnimation {
                duration: 140
                easing.type: Easing.BezierSpline
                easing.bezierCurve: [.22, 1, .36, 1, 1, 1]
            }
        }
        MenuArt {
            x: card.spec.shadow[0] + card.leanX * 44
            y: card.spec.shadow[1] + card.leanY * 44
            width: 440
            height: 172
            asset: "card-" + card.cardIndex + "-back"
        }
        MenuArt {
            anchors.fill: parent
            asset: "card-" + card.cardIndex + "-face"
        }
        MenuArt {
            anchors.fill: parent
            asset: "card-" + card.cardIndex + "-hover"
            opacity: pointer.containsMouse ? 1 : 0
            Behavior on opacity {
                NumberAnimation {
                    duration: 200
                }
            }
        }
        HudGlyph {
            x: card.spec.centre[0] - 27
            y: card.spec.centre[1] - 47
            width: 54
            height: 54
            kind: card.glyph
            ink: "#313131"
            rotation: -2
        }
        Text {
            x: card.spec.centre[0] - 75
            y: card.spec.text.y[0] - 34
            width: 150
            height: 34
            text: card.label
            color: "#313131"
            font.family: harmonyMedium.name
            font.pixelSize: card.spec.text.size
            horizontalAlignment: Text.AlignHCenter
            rotation: -2
            renderType: Text.CurveRendering
        }
    }
    Shape {
        id: hitShape
        anchors.fill: parent
        visible: false
        containsMode: Shape.FillContains
        ShapePath {
            strokeWidth: -1
            PathSvg {
                path: card.spec.path
            }
        }
    }
    MouseArea {
        id: pointer
        anchors.fill: parent
        containmentMask: hitShape
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: card.triggered()
    }
}
