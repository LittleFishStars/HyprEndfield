import QtQuick

Item {
    id: root

    required property string label
    property string glyph: ""
    property url iconSource: ""
    property real slant: -2
    property real designX: 0
    property real restRotation: 0
    property bool leftSide: false
    property bool selected: false
    property bool hovered: pointer.containsMouse
    property real connectorLength: Math.max(width + 30, 300 - designX)

    signal triggered()

    rotation: restRotation
    scale: selected ? 1.045 : hovered ? 1.025 : 1

    Behavior on scale {
        NumberAnimation { duration: 170; easing.type: Easing.OutCubic }
    }

    function platePoints(inset) {
        const margin = inset + 1.5
        return [
            { x: margin + Math.max(slant, 0), y: margin },
            { x: width - margin + Math.min(slant, 0), y: margin },
            { x: width - margin - Math.max(slant, 0), y: height - margin },
            { x: margin - Math.min(slant, 0), y: height - margin }
        ]
    }

    function platePath(context, inset) {
        const points = platePoints(inset)
        const radius = height > 65 ? 4 : 3
        context.beginPath()
        for (let index = 0; index < points.length; ++index) {
            const vertex = points[index]
            const previous = points[(index + points.length - 1) % points.length]
            const next = points[(index + 1) % points.length]
            const previousLength = Math.max(1, Math.hypot(previous.x - vertex.x, previous.y - vertex.y))
            const nextLength = Math.max(1, Math.hypot(next.x - vertex.x, next.y - vertex.y))
            const entryX = vertex.x + (previous.x - vertex.x) * radius / previousLength
            const entryY = vertex.y + (previous.y - vertex.y) * radius / previousLength
            const exitX = vertex.x + (next.x - vertex.x) * radius / nextLength
            const exitY = vertex.y + (next.y - vertex.y) * radius / nextLength
            if (index === 0)
                context.moveTo(entryX, entryY)
            else
                context.lineTo(entryX, entryY)
            context.quadraticCurveTo(vertex.x, vertex.y, exitX, exitY)
        }
        context.closePath()
    }

    onSelectedChanged: {
        face.requestPaint()
        side.requestPaint()
        marker.requestPaint()
    }
    onHoveredChanged: {
        face.requestPaint()
        side.requestPaint()
        marker.requestPaint()
    }
    onWidthChanged: {
        face.requestPaint()
        side.requestPaint()
        connectors.requestPaint()
    }
    onHeightChanged: {
        face.requestPaint()
        side.requestPaint()
        connectors.requestPaint()
    }

    Canvas {
        id: connectors
        visible: root.leftSide
        x: 0
        y: 0
        width: root.connectorLength
        height: root.height
        antialiasing: true

        onPaint: {
            const context = getContext("2d")
            context.reset()
            context.strokeStyle = "#6bc4c4c4"
            context.lineWidth = 0.65
            const endX = root.connectorLength
            const bend = (320 - (root.y + root.height / 2)) * 0.055
            for (let index = 0; index < 5; ++index) {
                const y = root.height / 2 + (index - 2) * 3
                context.beginPath()
                context.moveTo(root.width - 12, y)
                context.lineTo(root.width + 9, y)
                context.lineTo(root.width + 23, y + bend)
                context.lineTo(endX, y + bend)
                context.stroke()
            }
        }
    }

    Canvas {
        id: side
        x: root.leftSide ? 8 : -8
        y: 10
        width: root.width
        height: root.height
        antialiasing: true

        onPaint: {
            const context = getContext("2d")
            context.reset()
            root.platePath(context, 0)
            context.fillStyle = root.selected ? "#31eeeeee" : root.hovered ? "#25eeeeee" : "#21d1d1d1"
            context.fill()
            context.strokeStyle = root.selected ? "#47f0f0f0" : "#3fe0e0e0"
            context.lineWidth = 0.6
            context.stroke()
        }
    }

    Canvas {
        id: face
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const context = getContext("2d")
            context.reset()
            root.platePath(context, 0)
            context.fillStyle = root.hovered ? "#ffffff" : root.selected ? "#f2eeeeee" : "#dce7e7e7"
            context.fill()
            context.strokeStyle = "#a3404040"
            context.lineWidth = 0.65
            context.stroke()

            root.platePath(context, -3)
            context.strokeStyle = root.selected || root.hovered ? "#c7ffffff" : "#61ffffff"
            context.lineWidth = 0.5
            context.stroke()
        }
    }

    Canvas {
        id: marker
        x: root.width / 2 - 25
        y: root.leftSide ? 4 : -3
        width: 50
        height: 50
        visible: root.selected || root.hovered
        opacity: root.selected ? 1 : 0.55
        antialiasing: true

        onPaint: {
            const context = getContext("2d")
            context.reset()
            context.strokeStyle = "#fad41f"
            context.lineWidth = 2.5
            context.beginPath()
            context.arc(25, 25, 21, -Math.PI * 0.90, -Math.PI * 0.35, false)
            context.stroke()
            context.beginPath()
            context.arc(25, 25, 21, Math.PI * 0.08, Math.PI * 0.60, false)
            context.stroke()
        }
    }

    Item {
        id: iconBox
        width: root.leftSide ? 34 : 23
        height: width
        x: (root.width - width) / 2
        y: (root.leftSide ? 29 : 22) - height / 2

        HudGlyph {
            anchors.fill: parent
            kind: root.glyph
            ink: "#202020"
            visible: root.glyph.length > 0
        }

        Image {
            anchors.fill: parent
            anchors.margins: 1
            source: root.iconSource
            visible: root.iconSource.toString().length > 0
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
        }
    }

    Text {
        x: 7
        y: root.leftSide ? 52 : 42
        width: root.width - 14
        height: root.leftSide ? 24 : 25
        text: root.label
        color: "#202020"
        font.family: harmonyBold.name
        font.pixelSize: root.leftSide ? 14 : 10
        font.weight: Font.Bold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
        lineHeight: 0.88
    }

    MouseArea {
        id: pointer
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.triggered()
    }
}
