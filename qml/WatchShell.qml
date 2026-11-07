import QtQuick

Item {
    id: shell

    width: 1000
    height: 640

    property string selectedModule: ""
    property real rightScrollOffset: 0
    readonly property real maximumRightScrollOffset: 196
    property string clockText: "00:00:00"
    property string dateText: ""

    readonly property var leftButtons: [
        { key: "system", label: "系统", x: 54, y: 147, slant: 9, rotation: -0.859 },
        { key: "display", label: "显示", x: 16, y: 242, slant: 5, rotation: -0.458 },
        { key: "hotkeys", label: "快捷键", x: 22, y: 337, slant: 1, rotation: 0.229 },
        { key: "about", label: "关于", x: 60, y: 432, slant: -8, rotation: 0.917 }
    ]

    readonly property var rightButtons: [
        { key: "notes", label: "便笺", icon: "qrc:/assets/icons/notes.png" },
        { key: "fileShelf", label: "文件暂存架", icon: "qrc:/assets/icons/file-shelf.png" },
        { key: "clipboard", label: "剪贴板", icon: "qrc:/assets/icons/clipboard.png" },
        { key: "archive", label: "档案库", icon: "qrc:/assets/icons/archive.png" },
        { key: "nowPlaying", label: "当前播放", glyph: "nowPlaying" },
        { key: "volume", label: "音量", glyph: "volume" },
        { key: "reader", label: "阅读器", icon: "qrc:/assets/icons/reader.png" },
        { key: "workMode", label: "工作模式", icon: "qrc:/assets/icons/work-mode.png" },
        { key: "calendar", label: "日历", icon: "qrc:/assets/icons/calendar.png" },
        { key: "map", label: "地图", icon: "qrc:/assets/icons/map.png" },
        { key: "eventLog", label: "事件日志", icon: "qrc:/assets/icons/event-log.png" },
        { key: "addApp", label: "+ 添加应用", glyph: "addApp" }
    ]

    function rightButtonX(index) {
        const y = 174 + Math.floor(index / 2) * 90
        const centerY = y + 37
        const radius = 306
        const deltaY = centerY - 320
        return 508 + Math.sqrt(Math.max(0, radius * radius - deltaY * deltaY)) + (index % 2) * 84
    }

    function updateClock() {
        const now = new Date()
        clockText = Qt.formatTime(now, "HH:mm:ss")
        dateText = Qt.formatDate(now, "yyyy.MM.dd  ddd")
    }

    function scrollRightList(direction) {
        rightScrollOffset = Math.max(0, Math.min(maximumRightScrollOffset,
                                                  rightScrollOffset + direction * 90))
    }

    Timer {
        interval: 1000
        running: shell.visible
        repeat: true
        triggeredOnStart: true
        onTriggered: shell.updateClock()
    }

    Image {
        x: 150
        y: -30
        width: 700
        height: 700
        source: "qrc:/assets/watch/ui_out_ring.png"
        fillMode: Image.PreserveAspectFit
        opacity: 0.52
        smooth: true
        mipmap: true
    }

    Image {
        x: 200
        y: 20
        width: 600
        height: 600
        source: "qrc:/assets/watch/ui_out1_ring.png"
        fillMode: Image.PreserveAspectFit
        opacity: 0.20
        smooth: true
        mipmap: true
    }

    Canvas {
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const context = getContext("2d")
            context.reset()
            context.strokeStyle = "#78c9c9c9"
            context.lineWidth = 1
            context.beginPath()
            context.moveTo(44, 176)
            context.lineTo(92, 205)
            context.arc(500, 320, 416, Math.PI * 0.91, Math.PI * 1.10, false)
            context.lineTo(44, 464)
            context.lineTo(7, 419)
            context.lineTo(7, 225)
            context.closePath()
            context.stroke()
            context.beginPath()
            context.moveTo(956, 176)
            context.lineTo(908, 205)
            context.arc(500, 320, 416, Math.PI * 1.90, Math.PI * 2.09, false)
            context.lineTo(956, 464)
            context.lineTo(993, 419)
            context.lineTo(993, 225)
            context.closePath()
            context.stroke()
        }
    }

    Item {
        id: panelPlane
        anchors.fill: parent
        transform: Scale {
            origin.x: 500
            origin.y: 320
            xScale: 0.82
            yScale: 0.82
        }

        Rectangle {
            x: 250
            y: 70
            width: 500
            height: 500
            radius: 250
            color: "#151617"
            border.width: 1
            border.color: "#6bfad41f"
        }

        Image {
            x: 235
            y: 55
            width: 530
            height: 530
            source: "qrc:/assets/watch/ui_main_loop_bg.png"
            fillMode: Image.PreserveAspectFit
            opacity: 0.92
            smooth: true
            mipmap: true
        }

        Image {
            x: 251
            y: 71
            width: 498
            height: 499
            source: "qrc:/assets/watch/deco_04.png"
            fillMode: Image.PreserveAspectFit
            opacity: 0.20
            smooth: true
            mipmap: true
        }

        Image {
            x: 247
            y: 67
            width: 506
            height: 506
            source: "qrc:/assets/watch/ui_mid_ring.png"
            fillMode: Image.PreserveAspectFit
            opacity: 0.22
            smooth: true
            mipmap: true
        }

        Image {
            x: 428
            y: 55
            width: 143
            height: 123
            source: "qrc:/assets/watch/ui_triangle_fx.png"
            fillMode: Image.PreserveAspectFit
            opacity: 0.38
            smooth: true
            mipmap: true
        }

        Item {
            x: 270
            y: 2
            width: 600
            height: 74

            Text {
                x: 0
                y: 0
                width: 300
                height: 27
                text: "ENDFIELDHUD"
                color: "#f2f2f2"
                font.family: harmonyBold.name
                font.pixelSize: 20
                font.weight: Font.DemiBold
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                x: 0
                y: 31
                width: 300
                height: 18
                text: "SYSTEM INTERFACE"
                color: "#8f9496"
                font.family: harmonyMedium.name
                font.pixelSize: 9
                font.letterSpacing: 0.8
                verticalAlignment: Text.AlignVCenter
            }
        }

        Item {
            x: 652
            y: 0
            width: 205
            height: 78
            rotation: -1.4

            Rectangle {
                anchors.fill: parent
                color: "#e80b0c0d"
                border.width: 1.5
                border.color: "#d6fad41f"
                radius: 5
            }

            Rectangle {
                x: 8
                y: 8
                width: parent.width - 16
                height: 4
                color: "#fad41f"
            }

            Text {
                x: 11
                y: 14
                width: parent.width - 22
                height: 18
                text: shell.dateText
                color: "#d6fad41f"
                font.family: harmonyMedium.name
                font.pixelSize: 9
                font.weight: Font.Medium
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                x: 11
                y: 30
                width: parent.width - 22
                height: 34
                text: shell.clockText
                color: "#f5f5f5"
                font.family: harmonyBold.name
                font.pixelSize: 24
                font.weight: Font.Bold
                font.letterSpacing: 1.1
                verticalAlignment: Text.AlignVCenter
            }

            Rectangle {
                x: 11
                y: 67
                width: parent.width - 22
                height: 1
                color: "#71fad41f"
            }
        }

        Repeater {
            model: shell.leftButtons

            delegate: HudNavButton {
                required property var modelData
                x: modelData.x
                y: modelData.y
                width: 180
                height: 84
                label: modelData.label
                glyph: modelData.key
                slant: modelData.slant
                designX: modelData.x
                restRotation: modelData.rotation
                leftSide: true
                selected: shell.selectedModule === modelData.key
                onTriggered: shell.selectedModule = modelData.key
            }
        }

        Item {
            id: rightViewport
            x: 754
            y: 150
            width: 244
            height: 370
            clip: true

            Repeater {
                model: shell.rightButtons

                delegate: HudNavButton {
                    required property int index
                    required property var modelData
                    x: shell.rightButtonX(index) - rightViewport.x
                    y: 174 + Math.floor(index / 2) * 90 - shell.rightScrollOffset - rightViewport.y
                    width: 78
                    height: 74
                    label: modelData.label
                    glyph: modelData.glyph || ""
                    iconSource: modelData.icon || ""
                    slant: -2
                    designX: shell.rightButtonX(index)
                    selected: shell.selectedModule === modelData.key
                    onTriggered: shell.selectedModule = modelData.key
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                onWheel: wheel => {
                    shell.scrollRightList(wheel.angleDelta.y < 0 ? 1 : -1)
                    wheel.accepted = true
                }
            }
        }

        Item {
            x: 860
            y: 124
            width: 36
            height: 20
            opacity: shell.rightScrollOffset > 0 ? 0.82 : 0.18

            Text {
                anchors.centerIn: parent
                text: "⌃"
                color: "#e5e5e5"
                font.pixelSize: 22
            }

            MouseArea {
                anchors.fill: parent
                enabled: shell.rightScrollOffset > 0
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: shell.scrollRightList(-1)
            }
        }

        Item {
            x: 860
            y: 530
            width: 36
            height: 20
            opacity: shell.rightScrollOffset < shell.maximumRightScrollOffset ? 0.82 : 0.18

            Text {
                anchors.centerIn: parent
                text: "⌄"
                color: "#e5e5e5"
                font.pixelSize: 22
            }

            MouseArea {
                anchors.fill: parent
                enabled: shell.rightScrollOffset < shell.maximumRightScrollOffset
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: shell.scrollRightList(1)
            }
        }

        HudBottomButton {
            x: 350
            y: 463
            width: 144
            height: 65
            label: "存储"
            glyph: "storage"
            selected: shell.selectedModule === "storage"
            onTriggered: shell.selectedModule = "storage"
        }

        HudBottomButton {
            x: 506
            y: 463
            width: 144
            height: 65
            label: "活动监视器"
            glyph: "activity"
            mirrored: true
            selected: shell.selectedModule === "activity"
            onTriggered: shell.selectedModule = "activity"
        }

        Rectangle {
            x: 392
            y: 430
            width: 216
            height: 31
            radius: 15.5
            color: "#d91b1c1d"
            border.width: 1
            border.color: "#7d8f9292"

            Row {
                anchors.centerIn: parent
                spacing: 8

                Rectangle {
                    width: 11
                    height: 11
                    radius: 5.5
                    color: "transparent"
                    border.width: 2
                    border.color: "#f0f0f0"
                }

                Text {
                    text: "CHARGE MODE"
                    color: "#e8e8e8"
                    font.family: harmonyMedium.name
                    font.pixelSize: 8
                    font.letterSpacing: 0.7
                }
            }
        }

        Rectangle {
            x: 78
            y: 564
            width: 240
            height: 82
            radius: 12
            gradient: Gradient {
                GradientStop { position: 0; color: "#aa939393" }
                GradientStop { position: 0.63; color: "#c0404040" }
                GradientStop { position: 1; color: "#946d6d6d" }
            }
            border.width: 0.8
            border.color: "#61f5f5f5"

            Rectangle {
                x: 13
                y: 12
                width: 52
                height: 56
                radius: 6
                color: "#3b3c3d"
                border.width: 1
                border.color: "#8efad41f"

                Text {
                    anchors.centerIn: parent
                    text: "E"
                    color: "#f1f1f1"
                    font.family: harmonyBold.name
                    font.pixelSize: 31
                    font.weight: Font.Bold
                }
            }

            Text {
                x: 82
                y: 10
                width: 146
                height: 24
                text: "Endministrator"
                color: "#f7f7f7"
                font.family: harmonyMedium.name
                font.pixelSize: 17
                font.italic: true
                elide: Text.ElideRight
            }

            Text {
                x: 82
                y: 39
                width: 50
                height: 25
                text: "60"
                color: "#f7f7f7"
                font.family: harmonyBold.name
                font.pixelSize: 21
                font.weight: Font.DemiBold
            }

            Text {
                x: 140
                y: 41
                width: 72
                height: 14
                text: "权限等级  MAX"
                color: "#fad41f"
                font.family: harmonyMedium.name
                font.pixelSize: 8
                horizontalAlignment: Text.AlignRight
            }

            Rectangle {
                x: 84
                y: 65
                width: 112
                height: 2.5
                radius: 1.25
                color: "#fad41f"
            }

            Text {
                x: 84
                y: 69
                width: 120
                height: 11
                text: "UID: —"
                color: "#a8cecece"
                font.family: harmonyRegular.name
                font.pixelSize: 7
            }
        }

        Rectangle {
            x: 22
            y: 591
            width: 35
            height: 35
            radius: 4
            color: closePointer.containsMouse ? "#4de83333" : "#24e83333"
            border.width: 1
            border.color: "#b8e83333"

            HudGlyph {
                anchors.centerIn: parent
                width: 24
                height: 24
                kind: "power"
                ink: "#f7f7f7"
            }

            MouseArea {
                id: closePointer
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: appController.hideHud()
            }
        }

        Image {
            x: 425
            y: 544
            width: 150
            height: 29
            source: "qrc:/assets/watch/endfield_industries.png"
            sourceClipRect: Qt.rect(0, 142, 430, 83)
            fillMode: Image.Stretch
            smooth: true
            mipmap: true
        }
    }

    Text {
        x: 275
        y: 622
        width: 450
        height: 18
        text: "ESC / CLICK OUTSIDE TO CLOSE"
        color: "#747879"
        font.family: harmonyRegular.name
        font.pixelSize: 8
        font.letterSpacing: 0.5
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Component.onCompleted: updateClock()
}
