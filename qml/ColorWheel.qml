import QtQuick
import "Appearance.js" as Appearance

// NotesColorWheelControl: same 192px HSV raster, rim clamp and brightness.
Item {
    id: wheel
    width: 192; height: 224
    property color selectedColor: "#FFFA00"
    property real brightness: 1
    property point selection: Qt.point(.5, .5)
    signal chosen(string color)
    function syncColor() {
        const c = selectedColor
        brightness = c.hsvValue
        const angle = c.hsvHue * 2 * Math.PI
        selection = Qt.point((Math.cos(angle) * c.hsvSaturation + 1) / 2, (Math.sin(angle) * c.hsvSaturation + 1) / 2)
    }
    Component.onCompleted: syncColor()
    onSelectedColorChanged: syncColor()
    function pick(x, y) {
        const dx = x / 96 - 1, dy = y / 96 - 1, radius = Math.max(1, Math.hypot(dx, dy))
        selection = Qt.point((dx / radius + 1) / 2, (dy / radius + 1) / 2)
        const hue = (Math.atan2(dy, dx) / (2 * Math.PI) + 1) % 1
        const c = Qt.hsva(hue, Math.min(1, Math.hypot(dx, dy)), brightness, 1)
        chosen(c.toString())
    }
    Canvas {
        width: 192; height: 192
        onPaint: {
            const ctx = getContext("2d"), pixels = ctx.createImageData(192, 192)
            for (let y = 0; y < 192; y++) for (let x = 0; x < 192; x++) {
                const dx = (x + .5) / 96 - 1, dy = (y + .5) / 96 - 1, s = Math.hypot(dx, dy)
                if (s > 1) continue
                const c = Qt.hsva((Math.atan2(dy, dx) / (2 * Math.PI) + 1) % 1, s, 1, 1), i = (y * 192 + x) * 4
                pixels.data[i] = c.r * 255; pixels.data[i+1] = c.g * 255; pixels.data[i+2] = c.b * 255; pixels.data[i+3] = 255
            }
            ctx.putImageData(pixels, 0, 0)
        }
        MouseArea { anchors.fill: parent; onPressed: mouse => wheel.pick(mouse.x, mouse.y); onPositionChanged: mouse => { if (pressed) wheel.pick(mouse.x, mouse.y); } }
        Rectangle { x: wheel.selection.x * 192 - 7; y: wheel.selection.y * 192 - 7; width: 14; height: 14; radius: 7; border.width: 2; border.color: "#111111"; color: wheel.selectedColor }
    }
    Rectangle {
        y: 207; width: 192; height: 6
        gradient: Gradient { orientation: Gradient.Horizontal; GradientStop { position: 0; color: "black" } GradientStop { position: 1; color: "white" } }
        Rectangle { x: wheel.brightness * 188; y: -3; width: 4; height: 12; color: Appearance.accent("#FFFA00") }
        MouseArea { anchors.fill: parent; anchors.margins: -6; onPressed: mouse => { wheel.brightness = Math.max(0, Math.min(1, mouse.x / width)); wheel.pick(wheel.selection.x * 192, wheel.selection.y * 192); }; onPositionChanged: mouse => { if (pressed) { wheel.brightness = Math.max(0, Math.min(1, mouse.x / width)); wheel.pick(wheel.selection.x * 192, wheel.selection.y * 192); } } }
    }
}
