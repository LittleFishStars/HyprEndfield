import QtQuick
import "Appearance.js" as Appearance

// EndfieldHUD HUDArchiveTile / ShelfTile, including the original hover and
// selection motion. The surrounding grid owns selection and native transfer.
Item {
    id: tile
    required property var entry
    property int number: 1
    property bool selected: false
    property bool reduceMotion: false
    property bool presented: false
    property bool clearing: false
    property int animationIndex: 0
    property real entrance: 1
    property real removal: 0
    signal clicked(int modifiers, int clickCount)
    signal contextRequested(real localX, real localY)
    signal dragRequested()
    height: 124
    opacity: Math.min(1, entrance / 0.6) * (1 - removal)
    scale: 1 - 0.15 * removal
    transform: Translate { y: 16 * (1 - tile.entrance) }

    function enter() {
        entranceMotion.stop()
        if (!presented || reduceMotion) { entrance = 1; return }
        entrance = 0; entranceMotion.restart()
    }
    onPresentedChanged: if (presented) enter()
    Component.onCompleted: enter()
    onClearingChanged: {
        removalMotion.stop()
        if (clearing) removalMotion.restart()
        else removal = 0
    }
    SequentialAnimation {
        id: entranceMotion
        PauseAnimation { duration: tile.reduceMotion ? 0 : 60 + Math.min(14, tile.animationIndex) * 36 }
        NumberAnimation { target: tile; property: "entrance"; from: 0; to: 1; duration: tile.reduceMotion ? 0 : 380; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] }
    }
    SequentialAnimation {
        id: removalMotion
        PauseAnimation { duration: tile.reduceMotion ? 0 : tile.animationIndex * 30 }
        NumberAnimation { target: tile; property: "removal"; to: 1; duration: tile.reduceMotion ? 0 : 280; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] }
    }
    Rectangle {
        id: well
        width: parent.width; height: 100
        gradient: Gradient {
            GradientStop { position: 0; color: "#8A8885" }
            GradientStop { position: 0.55; color: "#BDBDBB" }
            GradientStop { position: 1; color: "#D9D9D8" }
        }
        transform: Translate {
            y: pointer.containsMouse ? -2 : 0
            Behavior on y { NumberAnimation { duration: tile.reduceMotion ? 0 : 250; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] } }
        }
        Rectangle {
            anchors.fill: parent; color: "#1FFFFFFF"; opacity: pointer.containsMouse ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: tile.reduceMotion ? 0 : 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1] } }
        }
        Rectangle { width: 34; height: 4; color: "#151515" }
        Text { x: 38; y: -1; text: "#" + String(tile.number).padStart(2, "0"); color: "#222222"; font.family: harmonyBold.name; font.pixelSize: 6; font.weight: Font.Bold; renderType: Text.CurveRendering }
        ShelfDocument { anchors.centerIn: parent; kind: tile.entry.kind; label: tile.entry.extension; hovered: pointer.containsMouse; reduceMotion: tile.reduceMotion }
        Rectangle {
            x: parent.width - width - 4; y: 84; width: sizeText.implicitWidth + 8; height: 12
            visible: tile.entry.sizeTag !== ""; color: "#BF141414"
            Text { id: sizeText; anchors.centerIn: parent; text: tile.entry.sizeTag; color: "#EEEEEE"; font.family: harmonyBold.name; font.pointSize: 8.5 * 0.75; font.weight: Font.Bold; renderType: Text.CurveRendering }
        }
    }
    Rectangle {
        y: 100; width: parent.width; height: 24; color: tile.selected ? "#151515" : "#C4C4C6"
        Text {
            x: 6; width: parent.width - 12; height: 24; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter
            text: tile.entry.name; elide: Text.ElideRight; color: tile.selected ? Appearance.accent("#FFFA00") : "#111111"
            visible: !Appearance.recording()
            font.family: harmonyMedium.name; font.pointSize: 11.5 * 0.75; font.weight: Font.Medium; renderType: Text.CurveRendering
        }
    }
    Item {
        property real inset: tile.selected ? -4 : -5
        x: inset; y: inset; width: tile.width - inset * 2; height: tile.height - inset * 2
        opacity: tile.selected ? 1 : 0
        Behavior on inset { NumberAnimation { duration: tile.reduceMotion ? 0 : 160; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] } }
        Behavior on opacity { NumberAnimation { duration: tile.reduceMotion ? 0 : 100; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1] } }
        Repeater {
            model: 2
            Item {
                required property int index; x: index === 0 ? 0 : parent.width - 9; width: 9; height: parent.height
                Rectangle { width: 9; height: 2; color: Appearance.accent("#FFFA00") }
                Rectangle { y: parent.height - 2; width: 9; height: 2; color: Appearance.accent("#FFFA00") }
                Rectangle { x: index === 0 ? 0 : 7; width: 2; height: parent.height; color: Appearance.accent("#FFFA00") }
            }
        }
    }
    Redaction { x: (tile.width - 84) / 2; y: 106; width: 84; height: 13; visible: Appearance.recording() }
    MouseArea {
        id: pointer; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor; preventStealing: true
        property point pressPoint
        property bool dragged: false
        onPressed: mouse => { pressPoint = Qt.point(mouse.x, mouse.y); dragged = false }
        onPositionChanged: mouse => {
            if (pressed && (pressedButtons & Qt.LeftButton) && !dragged && Math.hypot(mouse.x - pressPoint.x, mouse.y - pressPoint.y) >= 5) {
                dragged = true; tile.dragRequested()
            }
        }
        onClicked: mouse => {
            if (dragged) return
            if (mouse.button === Qt.RightButton) tile.contextRequested(mouse.x, mouse.y)
            else tile.clicked(mouse.modifiers, 1)
        }
        onDoubleClicked: mouse => { if (mouse.button === Qt.LeftButton && !dragged) tile.clicked(mouse.modifiers, 2) }
    }
}
