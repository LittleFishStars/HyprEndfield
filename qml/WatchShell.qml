import QtQuick

Item {
    id: shell

    width: 1000
    height: 640

    property string selectedModule: "system"
    property real rightScrollOffset: 0
    readonly property real maximumRightScrollOffset: 196
    property string clockText: "00:00:00"
    property string dateText: ""
    property real deploymentScale: 0.46
    property real deploymentPitch: 35
    property real deploymentOpacity: 0
    property real contentOpacity: 0
    property real outerRingScale: 0
    property real secondaryRingScale: 0.5
    property real interfaceScale: 0
    property real outerRingRotation: -8
    property real secondaryRingRotation: -10
    property real mainRingRotation: -35
    property real middleRingRotation: 80
    property real triangleRotation: -8
    property real pointerHorizontal: 0
    property real pointerVertical: 0
    readonly property bool transitioning: entranceAnimation.running || exitAnimation.running

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

    function animateEntrance() {
        exitAnimation.stop()
        deploymentScale = 0.46
        deploymentPitch = 35
        deploymentOpacity = 0
        contentOpacity = 0
        outerRingScale = 0
        secondaryRingScale = 0.5
        interfaceScale = 0
        outerRingRotation = -8
        secondaryRingRotation = -10
        mainRingRotation = -35
        middleRingRotation = 80
        triangleRotation = -8
        entranceAnimation.restart()
    }

    function animateExit() {
        entranceAnimation.stop()
        exitAnimation.restart()
    }

    Behavior on pointerHorizontal {
        NumberAnimation { duration: 100; easing.type: Easing.OutCubic }
    }

    Behavior on pointerVertical {
        NumberAnimation { duration: 100; easing.type: Easing.OutCubic }
    }

    HoverHandler {
        id: parallaxPointer

        onPointChanged: {
            shell.pointerHorizontal = Math.max(-1, Math.min(1, (point.position.x - 500) / 320))
            shell.pointerVertical = Math.max(-1, Math.min(1, (point.position.y - 320) / 240))
        }

        onHoveredChanged: {
            if (!hovered) {
                shell.pointerHorizontal = 0
                shell.pointerVertical = 0
            }
        }
    }

    ParallelAnimation {
        id: entranceAnimation

        NumberAnimation {
            target: shell
            property: "deploymentOpacity"
            to: 1
            duration: 30
            easing.type: Easing.Linear
        }

        NumberAnimation {
            target: shell
            property: "deploymentScale"
            to: 1
            duration: 750
            easing.type: Easing.OutCubic
        }

        SequentialAnimation {
            PauseAnimation { duration: 250 }
            NumberAnimation {
                target: shell
                property: "deploymentPitch"
                to: 34.9
                duration: 50
                easing.type: Easing.Linear
            }
            NumberAnimation {
                target: shell
                property: "deploymentPitch"
                to: 31.5
                duration: 100
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                target: shell
                property: "deploymentPitch"
                to: 17.1
                duration: 67
                easing.type: Easing.InOutCubic
            }
            NumberAnimation {
                target: shell
                property: "deploymentPitch"
                to: 4.7
                duration: 33
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: shell
                property: "deploymentPitch"
                to: -0.94
                duration: 150
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: shell
                property: "deploymentPitch"
                to: 0
                duration: 100
                easing.type: Easing.InOutCubic
            }
        }

        SequentialAnimation {
            PauseAnimation { duration: 283 }
            NumberAnimation {
                target: shell
                property: "outerRingScale"
                to: 1
                duration: 417
                easing.type: Easing.OutCubic
            }
            PauseAnimation { duration: 50 }
        }

        SequentialAnimation {
            PauseAnimation { duration: 200 }
            NumberAnimation {
                target: shell
                property: "secondaryRingScale"
                to: 1
                duration: 550
                easing.type: Easing.OutCubic
            }
        }

        NumberAnimation {
            target: shell
            property: "interfaceScale"
            to: 1
            duration: 667
            easing.type: Easing.OutCubic
        }

        NumberAnimation { target: shell; property: "outerRingRotation"; to: 0; duration: 750; easing.type: Easing.OutCubic }
        NumberAnimation { target: shell; property: "secondaryRingRotation"; to: 0; duration: 750; easing.type: Easing.OutCubic }
        NumberAnimation { target: shell; property: "mainRingRotation"; to: 0; duration: 750; easing.type: Easing.OutCubic }
        NumberAnimation { target: shell; property: "middleRingRotation"; to: 0; duration: 750; easing.type: Easing.OutCubic }
        NumberAnimation { target: shell; property: "triangleRotation"; to: 0; duration: 750; easing.type: Easing.OutCubic }

        SequentialAnimation {
            PauseAnimation { duration: 416 }
            NumberAnimation { target: shell; property: "contentOpacity"; to: 1; duration: 17 }
            NumberAnimation { target: shell; property: "contentOpacity"; to: 0; duration: 17 }
            NumberAnimation { target: shell; property: "contentOpacity"; to: 1; duration: 17 }
            PauseAnimation { duration: 83 }
            NumberAnimation { target: shell; property: "contentOpacity"; to: 0; duration: 17 }
            NumberAnimation { target: shell; property: "contentOpacity"; to: 1; duration: 17 }
            NumberAnimation { target: shell; property: "contentOpacity"; to: 0; duration: 17 }
            NumberAnimation { target: shell; property: "contentOpacity"; to: 1; duration: 17 }
            PauseAnimation { duration: 132 }
        }
    }

    ParallelAnimation {
        id: exitAnimation

        NumberAnimation {
            target: shell
            property: "deploymentScale"
            to: 0.62
            duration: 333
            easing.type: Easing.InCubic
        }

        SequentialAnimation {
            NumberAnimation { target: shell; property: "deploymentPitch"; to: -0.8; duration: 67 }
            NumberAnimation { target: shell; property: "deploymentPitch"; to: 0.3; duration: 66 }
            NumberAnimation { target: shell; property: "deploymentPitch"; to: 3.4; duration: 34 }
            NumberAnimation { target: shell; property: "deploymentPitch"; to: 7.3; duration: 33 }
            NumberAnimation { target: shell; property: "deploymentPitch"; to: 13.5; duration: 33 }
            NumberAnimation { target: shell; property: "deploymentPitch"; to: 23.9; duration: 34 }
            NumberAnimation { target: shell; property: "deploymentPitch"; to: 42.8; duration: 33 }
            NumberAnimation { target: shell; property: "deploymentPitch"; to: 50; duration: 33 }
        }

        SequentialAnimation {
            NumberAnimation {
                target: shell
                property: "outerRingScale"
                to: 1.1
                duration: 167
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: shell
                property: "outerRingScale"
                to: 0
                duration: 166
                easing.type: Easing.InCubic
            }
        }

        NumberAnimation { target: shell; property: "mainRingRotation"; to: -140; duration: 333; easing.type: Easing.InCubic }
        NumberAnimation { target: shell; property: "middleRingRotation"; to: 45; duration: 333; easing.type: Easing.InCubic }
        NumberAnimation { target: shell; property: "triangleRotation"; to: -15; duration: 333; easing.type: Easing.InCubic }

        SequentialAnimation {
            NumberAnimation {
                target: shell
                property: "secondaryRingScale"
                to: 1.1
                duration: 167
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: shell
                property: "secondaryRingScale"
                to: 0
                duration: 166
                easing.type: Easing.InCubic
            }
        }

        SequentialAnimation {
            NumberAnimation {
                target: shell
                property: "interfaceScale"
                to: 1.1
                duration: 167
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: shell
                property: "interfaceScale"
                to: 0
                duration: 166
                easing.type: Easing.InCubic
            }
        }

        SequentialAnimation {
            NumberAnimation { target: shell; property: "contentOpacity"; to: 0; duration: 33 }
            NumberAnimation { target: shell; property: "contentOpacity"; to: 1; duration: 33 }
            NumberAnimation { target: shell; property: "contentOpacity"; to: 0; duration: 34 }
            PauseAnimation { duration: 33 }
            NumberAnimation { target: shell; property: "contentOpacity"; to: 1; duration: 33 }
            NumberAnimation { target: shell; property: "contentOpacity"; to: 0; duration: 34 }
            PauseAnimation { duration: 133 }
        }

        SequentialAnimation {
            PauseAnimation { duration: 320 }
            NumberAnimation {
                target: shell
                property: "deploymentOpacity"
                to: 0
                duration: 13
                easing.type: Easing.Linear
            }
        }
    }

    Timer {
        interval: 1000
        running: shell.visible
        repeat: true
        triggeredOnStart: true
        onTriggered: shell.updateClock()
    }

    MouseArea {
        anchors.fill: parent
    }

    Item {
        id: scene
        anchors.fill: parent
        transformOrigin: Item.Center
        scale: shell.deploymentScale
        rotation: -1
        opacity: shell.deploymentOpacity
        enabled: !shell.transitioning

        transform: [
            Rotation {
                origin.x: 500
                origin.y: 320
                axis.x: 1
                axis.y: 0
                axis.z: 0
                angle: 5 + shell.deploymentPitch + shell.pointerVertical * 6
            },
            Rotation {
                origin.x: 500
                origin.y: 320
                axis.x: 0
                axis.y: 1
                axis.z: 0
                angle: -4 - shell.pointerHorizontal * 7
            },
            Translate {
                x: shell.pointerHorizontal * 4
                y: shell.pointerVertical * 3
            }
        ]

    Image {
        id: outerRing
        x: 150 - shell.pointerHorizontal * 10
        y: -30 - shell.pointerVertical * 7
        width: 700
        height: 700
        source: "qrc:/assets/watch/ui_out_ring.png"
        fillMode: Image.PreserveAspectFit
        opacity: 0.52
        scale: shell.outerRingScale
        rotation: shell.outerRingRotation
        transformOrigin: Item.Center
        smooth: true
        mipmap: true
    }

    Image {
        id: secondaryRing
        x: 200 - shell.pointerHorizontal * 6
        y: 20 - shell.pointerVertical * 4
        width: 600
        height: 600
        source: "qrc:/assets/watch/ui_out1_ring.png"
        fillMode: Image.PreserveAspectFit
        opacity: 0.20
        scale: shell.secondaryRingScale
        rotation: shell.secondaryRingRotation
        transformOrigin: Item.Center
        smooth: true
        mipmap: true
    }

    Canvas {
        anchors.fill: parent
        antialiasing: true
        scale: shell.outerRingScale
        transformOrigin: Item.Center

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
        opacity: shell.contentOpacity
        transform: [
            Scale {
                origin.x: 500
                origin.y: 320
                xScale: 0.82 * shell.interfaceScale
                yScale: 0.82 * shell.interfaceScale
            },
            Translate {
                x: shell.pointerHorizontal * 12
                y: shell.pointerVertical * 9
            }
        ]

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
            id: mainRingImage
            x: 235
            y: 55
            width: 530
            height: 530
            source: "qrc:/assets/watch/ui_main_loop_bg.png"
            fillMode: Image.PreserveAspectFit
            opacity: 0.92
            rotation: shell.mainRingRotation
            smooth: true
            mipmap: true
        }

        Image {
            id: middleDecoration
            x: 251
            y: 71
            width: 498
            height: 499
            source: "qrc:/assets/watch/deco_04.png"
            fillMode: Image.PreserveAspectFit
            opacity: 0.20
            rotation: shell.middleRingRotation
            smooth: true
            mipmap: true
        }

        Image {
            id: middleRingImage
            x: 247
            y: 67
            width: 506
            height: 506
            source: "qrc:/assets/watch/ui_mid_ring.png"
            fillMode: Image.PreserveAspectFit
            opacity: 0.22
            rotation: shell.middleRingRotation
            smooth: true
            mipmap: true
        }

        Image {
            id: triangleDecoration
            x: 428
            y: 55
            width: 143
            height: 123
            source: "qrc:/assets/watch/ui_triangle_fx.png"
            fillMode: Image.PreserveAspectFit
            opacity: 0.38
            rotation: shell.triangleRotation
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

        SystemPage {
            x: 303.6
            y: 116.9
            scale: 0.66576
            transformOrigin: Item.TopLeft
            visible: shell.selectedModule === "system"
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
            visible: shell.selectedModule === ""

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
            x: 493
            y: 426
            width: 14
            height: 14
            radius: 7
            color: "#EFEFEF"
            visible: shell.selectedModule !== ""

            Text {
                anchors.centerIn: parent
                text: "ϟ"
                color: "#202020"
                font.family: harmonyBold.name
                font.pixelSize: 10
                font.weight: Font.Bold
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

    }

    Component.onCompleted: updateClock()
}
