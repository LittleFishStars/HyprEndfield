import QtQuick

Item {
    id: root

    required property string title
    property string mode: "plain"
    property string value: ""
    property bool switched: false
    property bool dropdownOpened: false
    property string chipText: ""
    property real hoverShift: hoverHandler.hovered ? 3 : 0

    signal activated()
    signal toggleRequested(bool value)

    width: 590
    height: 50

    transform: Translate { x: root.hoverShift }

    Behavior on hoverShift {
        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
    }

    HoverHandler {
        id: hoverHandler
        cursorShape: root.mode === "plain" ? Qt.ArrowCursor : Qt.PointingHandCursor
    }

    Canvas {
        id: plate
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const context = getContext("2d")
            context.reset()
            context.beginPath()
            context.moveTo(0, 0)
            context.lineTo(width - 11, 0)
            context.lineTo(width, 11)
            context.lineTo(width, height)
            context.lineTo(0, height)
            context.closePath()

            const gradient = context.createLinearGradient(0, 0, 0, height)
            gradient.addColorStop(0, hoverHandler.hovered ? "#ECEDE8" : "#E4E5E0")
            gradient.addColorStop(0.55, hoverHandler.hovered ? "#DADBD6" : "#D3D4CF")
            gradient.addColorStop(1, hoverHandler.hovered ? "#CCCDC8" : "#C6C7C2")
            context.fillStyle = gradient
            context.fill()

            context.strokeStyle = "#B9FFFFFF"
            context.lineWidth = 1
            context.beginPath()
            context.moveTo(0, 0.5)
            context.lineTo(width - 11, 0.5)
            context.stroke()
        }
    }

    Rectangle {
        x: 0
        y: hoverHandler.hovered ? 5 : 9
        width: 3
        height: hoverHandler.hovered ? 40 : 32
        color: hoverHandler.hovered ? "#D7BC18" : "#2A2A2A"

        Behavior on y { NumberAnimation { duration: 220 } }
        Behavior on height { NumberAnimation { duration: 220 } }
        Behavior on color { ColorAnimation { duration: 220 } }
    }

    Item {
        x: 18
        y: 4
        width: 78
        height: 7

        Rectangle {
            width: 34
            height: 7
            color: "#161616"

            Text {
                anchors.fill: parent
                anchors.leftMargin: 4
                text: "ENDFIELD"
                color: "#E9E9E9"
                font.family: harmonyBold.name
                font.pixelSize: 6
                font.weight: Font.Bold
                font.letterSpacing: 0.08
                verticalAlignment: Text.AlignVCenter
            }
        }

        Item {
            x: 34
            width: 44
            height: 7
            clip: true

            Repeater {
                model: 15

                Rectangle {
                    required property int index
                    x: index * 3.5 - 7
                    y: -5
                    width: 1
                    height: 18
                    rotation: 45
                    color: "#575757"
                }
            }
        }
    }

    Text {
        x: 18
        y: 16
        width: 350
        height: 24
        text: root.title
        color: "#121212"
        font.family: harmonyMedium.name
        font.pixelSize: 16
        font.weight: Font.Medium
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    Item {
        id: dropdown
        visible: root.mode === "dropdown"
        x: 400
        y: 10
        width: 178
        height: 30

        Rectangle {
            anchors.fill: parent
            color: "#212121"
            border.width: 1
            border.color: "#3A3A3A"
        }

        Rectangle {
            x: 0
            y: root.dropdownOpened || dropdownPointer.containsMouse ? 0 : 15
            width: 2
            height: root.dropdownOpened || dropdownPointer.containsMouse ? 30 : 0
            color: "#D7BC18"

            Behavior on y { NumberAnimation { duration: 140 } }
            Behavior on height { NumberAnimation { duration: 140 } }
        }

        Text {
            x: 12
            y: 0
            width: 133
            height: 30
            text: root.value
            color: "#EEEEEE"
            font.family: harmonyMedium.name
            font.pixelSize: 13
            font.weight: Font.Medium
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Canvas {
            x: 155
            y: 12
            width: 9
            height: 6
            rotation: root.dropdownOpened ? 180 : dropdownPointer.containsMouse ? 8 : 0
            antialiasing: true

            Behavior on rotation {
                NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
            }

            onPaint: {
                const context = getContext("2d")
                context.reset()
                context.beginPath()
                context.moveTo(0, 0)
                context.lineTo(9, 0)
                context.lineTo(4.5, 6)
                context.closePath()
                context.fillStyle = "#D7BC18"
                context.fill()
            }
        }

        MouseArea {
            id: dropdownPointer
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.activated()
        }
    }

    Item {
        id: toggle
        visible: root.mode === "toggle"
        x: 490
        y: 12
        width: 88
        height: 26

        Rectangle {
            anchors.fill: parent
            color: "#202020"
            border.width: 1
            border.color: "#393939"
        }

        Text {
            x: 0
            y: 0
            width: 44
            height: 26
            text: "OFF"
            color: root.switched ? "#777777" : "#111111"
            font.family: harmonyBold.name
            font.pixelSize: 10
            font.weight: Font.Bold
            font.letterSpacing: 0.12
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            z: 2
        }

        Text {
            x: 44
            y: 0
            width: 44
            height: 26
            text: "ON"
            color: root.switched ? "#111111" : "#777777"
            font.family: harmonyBold.name
            font.pixelSize: 10
            font.weight: Font.Bold
            font.letterSpacing: 0.12
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            z: 2
        }

        Rectangle {
            x: root.switched ? 44 : 2
            y: 2
            width: 42
            height: 22
            color: root.switched ? "#FFFA00" : "#8E8F8A"
            z: 1

            Behavior on x {
                NumberAnimation { duration: 180; easing.type: Easing.OutBack }
            }

            Behavior on color { ColorAnimation { duration: 180 } }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.toggleRequested(!root.switched)
        }
    }

    Item {
        visible: root.mode === "nav"
        anchors.fill: parent

        Rectangle {
            id: countChip
            x: 548 - width
            y: 16
            width: chipLabel.implicitWidth + 14
            height: 18
            color: "#151515"

            Text {
                id: chipLabel
                anchors.centerIn: parent
                text: root.chipText
                color: "#EEEEEE"
                font.family: harmonyBold.name
                font.pixelSize: 10
                font.weight: Font.Bold
                font.letterSpacing: 0.1
            }
        }

        Text {
            x: 558
            y: 0
            width: 20
            height: 50
            text: "›"
            color: "#333333"
            font.family: harmonyBold.name
            font.pixelSize: 16
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.activated()
        }
    }

    Item {
        id: secondaryButton
        visible: root.mode === "button"
        x: 499
        y: 12
        width: 79
        height: 26

        Rectangle {
            anchors.fill: parent
            color: restorePointer.containsMouse ? "#484848" : "#383838"

            Behavior on color { ColorAnimation { duration: 220 } }
        }

        Rectangle {
            x: 8
            y: 7
            width: restorePointer.containsMouse ? 7 : 4
            height: 12
            color: "#D7BC18"

            Behavior on width { NumberAnimation { duration: 200 } }
        }

        Text {
            x: 21
            y: 0
            width: 15
            height: 26
            text: "↻"
            color: "#EEEEEE"
            font.family: harmonyBold.name
            font.pixelSize: 15
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Text {
            x: 41
            y: 0
            width: 31
            height: 26
            text: "恢复"
            color: "#EEEEEE"
            font.family: harmonyBold.name
            font.pixelSize: 11
            font.weight: Font.Bold
            font.letterSpacing: 0.08
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        MouseArea {
            id: restorePointer
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.activated()
        }
    }
}
