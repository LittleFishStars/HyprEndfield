import QtQuick

Item {
    id: root

    required property string label
    required property string glyph
    property bool mirrored: false
    property bool selected: false
    property bool hovered: pointer.containsMouse

    signal triggered()

    function configureMirror(context) {
        if (mirrored) {
            context.translate(width, 0)
            context.scale(-1, 1)
        }
    }

    function sectorPath(context, stripe) {
        const radius = 208
        const centerX = width + 6
        const centerY = -143
        const outerAngle = Math.acos(-centerX / radius)
        const innerAngle = Math.acos((width - centerX) / radius)
        const outerY = centerY + radius * Math.sin(outerAngle)
        const innerY = centerY + radius * Math.sin(innerAngle)
        context.beginPath()
        if (stripe) {
            const top = 44
            const angle = Math.PI - Math.asin((top - centerY) / radius)
            const endpointX = centerX + radius * Math.cos(angle)
            context.moveTo(endpointX, top)
            context.lineTo(width, top)
            context.lineTo(width, innerY)
            context.arc(centerX, centerY, radius, innerAngle, angle, false)
        } else {
            context.moveTo(0, outerY)
            context.lineTo(width * 0.62, 3)
            context.lineTo(width - 2, 8)
            context.quadraticCurveTo(width, 8, width, 10)
            context.lineTo(width, innerY)
            context.arc(centerX, centerY, radius, innerAngle, outerAngle, false)
        }
        context.closePath()
    }

    onSelectedChanged: face.requestPaint()
    onHoveredChanged: face.requestPaint()

    Canvas {
        id: face
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const context = getContext("2d")
            context.reset()
            context.save()
            root.configureMirror(context)
            root.sectorPath(context, false)
            context.fillStyle = root.hovered ? "#e8e0c429" : root.selected ? "#c9ddbd20" : "#9db79b1c"
            context.fill()
            context.strokeStyle = "#e6fad41f"
            context.lineWidth = 0.9
            context.stroke()
            root.sectorPath(context, true)
            context.fillStyle = root.hovered ? "#fff0dc47" : "#eefad41f"
            context.fill()
            context.restore()
        }
    }

    HudGlyph {
        width: root.glyph === "storage" ? 30 : 24
        height: width
        x: root.mirrored ? root.width - 60 - width / 2 : 60 - width / 2
        y: 23 - height / 2
        kind: root.glyph
        ink: "#f5f5f5"
    }

    Text {
        x: root.mirrored ? 5 : 73
        y: 8
        width: root.mirrored ? 64 : 65
        height: 30
        text: root.label
        color: "#f5f5f5"
        font.family: harmonyBold.name
        font.pixelSize: 12
        font.weight: Font.Bold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
        lineHeight: 0.84
    }

    MouseArea {
        id: pointer
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.triggered()
    }
}
