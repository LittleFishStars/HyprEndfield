import QtQuick

Item {
    id: root

    property string kind: ""
    property color ink: "#202020"

    onKindChanged: glyph.requestPaint()
    onInkChanged: glyph.requestPaint()

    function roundedRect(context, x, y, width, height, radius) {
        context.beginPath()
        context.moveTo(x + radius, y)
        context.lineTo(x + width - radius, y)
        context.quadraticCurveTo(x + width, y, x + width, y + radius)
        context.lineTo(x + width, y + height - radius)
        context.quadraticCurveTo(x + width, y + height, x + width - radius, y + height)
        context.lineTo(x + radius, y + height)
        context.quadraticCurveTo(x, y + height, x, y + height - radius)
        context.lineTo(x, y + radius)
        context.quadraticCurveTo(x, y, x + radius, y)
        context.closePath()
    }

    function circle(context, x, y, radius) {
        context.beginPath()
        context.arc(x, y, radius, 0, Math.PI * 2, false)
        context.closePath()
    }

    Canvas {
        id: glyph
        anchors.fill: parent
        antialiasing: true

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            const context = getContext("2d")
            context.reset()
            context.save()
            context.scale(width / 32, height / 32)
            context.fillStyle = root.ink
            context.strokeStyle = root.ink
            context.lineJoin = "miter"

            if (root.kind === "system") {
                context.fillRect(7, 7, 18, 18)
                context.clearRect(12, 12, 8, 8)
                for (const position of [9, 19]) {
                    context.fillRect(position, 1, 4, 5)
                    context.fillRect(position, 26, 4, 5)
                    context.fillRect(1, position, 5, 4)
                    context.fillRect(26, position, 5, 4)
                }
            } else if (root.kind === "display") {
                root.roundedRect(context, 2, 4, 28, 20, 2)
                context.fill()
                context.clearRect(6, 8, 20, 12)
                context.fillRect(13, 25, 6, 2)
                context.fillRect(8, 28, 16, 3)
            } else if (root.kind === "hotkeys") {
                root.roundedRect(context, 1, 6, 30, 21, 2)
                context.fill()
                for (const position of [5, 11, 17, 23])
                    context.clearRect(position, 10, 4, 4)
                context.clearRect(6, 19, 20, 3)
            } else if (root.kind === "about") {
                root.circle(context, 16, 16, 14)
                context.fill()
                context.clearRect(14, 7, 4, 4)
                context.clearRect(14, 14, 4, 11)
            } else if (root.kind === "nowPlaying") {
                root.circle(context, 16, 16, 14)
                context.fill()
                context.globalCompositeOperation = "destination-out"
                root.circle(context, 16, 16, 11)
                context.fill()
                context.globalCompositeOperation = "source-over"
                context.beginPath()
                context.moveTo(12, 9)
                context.lineTo(24, 16)
                context.lineTo(12, 23)
                context.closePath()
                context.fill()
            } else if (root.kind === "volume") {
                context.beginPath()
                context.moveTo(2, 12)
                context.lineTo(8, 12)
                context.lineTo(17, 5)
                context.lineTo(17, 27)
                context.lineTo(8, 20)
                context.lineTo(2, 20)
                context.closePath()
                context.fill()
                context.lineWidth = 3
                context.beginPath()
                context.arc(17, 16, 7, -Math.PI / 4, Math.PI / 4, false)
                context.stroke()
                context.beginPath()
                context.arc(17, 16, 12, -Math.PI / 4, Math.PI / 4, false)
                context.stroke()
            } else if (root.kind === "addApp") {
                root.roundedRect(context, 3, 3, 26, 26, 3)
                context.fill()
                context.clearRect(7, 7, 18, 18)
                context.fillRect(14, 10, 4, 12)
                context.fillRect(10, 14, 12, 4)
            } else if (root.kind === "storage") {
                context.fillRect(12, 2, 8, 8)
                context.fillRect(14.5, 11, 3, 4)
                context.fillRect(5, 15, 22, 3)
                for (const position of [5, 15, 25])
                    context.fillRect(position, 19, 2, 3)
                for (const position of [2, 12, 22])
                    context.fillRect(position, 23, 8, 7)
            } else if (root.kind === "activity") {
                context.fillRect(3, 23, 5, 7)
                context.fillRect(13, 18, 5, 12)
                context.fillRect(23, 12, 5, 18)
                context.lineWidth = 2.8
                context.beginPath()
                context.moveTo(3, 17)
                context.lineTo(12, 8)
                context.lineTo(19, 13)
                context.lineTo(28, 2)
                context.moveTo(20, 2)
                context.lineTo(28, 2)
                context.lineTo(28, 10)
                context.stroke()
            } else if (root.kind === "power") {
                context.lineWidth = 4.2
                context.lineCap = "round"
                context.beginPath()
                context.arc(16, 17, 11, -Math.PI * 0.28, Math.PI * 1.28, false)
                context.stroke()
                context.lineCap = "butt"
                context.fillRect(14, 1, 4, 15)
            }

            context.restore()
        }
    }
}
