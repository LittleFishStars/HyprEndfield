import QtQuick
import "Appearance.js" as Appearance
import QtQuick.Shapes

Item {
    id: root

    required property string title
    property string mode: "plain"
    property string value: ""
    property url artwork: ""
    property string sublabel: ""
    property real titleInset: 18
    property bool toggleEnabled: true
    property string buttonTitle: qsTr("恢复")
    property bool switched: false
    property bool dropdownOpened: false
    property string chipText: ""
    property real sliderValue: 0
    property real sliderMinimum: 0
    property real sliderMaximum: 100
    property real sliderStep: 1
    property real sliderZero: Number.NaN
    property string sliderFormat: "percent"
    property Item sliderBubbleHost: null
    property var segmentOptions: [qsTr("24 小时制"), qsTr("12 小时制")]
    property int segmentIndex: 0
    property var swatchColors: ["#FAD41F", "#6EDFE8", "#A8E58B", "#C9A2FF", "#FF8F78", "custom"]
    property int swatchIndex: 0
    property real hoverShift: hoverHandler.hovered ? 3 : 0

    signal activated
    signal toggleRequested(bool value)
    signal sliderMoved(real value, bool final)
    signal sliderBubbleRequested(bool shown, string text, real x, real y)
    signal segmentRequested(int index)
    signal swatchRequested(int index)

    width: 590
    height: 50

    transform: Translate {
        x: root.hoverShift
    }

    Behavior on hoverShift {
        NumberAnimation {
            duration: 220
            easing.type: Easing.OutCubic
        }
    }

    HoverHandler {
        id: hoverHandler
        cursorShape: root.mode === "plain" ? Qt.ArrowCursor : Qt.PointingHandCursor
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1
            fillGradient: LinearGradient {
                x1: 0
                y1: 0
                x2: 0
                y2: root.height
                GradientStop {
                    position: 0
                    color: hoverHandler.hovered ? "#ECEDE8" : "#E4E5E0"
                }
                GradientStop {
                    position: .55
                    color: hoverHandler.hovered ? "#DADBD6" : "#D3D4CF"
                }
                GradientStop {
                    position: 1
                    color: hoverHandler.hovered ? "#CCCDC8" : "#C6C7C2"
                }
            }
            PathSvg {
                path: "M0 0H" + (root.width - 11) + "L" + root.width + " 11V" + root.height + "H0Z"
            }
        }
        ShapePath {
            strokeWidth: 1
            strokeColor: "#B9FFFFFF"
            fillColor: "transparent"
            PathSvg {
                path: "M0 .5H" + (root.width - 11)
            }
        }
    }

    Rectangle {
        x: 0
        y: hoverHandler.hovered ? 5 : 9
        width: 3
        height: hoverHandler.hovered ? 40 : 32
        color: hoverHandler.hovered ? Appearance.accent("#D7BC18") : "#2A2A2A"

        Behavior on y {
            NumberAnimation {
                duration: 220
            }
        }
        Behavior on height {
            NumberAnimation {
                duration: 220
            }
        }
        Behavior on color {
            ColorAnimation {
                duration: 220
            }
        }
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
                renderType: Text.CurveRendering
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
        renderType: Text.CurveRendering
        x: root.titleInset
        y: root.sublabel ? 11 : 16
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

    Text {
        visible: root.sublabel.length > 0
        x: root.titleInset; y: 33; width: 390; height: 14
        text: root.sublabel; color: "#6C6D68"; elide: Text.ElideRight
        font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering
    }
    Text {
        visible: root.mode === "value"
        x: 280; y: 12; width: root.width - 292; height: 26
        text: "[ " + root.value + " ]"; color: "#151515"; horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
        font.family: harmonyBold.name; font.pixelSize: 12; renderType: Text.CurveRendering
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
            color: Appearance.accent("#D7BC18")

            Behavior on y {
                NumberAnimation {
                    duration: 140
                }
            }
            Behavior on height {
                NumberAnimation {
                    duration: 140
                }
            }
        }

        Text {
            renderType: Text.CurveRendering
            x: 12
            y: 0
            width: 133
            height: 30
            text: root.value
            visible: root.artwork.toString() === ""
            color: "#EEEEEE"
            font.family: harmonyMedium.name
            font.pixelSize: 13
            font.weight: Font.Medium
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Image { x: 12; y: 7; width: 130; height: 16; visible: root.artwork.toString() !== ""; source: root.artwork; fillMode: Image.PreserveAspectFit; smooth: true; mipmap: true }
        Shape {
            x: 155
            y: 12
            width: 9
            height: 6
            rotation: root.dropdownOpened ? 180 : dropdownPointer.containsMouse ? 8 : 0
            preferredRendererType: Shape.CurveRenderer
            Behavior on rotation {
                NumberAnimation {
                    duration: 160
                    easing.type: Easing.OutCubic
                }
            }
            ShapePath {
                strokeWidth: -1
                fillColor: Appearance.accent("#D7BC18")
                PathSvg {
                    path: "M0 0H9L4.5 6Z"
                }
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
        enabled: root.toggleEnabled
        opacity: enabled ? 1 : 0.35
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
            renderType: Text.CurveRendering
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
            renderType: Text.CurveRendering
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
            color: root.switched ? Appearance.accent("#FFFA00") : "#8E8F8A"
            z: 1

            Behavior on x {
                NumberAnimation {
                    duration: 180
                    easing.type: Easing.OutBack
                }
            }

            Behavior on color {
                ColorAnimation {
                    duration: 180
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.toggleRequested(!root.switched)
        }
    }

    HudSlider {
        visible: root.mode === "slider"
        x: 202
        y: 10
        width: 366
        value: root.sliderValue
        minimum: root.sliderMinimum
        maximum: root.sliderMaximum
        stepSize: root.sliderStep
        zeroValue: root.sliderZero
        valueFormat: root.sliderFormat
        bubbleHost: root.sliderBubbleHost
        onValueModified: (value, final) => root.sliderMoved(value, final)
        onBubbleRequested: (shown, text, x, y) => root.sliderBubbleRequested(shown, text, x, y)
    }

    Item {
        visible: root.mode === "segmented"
        x: 400
        y: 10
        width: 178
        height: 30

        Repeater {
            model: root.segmentOptions

            delegate: Rectangle {
                required property int index
                required property string modelData
                readonly property real segmentWidth: (178 - 4 * Math.max(0, root.segmentOptions.length - 1)) / Math.max(1, root.segmentOptions.length)
                x: index * (segmentWidth + 4)
                width: segmentWidth
                height: 30
                color: index === root.segmentIndex ? "#D3D4CF" : segmentPointer.containsMouse ? "#2B2B2B" : "#212121"
                border.width: index === root.segmentIndex ? 0 : 1
                border.color: segmentPointer.containsMouse ? "#777777" : "#3A3A3A"

                Behavior on color {
                    ColorAnimation {
                        duration: 220
                    }
                }

                Rectangle {
                    visible: index === root.segmentIndex
                    x: 0
                    y: 0
                    width: 3
                    height: 30
                    color: "#111111"
                }

                Text {
                    renderType: Text.CurveRendering
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    text: modelData
                    color: index === root.segmentIndex ? "#111111" : "#CCCCCC"
                    font.family: harmonyMedium.name
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                MouseArea {
                    id: segmentPointer
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.segmentRequested(index)
                }
            }
        }
    }

    Row {
        visible: root.mode === "swatches"
        x: 384
        y: 13
        spacing: 8

        Repeater {
            model: root.swatchColors

            delegate: Item {
                required property int index
                required property string modelData
                width: 24
                height: 24
                scale: swatchPointer.containsMouse ? 1.08 : 1

                Behavior on scale {
                    NumberAnimation {
                        duration: 220
                        easing.type: Easing.OutCubic
                    }
                }

                Rectangle {
                    visible: index === root.swatchIndex
                    x: -5
                    y: -5
                    width: 34
                    height: 34
                    color: "transparent"
                    border.width: 3
                    border.color: Appearance.accent("#FFFA00")

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        color: "transparent"
                        border.width: 2
                        border.color: "#111111"
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    visible: modelData !== "custom"
                    radius: modelData === "custom" ? 12 : 0
                    color: modelData
                    border.width: 1
                    border.color: "#111111"
                }

                Item {
                    visible: modelData === "custom"
                    anchors.fill: parent
                    Repeater {
                        model: 60
                        Shape {
                            id: swatchSegment
                            required property int index
                            anchors.fill: parent
                            preferredRendererType: Shape.CurveRenderer
                            ShapePath {
                                strokeWidth: -1
                                fillColor: ["#FF5A5A", "#FFD84A", "#7BE37B", "#4AC6FF", "#A77BFF"][Math.floor(index / 12)]
                                PathSvg {
                                    path: {
                                        const r = swatchSegment.width / 2;
                                        const a = index * Math.PI * 2 / 60 - Math.PI / 2;
                                        const b = (index + 1.2) * Math.PI * 2 / 60 - Math.PI / 2;
                                        return "M" + r + " " + r + "L" + (r + r * Math.cos(a)) + " " + (r + r * Math.sin(a)) + "A" + r + " " + r + " 0 0 1 " + (r + r * Math.cos(b)) + " " + (r + r * Math.sin(b)) + "Z";
                                    }
                                }
                            }
                        }
                    }
                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        color: "transparent"
                        border.width: 1
                        border.color: "#111111"
                        antialiasing: true
                    }
                }

                MouseArea {
                    id: swatchPointer
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.swatchRequested(index)
                }
            }
        }
    }

    Item {
        visible: root.mode === "nav"
        anchors.fill: parent

        Rectangle {
            id: countChip
            visible: root.chipText.length > 0
            x: 548 - width
            y: 16
            width: chipLabel.implicitWidth + 14
            height: 18
            color: "#151515"

            Text {
                id: chipLabel
                renderType: Text.CurveRendering
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
            renderType: Text.CurveRendering
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

            Behavior on color {
                ColorAnimation {
                    duration: 220
                }
            }
        }

        Rectangle {
            x: 8
            y: 7
            width: restorePointer.containsMouse ? 7 : 4
            height: 12
            color: Appearance.accent("#D7BC18")

            Behavior on width {
                NumberAnimation {
                    duration: 200
                }
            }
        }

        Text {
            renderType: Text.CurveRendering
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
            renderType: Text.CurveRendering
            x: 41
            y: 0
            width: 31
            height: 26
            text: root.buttonTitle
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: 7
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
