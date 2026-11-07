import QtQuick
import QtQuick.Shapes

// EndfieldHUD HUDDesignButtons.swift / HUDDesignIcons.swift / NowPlayingPlayButton.
Item {
    id: button
    property string kind: "play"
    property string label: ""
    property bool selected: false
    property bool reduceMotion: false
    readonly property bool playButton: kind === "play" || kind === "pause"
    readonly property bool transport: playButton || kind === "previous" || kind === "next"
    readonly property color ink: playButton || selected ? "#111111" : "#EEEEEE"
    signal triggered()
    width: playButton ? 54 : transport ? 44 : 30
    height: width
    opacity: enabled ? 1 : 0.35
    scale: pointer.pressed ? (playButton ? 0.95 : 0.94) : pointer.containsMouse && enabled ? 1.06 : 1
    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.onPressAction: if (enabled) triggered()
    Behavior on scale {
        NumberAnimation {
            duration: button.reduceMotion ? 0 : pointer.pressed ? 100 : 220
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1]
        }
    }
    Rectangle {
        anchors.fill: parent
        radius: button.transport ? width / 2 : 0
        color: button.selected ? "#FFFA00" : "#2B2B2B"
        border.width: button.playButton || button.selected ? 0 : 1
        border.color: pointer.containsMouse ? "#888888" : "#444444"
        Behavior on color {
            ColorAnimation { duration: button.reduceMotion ? 0 : 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1] }
        }
        Behavior on border.color {
            ColorAnimation { duration: button.reduceMotion ? 0 : 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1] }
        }
        gradient: button.playButton ? playGradient : null
        Gradient {
            id: playGradient
            GradientStop { position: 0; color: "#FFFC6B" }
            GradientStop { position: 0.5; color: "#FFFA00" }
            GradientStop { position: 1; color: "#E9CF12" }
        }
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "white"
            opacity: button.playButton && pointer.containsMouse ? 0.06 : 0
        }
    }
    Item {
        width: 24; height: 24
        anchors.centerIn: parent
        scale: (button.playButton ? 22 : button.transport ? 20 : 15) / 24
        Shape {
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeWidth: -1
                fillColor: button.ink
                PathSvg {
                    path: button.kind === "play" ? "M7 4l13 8-13 8z"
                        : button.kind === "pause" ? "M6 4h4v16H6zM14 4h4v16h-4z"
                        : button.kind === "previous" ? "M6 4h3v16H6zM20 4L10 12l10 8z"
                        : button.kind === "next" ? "M15 4h3v16h-3zM4 4l10 8-10 8z"
                        : button.kind === "volume" || button.kind === "mute" ? "M3 9h4l6-5v16l-6-5H3z"
                        : "M14.4 18a2.6 2.6 0 1 0 5.2 0a2.6 2.6 0 1 0 -5.2 0"
                }
            }
            ShapePath {
                fillColor: "transparent"
                strokeColor: button.ink
                strokeWidth: button.kind === "volume" || button.kind === "mute" ? 2.4 : 2.6
                capStyle: ShapePath.FlatCap
                joinStyle: ShapePath.MiterJoin
                PathSvg {
                    path: button.kind === "volume" ? "M16 8.5a5 5 0 0 1 0 7M18.5 6a8.5 8.5 0 0 1 0 12"
                        : button.kind === "mute" ? "M16 9l5 6M21 9l-5 6"
                        : button.kind === "lyrics" ? "M4 6h12M4 11h12M4 16h7M19 9v9" : ""
                }
            }
        }
    }
    MouseArea {
        id: pointer
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: button.triggered()
    }
}
