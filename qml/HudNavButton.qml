import QtQuick
import QtQuick.Shapes

Item {
    id: root

    required property string label
    property string glyph: ""
    property url iconSource: ""
    property real designX: 0
    property real restRotation: 0
    property bool leftSide: false
    property bool selected: false
    property bool hovered: pointer.containsMouse
    property int shapeIndex: 0
    property var contourLines: []

    readonly property real sourceScale: 1000 / 1920
    readonly property real sourceYOffset: (640 - 1080 * sourceScale) / 2
    readonly property real cardScale: width / 440
    readonly property var cardShapes: [
        { top: [22.19, -0.03542], bottom: [168.99, -0.03362], left: [173.3, -0.54768, 0.00065769], right: [399.48, -0.61494, 0.00036623], plate: { top: [38.65, -0.0352], left: [196.74, -0.56595, 0.00063797] }, shadow: [14, 20.3] },
        { top: [17.81, -0.03285], bottom: [170.39, -0.03319], left: [156.26, -0.40245, 0.0014242], right: [362.1, -0.28868, 0.00063431], plate: { top: [31.58, -0.03529], left: [176.6, -0.41875, 0.0012589] }, shadow: [15.9, 16.2] },
        { top: [16.02, -0.03221], bottom: [175.66, -0.0329], left: [125.98, -0.15145, 0.0014506], right: [330.5, 0.01888, 0.0010545], plate: { top: [26.28, -0.0346], left: [143.06, -0.1786, 0.0013852] }, shadow: [16.5, 12] },
        { top: [34.97, -0.02944], bottom: [153.99, -0.03242], left: [99.86, 0.28886, 0.0003757], right: [320.36, 0.33845, 0.00088469], plate: { top: [41.71, -0.03279], left: [113.23, 0.2459, 0.0005216] }, shadow: [15, 8.6] }
    ]
    readonly property var sourceAngles: [28.02, 10.52, -6.22, -21.56]

    signal triggered()

    function shapePath(shape, padding, cornerRadius) {
        const lineY = function (line, x) { return line[0] + line[1] * x }
        const sideX = function (side, y) { return side[0] + side[1] * y + side[2] * y * y }
        const slope = function (side, y) { return side[1] + 2 * side[2] * y }
        const middleY = (lineY(shape.top, 250) + lineY(shape.bottom, 250)) / 2
        const top = [shape.top[0] - padding * Math.hypot(1, shape.top[1]), shape.top[1]]
        const bottom = [shape.bottom[0] + padding * Math.hypot(1, shape.bottom[1]), shape.bottom[1]]
        const left = [shape.left[0] - padding * Math.hypot(1, slope(shape.left, middleY)), shape.left[1], shape.left[2]]
        const right = [shape.right[0] + padding * Math.hypot(1, slope(shape.right, middleY)), shape.right[1], shape.right[2]]

        function corner(edge, side) {
            let y = lineY(edge, 250)
            for (let index = 0; index < 40; ++index)
                y = lineY(edge, sideX(side, y))
            return [sideX(side, y), y]
        }
        function along(point, edge, direction) {
            const length = Math.hypot(1, edge[1])
            return [point[0] + direction * cornerRadius / length,
                    point[1] + direction * cornerRadius * edge[1] / length]
        }
        function down(point, side, direction) {
            const y = point[1] + direction * cornerRadius / Math.hypot(1, slope(side, point[1]))
            return [sideX(side, y), y]
        }
        function control(side, startY, endY) {
            return [sideX(side, startY) + slope(side, startY) * (endY - startY) / 2,
                    (startY + endY) / 2]
        }
        function point(value) { return value[0].toFixed(1) + " " + value[1].toFixed(1) }

        const topLeft = corner(top, left)
        const topRight = corner(top, right)
        const bottomRight = corner(bottom, right)
        const bottomLeft = corner(bottom, left)
        const topStart = along(topLeft, top, 1)
        const topEnd = along(topRight, top, -1)
        const rightStart = down(topRight, right, 1)
        const rightEnd = down(bottomRight, right, -1)
        const bottomStart = along(bottomRight, bottom, -1)
        const bottomEnd = along(bottomLeft, bottom, 1)
        const leftStart = down(bottomLeft, left, -1)
        const leftEnd = down(topLeft, left, 1)

        return "M" + point(topStart) + " L" + point(topEnd)
                + " Q" + point(topRight) + " " + point(rightStart)
                + " Q" + point(control(right, rightStart[1], rightEnd[1])) + " " + point(rightEnd)
                + " Q" + point(bottomRight) + " " + point(bottomStart)
                + " L" + point(bottomEnd)
                + " Q" + point(bottomLeft) + " " + point(leftStart)
                + " Q" + point(control(left, leftStart[1], leftEnd[1])) + " " + point(leftEnd)
                + " Q" + point(topLeft) + " " + point(topStart) + " Z"
    }

    function cardPath(padding) {
        return shapePath(cardShapes[shapeIndex], padding, 12)
    }

    function platePath() {
        const shape = cardShapes[shapeIndex]
        const plate = {
            top: shape.plate.top,
            left: shape.plate.left,
            right: [shape.right[0] + 30, shape.right[1], shape.right[2]],
            bottom: [shape.bottom[0] + 30, shape.bottom[1]]
        }
        return shapePath(plate, 0, 12)
                + " " + shapePath(plate, -2.35, 9.65)
                + " " + shapePath(plate, -5.25, 6.75)
    }

    function cardCenterX() {
        const shape = cardShapes[shapeIndex]
        const y = (shape.top[0] + shape.top[1] * 250
                   + shape.bottom[0] + shape.bottom[1] * 250) / 2
        const left = shape.left[0] + shape.left[1] * y + shape.left[2] * y * y
        const right = shape.right[0] + shape.right[1] * y + shape.right[2] * y * y
        return (left + right) / 2
    }

    function loadContours() {
        if (!leftSide)
            return
        const request = new XMLHttpRequest()
        request.open("GET", "qrc:/assets/menu/card-contours.json")
        request.onreadystatechange = function () {
            if (request.readyState !== 4)
                return
            if (request.status === 0 || request.status === 200) {
                const allContours = JSON.parse(request.responseText)
                root.contourLines = allContours[root.shapeIndex] || []
            }
        }
        request.send()
    }

    Component.onCompleted: loadContours()
    onShapeIndexChanged: loadContours()
    onContourLinesChanged: contours.requestPaint()

    Canvas {
        id: connector
        visible: root.leftSide
        x: -root.x
        y: -root.y
        width: 1000
        height: 640
        antialiasing: true

        onPaint: {
            const context = getContext("2d")
            context.reset()
            const sourceCenterX = 960
            const sourceCenterY = 540
            const bezelRadius = 492 * 0.96
            const cardRadius = bezelRadius + 126
            const angle = root.sourceAngles[root.shapeIndex] * Math.PI / 180
            const unitX = Math.cos(angle)
            const unitY = Math.sin(angle)
            const tangentX = -Math.sin(angle)
            const tangentY = Math.cos(angle)
            const outerRadius = cardRadius - 20
            const topRadius = bezelRadius + 80
            const footRadius = bezelRadius + 34
            const endRadius = bezelRadius + 10
            const dx = 3
            const dy = 13
            const mapX = function (value) { return value * root.sourceScale }
            const mapY = function (value) { return value * root.sourceScale + root.sourceYOffset }
            const pointX = function (radius, offset) {
                return sourceCenterX - radius * unitX + offset * tangentX
            }
            const pointY = function (radius, offset) {
                return sourceCenterY - radius * unitY + offset * tangentY
            }
            const gradient = context.createLinearGradient(
                        mapX(pointX(outerRadius, 0)), mapY(pointY(outerRadius, 0)),
                        mapX(pointX(endRadius, 0) + dx), mapY(pointY(endRadius, 0) + dy))
            gradient.addColorStop(0, "rgba(255,255,255,0.06)")
            gradient.addColorStop(0.22, "rgba(255,255,255,0.5)")
            gradient.addColorStop(0.75, "rgba(255,255,255,0.5)")
            gradient.addColorStop(1, "rgba(255,255,255,0.04)")
            context.strokeStyle = gradient
            context.lineWidth = 2.7 * root.sourceScale
            context.lineJoin = "miter"
            context.lineCap = "round"
            for (const offset of [-6.9, -2.3, 2.2, 6.8]) {
                context.beginPath()
                context.moveTo(mapX(pointX(outerRadius, offset)), mapY(pointY(outerRadius, offset)))
                context.lineTo(mapX(pointX(topRadius, offset)), mapY(pointY(topRadius, offset)))
                context.lineTo(mapX(pointX(footRadius, offset) + dx), mapY(pointY(footRadius, offset) + dy))
                context.lineTo(mapX(pointX(endRadius, offset) + dx), mapY(pointY(endRadius, offset) + dy))
                context.stroke()
            }
        }
    }

    Item {
        id: body
        anchors.fill: parent
        rotation: root.restRotation
        scale: root.selected ? 1.015 : root.hovered ? 1.012 : 1
        x: root.hovered ? (root.leftSide ? -1 : 1.1) : 0
        y: root.hovered ? -1.6 : 0

        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        Behavior on x { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        Behavior on y { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

        Item {
            id: leftCard
            visible: root.leftSide
            width: 440
            height: 172
            scale: root.cardScale
            transformOrigin: Item.TopLeft

            Shape {
                x: root.cardShapes[root.shapeIndex].shadow[0]
                y: root.cardShapes[root.shapeIndex].shadow[1]
                width: 440
                height: 172
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: "#55fffffa"
                    strokeWidth: 2.5
                    fillColor: "#45fffffa"
                    PathSvg { path: root.cardPath(0) }
                }
                ShapePath {
                    strokeColor: "#55fffffa"
                    strokeWidth: 2.5
                    fillColor: "transparent"
                    PathSvg { path: root.cardPath(4) }
                }
            }

            Shape {
                width: 440
                height: 172
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: "transparent"
                    fillColor: root.hovered ? "#f3f3f1" : root.selected ? "#e9e8e4" : "#d8d7d2"
                    PathSvg { path: root.cardPath(0) }
                }
                ShapePath {
                    fillRule: ShapePath.OddEvenFill
                    strokeColor: "transparent"
                    fillColor: "#50fffdf7"
                    PathSvg { path: root.platePath() }
                }
            }

            Canvas {
                id: contours
                width: 440
                height: 172
                antialiasing: true

                onPaint: {
                    const context = getContext("2d")
                    context.reset()
                    context.lineCap = "round"
                    context.lineJoin = "round"
                    for (const line of root.contourLines) {
                        if (line.length < 6)
                            continue
                        context.strokeStyle = "rgba(35,35,31," + line[1] + ")"
                        context.lineWidth = line[0]
                        context.beginPath()
                        context.moveTo(line[2], line[3])
                        for (let index = 4; index < line.length; index += 2)
                            context.lineTo(line[index], line[index + 1])
                        context.stroke()
                    }
                }
            }

            Shape {
                width: 440
                height: 172
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: "#b8f2f2ec"
                    strokeWidth: 2
                    fillColor: "transparent"
                    PathSvg { path: root.cardPath(4) }
                }
            }

            HudGlyph {
                x: root.cardCenterX() - 23
                y: 43
                width: 46
                height: 46
                kind: root.glyph
                ink: "#313131"
                visible: root.glyph.length > 0
            }

            Text {
                x: root.cardCenterX() - 90
                y: 112
                width: 180
                height: 44
                text: root.label
                color: "#313131"
                font.family: harmonyBold.name
                font.pixelSize: 24
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Item {
            id: rightTile
            visible: !root.leftSide
            width: 107
            height: 104
            scale: root.width / 107
            transformOrigin: Item.TopLeft

            Rectangle {
                x: -22
                y: 7
                width: 117
                height: 114
                radius: 13
                color: "transparent"
                border.width: 2.5
                border.color: "#52fffffa"
            }
            Rectangle {
                x: -17
                y: 12
                width: 107
                height: 104
                radius: 8
                color: "#63fffffa"
            }
            Rectangle {
                x: -5
                y: -5
                width: 117
                height: 114
                radius: 13
                color: "transparent"
                border.width: 2.5
                border.color: "#9eedf1f5"
            }
            Rectangle {
                width: 107
                height: 104
                radius: 8
                color: root.hovered ? "#f6f6f4" : root.selected ? "#f1f0ed" : "#efeeea"
            }

            Canvas {
                width: 107
                height: 104
                antialiasing: true

                onPaint: {
                    const context = getContext("2d")
                    context.reset()
                    context.strokeStyle = "rgba(0,0,0,0.026)"
                    context.lineWidth = 1
                    for (let y = 1; y < 104; y += 3.3) {
                        context.beginPath()
                        context.moveTo(7, y)
                        context.lineTo(100, y)
                        context.stroke()
                    }
                }
            }

            Item {
                width: 47
                height: 47
                x: 30
                y: 12

                HudGlyph {
                    anchors.fill: parent
                    kind: root.glyph
                    ink: "#313131"
                    visible: root.glyph.length > 0
                }
                Image {
                    anchors.fill: parent
                    source: root.iconSource
                    visible: root.iconSource.toString().length > 0
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                    sourceSize.width: 94
                    sourceSize.height: 94
                }
            }

            Text {
                x: 5
                y: 68
                width: 97
                height: 30
                text: root.label
                color: "#313131"
                font.family: harmonyMedium.name
                font.pixelSize: 19
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                minimumPixelSize: 13
                fontSizeMode: Text.Fit
            }
        }
    }

    MouseArea {
        id: pointer
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.triggered()
    }
}
