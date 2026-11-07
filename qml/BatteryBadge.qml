import QtQuick
import QtQuick.Shapes
import QtQuick.Window

// ChargeIndicatorView.swift: original circle/compact geometry and 260 ms morph.
// The requested interaction is click-to-expand instead of source hover.
Item {
    id: badge

    property bool expanded: false
    property bool reduceMotion: false
    property bool presented: false
    property var reading: batteryController.state
    property bool alwaysExpanded: false
    property bool manageMonitoring: true
    readonly property color foreground: "#f2f2f2"
    readonly property color level: reading.percentage < 0 ? "#737373" : reading.percentage < 20 ? "#fa5957" : reading.percentage <= 70 ? "#f5cc40" : "#40de6e"
    property real morph: expanded || alwaysExpanded ? 1 : 0

    width: 300
    height: 84
    onPresentedChanged: {
        if (manageMonitoring) batteryController.presented = presented;
        if (!presented)
            expanded = false;

    }
    Accessible.role: Accessible.Button
    Accessible.name: qsTr("电池容量，") + reading.status + (reading.estimated ? qsTr("，容量为按标称电压换算的估计值") : "")
    Accessible.onPressAction: {
        if (reading.available)
            expanded = !expanded;

    }

    Connections {
        function onStateChanged() {
            if (!badge.reading.available)
                badge.expanded = false;

        }

        target: batteryController
    }

    Rectangle {
        id: body

        x: 131 - 113 * badge.morph
        y: 23
        width: 38 + 226 * badge.morph
        height: 38
        radius: 19
        color: "#fc1b1b1b"
        border.width: hit.containsMouse ? 1.5 : 0.5
        border.color: hit.containsMouse ? "#faf7d52c" : "#0bf2f2f2"

        MouseArea {
            id: hit

            anchors.fill: parent
            enabled: badge.presented && badge.reading.available
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.PointingHandCursor
            onClicked: badge.expanded = !badge.expanded
        }

    }

    Rectangle {
        x: 150 - 113 * badge.morph - width / 2
        y: 42 - height / 2
        width: 23 - 6 * badge.morph
        height: 23 - 3 * badge.morph
        radius: 11.5 - 8.5 * badge.morph
        color: Qt.rgba(0.95, 0.95, 0.95, 1 - 0.81 * badge.morph)
    }

    Shape {
        x: 150 - 113 * badge.morph - 12
        y: 30
        width: 24
        height: 24
        scale: 0.69 - 0.09 * badge.morph
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeWidth: -1
            fillColor: badge.expanded || badge.alwaysExpanded ? badge.foreground : "#1b1b1b"

            PathSvg {
                path: "M14 2.8L5.5 13.3H11L9.5 21.2L18.5 10.2H13Z"
            }

        }

    }

    Row {
        x: 83 - 28 * badge.morph
        y: 33
        opacity: badge.morph

        Text {
            text: (badge.reading.estimated ? "≈" : "") + badge.reading.current
            color: badge.foreground
            font.family: harmonyBold.name
            font.pointSize: 12.5 * 72 / (Screen.logicalPixelDensity * 25.4)
            renderType: Text.CurveRendering
        }

        Text {
            anchors.baseline: parent.children[0].baseline
            text: badge.reading.full ? "/" + badge.reading.full : ""
            color: "#78f2f2f2"
            font.family: harmonyMedium.name
            font.pointSize: 9.5 * 72 / (Screen.logicalPixelDensity * 25.4)
            renderType: Text.CurveRendering
        }

        Text {
            anchors.baseline: parent.children[0].baseline
            text: " " + badge.reading.unit
            color: "#78f2f2f2"
            font.family: harmonyMedium.name
            font.pointSize: 7.5 * 72 / (Screen.logicalPixelDensity * 25.4)
            renderType: Text.CurveRendering
        }

    }

    Text {
        x: 192 + 13 * badge.morph
        y: 32.5
        width: 40
        height: 20
        opacity: badge.morph
        text: badge.reading.trailing !== undefined ? badge.reading.trailing : badge.reading.percentage < 0 ? "—" : badge.reading.percentage + "%"
        horizontalAlignment: Text.AlignRight
        color: badge.foreground
        font.family: harmonyBold.name
        font.pixelSize: 13
        renderType: Text.CurveRendering
    }

    Item {
        x: 225 + 24 * badge.morph
        y: 28
        width: 28
        height: 28
        opacity: badge.morph

        Rectangle {
            x: 3
            y: 3
            width: 22
            height: 22
            radius: 11
            color: "transparent"
            border.color: Qt.rgba(badge.level.r, badge.level.g, badge.level.b, 0.19)
            border.width: 2
        }

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: "transparent"
                strokeColor: badge.level
                strokeWidth: 2
                capStyle: ShapePath.RoundCap

                PathAngleArc {
                    centerX: 14
                    centerY: 14
                    radiusX: 11
                    radiusY: 11
                    startAngle: -90
                    sweepAngle: 360 * badge.reading.progress
                }

            }

            ShapePath {
                fillColor: "transparent"
                strokeColor: "#c7f2f2f2"
                strokeWidth: 1.15
                joinStyle: ShapePath.RoundJoin

                PathSvg {
                    path: "M9.2 8H18.8Q19.5 8 19.5 8.7V16.3Q19.5 17 18.8 17H9.2Q8.5 17 8.5 16.3V8.7Q8.5 8 9.2 8ZM7 19H21M8.5 17L7 19M19.5 17L21 19"
                }

            }

        }

    }

    Behavior on morph {
        NumberAnimation {
            duration: badge.reduceMotion ? 0 : 260
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.18, 0.78, 0.22, 1, 1, 1]
        }

    }

}
