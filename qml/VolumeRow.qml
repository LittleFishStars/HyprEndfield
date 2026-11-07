import QtQuick
import QtQuick.Shapes

// EndfieldHUD HUDRowPlate / HUDSheen, compact and application row variants.
Item {
    id: row
    property string title: ""
    property string subtitle: ""
    property bool applicationRow: false
    property bool reduceMotion: false
    property string icon: ""
    property real shift: hover.hovered ? 3 : 0
    property real brighten: hover.hovered ? 0.07 : 0
    property real bandX: -width * 1.1
    width: 590; height: applicationRow ? 54 : 44
    transform: Translate { x: row.shift }
    Behavior on shift { NumberAnimation { duration: row.reduceMotion ? 0 : 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] } }
    Behavior on brighten { NumberAnimation { duration: row.reduceMotion ? 0 : 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1] } }
    HoverHandler {
        id: hover
        onHoveredChanged: {
            sweep.stop();
            if (hovered && !row.reduceMotion) sweep.start();
        }
    }
    NumberAnimation {
        id: sweep; target: row; property: "bandX"; from: -row.width * 1.1; to: row.width * 1.1
        duration: 700; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1]
    }
    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: 0; y2: row.height
                GradientStop { position: 0; color: "#E4E5E0" }
                GradientStop { position: 0.55; color: "#D3D4CF" }
                GradientStop { position: 1; color: "#C6C7C2" }
            }
            PathSvg { path: "M0 0H" + (row.width - 11) + "L" + row.width + " 11V" + row.height + "H0Z" }
        }
        ShapePath {
            strokeWidth: -1; fillColor: Qt.rgba(1, 1, 1, row.brighten)
            PathSvg { path: "M0 0H" + (row.width - 11) + "L" + row.width + " 11V" + row.height + "H0Z" }
        }
        ShapePath {
            strokeWidth: 1; strokeColor: Qt.rgba(1, 1, 1, 0.75); fillColor: "transparent"
            PathSvg { path: "M0 .5H" + (row.width - 11) }
        }
    }
    Shape {
        anchors.fill: parent
        visible: sweep.running
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1
            fillGradient: LinearGradient {
                x1: row.bandX; y1: row.height * 0.3; x2: row.bandX + row.width; y2: row.height * 0.7
                GradientStop { position: 0.35; color: "#00FFFFFF" }
                GradientStop { position: 0.5; color: "#47FFFFFF" }
                GradientStop { position: 0.65; color: "#00FFFFFF" }
            }
            PathSvg { path: "M0 0H" + (row.width - 11) + "L" + row.width + " 11V" + row.height + "H0Z" }
        }
    }
    Rectangle {
        y: (row.height - height) / 2; width: 3; height: hover.hovered ? row.height - 10 : row.height - 18
        color: hover.hovered ? "#E9CF12" : "#2A2A2A"
        Behavior on height { NumberAnimation { duration: row.reduceMotion ? 0 : 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] } }
        Behavior on color { ColorAnimation { duration: row.reduceMotion ? 0 : 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1] } }
    }
    Item {
        x: 18; y: 4; width: tagLabel.width + 52; height: 7
        Rectangle {
            id: tagBlack
            width: tagLabel.width + 8; height: 7; color: "#161616"
            Text {
                id: tagLabel
                x: 4; anchors.verticalCenter: parent.verticalCenter
                text: "ENDFIELD"; color: "#E9E9E9"
                font.family: harmonyBold.name; font.pointSize: 5.5 * 0.75; font.weight: Font.Bold; font.letterSpacing: 0.44
                renderType: Text.CurveRendering
            }
        }
        Item {
            x: tagBlack.width; width: 44; height: 7; clip: true
            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeWidth: -1; fillColor: Qt.rgba(0, 0, 0, 0.42)
                    PathSvg { path: { let p = ""; for (let x = -7; x < 51; x += 3.5 * Math.SQRT2) p += "M" + x + " 7h1.414l7 -7h-1.414Z"; return p; } }
                }
            }
        }
    }
    Rectangle {
        visible: row.applicationRow
        x: 18; y: 11.5; width: 18; height: 18; color: "#4A4A4A"
        Image { anchors.fill: parent; source: row.icon ? "image://audio-icons/" + encodeURIComponent(row.icon) : ""; sourceSize: Qt.size(64, 64); mipmap: true; fillMode: Image.PreserveAspectFit }
    }
    Text {
        x: row.applicationRow ? 44 : 18; y: row.applicationRow ? 11.2 : 15.2
        width: row.applicationRow ? 144 : 170; height: 18.6
        text: row.title; color: "#121212"; elide: Text.ElideRight
        font.family: harmonyMedium.name; font.pointSize: 15.5 * 0.75; font.weight: Font.Medium
        renderType: Text.CurveRendering
    }
    Text {
        visible: row.subtitle !== ""
        x: 18; y: 32.7; width: 170; height: 13.2
        text: row.subtitle; color: "#6C6D68"; elide: Text.ElideRight
        font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering
    }
}
