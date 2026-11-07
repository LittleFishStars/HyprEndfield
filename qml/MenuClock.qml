import QtQuick
import "Appearance.js" as Appearance

// HUDClockPageViewport: only the 312 × 75 inner page slides; status stays put.
Item {
    id: clock
    required property string timeText
    required property string dateText
    property int seconds: 0
    property bool focusActive: false
    property int styleIndex: 0
    property bool recordingMode: false
    property bool reduceMotion: false
    property int previousStyle: 0
    property int lastStyle: 0
    property real slide: 1
    property int direction: 1
    property real wheelDistance: 0
    property bool wheelConsumed: false
    signal styleRequested(int index)
    width: 340; height: 145
    onStyleIndexChanged: {
        previousStyle = lastStyle; direction = (styleIndex - lastStyle + 5) % 5 === 4 ? -1 : 1; lastStyle = styleIndex;
        if (reduceMotion) slide = 1; else { slide = 0; transition.restart() }
    }
    NumberAnimation { id: transition; target: clock; property: "slide"; to: 1; duration: 260; easing.type: Easing.BezierSpline; easing.bezierCurve: [.42, 0, .58, 1, 1, 1] }
    Rectangle { x: .8; y: .8; width: 338.4; height: 120.4; radius: 17; color: "transparent"; border.width: 1.5; border.color: Appearance.accent("#FAD41F"); antialiasing: true }
    Rectangle { x: 7; y: 7; width: 326; height: 108; radius: 12; color: "#C70E0E0E"; border.width: 1; border.color: "#59B3B3B3"; antialiasing: true }
    Item {
        x: 14; y: 12; width: 312; height: 75; clip: true
        ClockInstrument { visible: clock.slide < 1; x: -14 - clock.slide * 312 * clock.direction; y: -12; timeText: clock.timeText; dateText: clock.dateText; styleIndex: clock.previousStyle }
        ClockInstrument { x: -14 + (1 - clock.slide) * 312 * clock.direction; y: -12; timeText: clock.timeText; dateText: clock.dateText; styleIndex: clock.styleIndex }
        WheelHandler { acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad; onWheel: event => {
            wheelEnd.restart(); if (clock.wheelConsumed) return;
            clock.wheelDistance += event.pixelDelta.x || event.pixelDelta.y || event.angleDelta.x || event.angleDelta.y;
            if (Math.abs(clock.wheelDistance) >= 28) { clock.wheelConsumed = true; clock.styleRequested((clock.styleIndex + (clock.wheelDistance > 0 ? 1 : 4)) % 5); }
        } }
    }
    Timer { id: wheelEnd; interval: 180; onTriggered: { clock.wheelDistance = 0; clock.wheelConsumed = false; } }
    Rectangle { x: 22; y: 88.5; width: 296; height: 1; color: "#29FFFFFF" }
    Rectangle { x: 22; y: 86; width: 14; height: 2.5; color: Appearance.accent("#FAD41F"); opacity: .8 }
    Text { objectName: "menuClockModeLabel"; x: 22; y: 94; text: clock.focusActive ? "FOCUS MODE" : "WORK MODE"; color: clock.focusActive ? Appearance.accent("#FAD41F") : "#737373"; font.family: harmonyMedium.name; font.pixelSize: 12; renderType: Text.CurveRendering }
    Text { id: record; x: 318 - width; y: 94; text: "RECORDING MODE"; color: clock.recordingMode ? "#E5383B" : "#737373"; font.family: harmonyMedium.name; font.pixelSize: 12; renderType: Text.CurveRendering }
    Rectangle { x: record.x - 13.5; y: 98; width: 7; height: 7; radius: 3.5; color: record.color; opacity: !clock.recordingMode || clock.reduceMotion || clock.seconds % 2 === 0 ? 1 : .28; Behavior on opacity { NumberAnimation { duration: clock.reduceMotion ? 0 : 350; easing.type: Easing.InOutQuad } } }
    Repeater {
        model: 5
        Item { required property int index; x: 14 + index * 64; y: 124; width: 56; height: 18
            Rectangle { y: 5.5; width: 49; height: 3; color: index === clock.styleIndex ? Appearance.accent("#FAD41F") : "#777777" }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: clock.styleRequested(index) }
        }
    }
}
