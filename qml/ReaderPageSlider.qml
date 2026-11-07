import QtQuick

Item {
    id: slider
    property real value: 1
    property real minimum: 1
    property real maximum: 100
    property bool reduceMotion: false
    signal changed(real value, bool final)
    width: 366; height: 30; opacity: enabled ? 1 : 0.35
    readonly property real fraction: maximum <= minimum ? 0 : (value - minimum) / (maximum - minimum)

    Rectangle { x: 8; y: 14; width: slider.width - 16; height: 2; color: "#565656" }
    Rectangle { x: 8; y: 14; width: Math.max(0, (slider.width - 16) * slider.fraction); height: 2; color: "#FFFA00" }
    Rectangle {
        x: 8 + (slider.width - 16) * slider.fraction - 5; y: 10; width: 10; height: 10
        color: "#FFFA00"; border.color: "#161616"; border.width: 2
    }
    MouseArea {
        anchors.fill: parent; enabled: slider.enabled; cursorShape: Qt.PointingHandCursor
        function update(mouse, final) {
            const ratio = Math.max(0, Math.min(1, (mouse.x - 8) / Math.max(1, slider.width - 16)))
            slider.value = Math.round(slider.minimum + ratio * (slider.maximum - slider.minimum))
            slider.changed(slider.value, final)
        }
        onPressed: mouse => update(mouse, false)
        onPositionChanged: mouse => { if (pressed) update(mouse, false) }
        onReleased: mouse => update(mouse, true)
    }
}
