import QtQuick

Item {
    id: shell

    required property var displaySettings
    width: 1000
    height: 640

    property string selectedModule: "system"
    property real rightScrollOffset: 0
    readonly property real maximumRightScrollOffset: 196
    property string clockText: "00:00:00"
    property string dateText: ""
    property int animationPhase: 0
    property real animationTime: sourceWatchAnimation.openingDuration
    property real contentAnimationTime: sourceWatchAnimation.openingDuration
    readonly property real contentAdvanceDuration: 0.40
    property quaternion gyroStart: sourceWatchAnimation.neutralGyro
    property quaternion gyroEnd: sourceWatchAnimation.neutralGyro
    property real gyroProgress: 1
    readonly property quaternion gyroRotation: sourceWatchAnimation.interpolateRotation(
                                                   gyroStart, gyroEnd, gyroProgress)
    readonly property var animationFrame: sourceWatchAnimation.frame(animationPhase, animationTime)
    readonly property var baseContentAnimationFrame: animationPhase === 1
                                                       ? sourceWatchAnimation.frame(
                                                             animationPhase,
                                                             contentAnimationTime)
                                                       : animationFrame
    readonly property var contentAnimationFrame: animationPhase === 1
                                                   ? sourceWatchAnimation.frame(
                                                         animationPhase,
                                                         Math.min(sourceWatchAnimation.openingDuration,
                                                                  contentAnimationTime
                                                                  + contentAdvanceDuration))
                                                   : animationFrame
    readonly property var navigationAnimationFrame: animationPhase === 1
                                                      ? sourceWatchAnimation.frame(
                                                            animationPhase,
                                                            Math.min(sourceWatchAnimation.openingDuration,
                                                                     contentAnimationTime
                                                                     + contentAdvanceDuration))
                                                      : animationFrame
    readonly property quaternion sceneRotation: sourceWatchAnimation.combinedRotation(
                                                    animationPhase, animationTime,
                                                    gyroRotation)
    readonly property real sceneVectorLength: Math.sqrt(sceneRotation.x * sceneRotation.x
                                                        + sceneRotation.y * sceneRotation.y
                                                        + sceneRotation.z * sceneRotation.z)
    readonly property real sceneAxisX: sceneVectorLength > 0.000001
                                       ? sceneRotation.x / sceneVectorLength : 0
    readonly property real sceneAxisY: sceneVectorLength > 0.000001
                                       ? sceneRotation.y / sceneVectorLength : 0
    readonly property real sceneAxisZ: sceneVectorLength > 0.000001
                                       ? sceneRotation.z / sceneVectorLength : 1
    readonly property real sceneAngle: 2 * Math.atan2(sceneVectorLength,
                                                      sceneRotation.scalar) * 180 / Math.PI
    readonly property real backdropOpacity: frameValue("backdropOpacity", 1)
    readonly property bool transitioning: sourceClip.running || contentClip.running
    readonly property real contentOverlapDuration: 0.40
    readonly property real motionIntensity: 1.25
                                                * Math.max(0, Math.min(2, displaySettings.parallaxIntensity))
                                                * Math.max(0, Math.min(2, displaySettings.perspectiveIntensity))

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
        clockText = Qt.formatTime(now, displaySettings.clock24Hour ? "HH:mm:ss" : "hh:mm:ss AP")
        dateText = Qt.formatDate(now, "yyyy.MM.dd  ddd")
    }

    function scrollRightList(direction) {
        rightScrollOffset = Math.max(0, Math.min(maximumRightScrollOffset,
                                                  rightScrollOffset + direction * 90))
    }

    function frameValue(name, fallbackValue) {
        const value = animationFrame[name]
        return value === undefined ? fallbackValue : value
    }

    function listValue(name, index, fallbackValue) {
        const values = animationFrame[name]
        return values === undefined || index >= values.length ? fallbackValue : values[index]
    }

    function contentFrameValue(name, fallbackValue) {
        const value = contentAnimationFrame[name]
        return value === undefined ? fallbackValue : value
    }

    function baseContentFrameValue(name, fallbackValue) {
        const value = baseContentAnimationFrame[name]
        return value === undefined ? fallbackValue : value
    }

    function contentListValue(name, index, fallbackValue) {
        const values = contentAnimationFrame[name]
        return values === undefined || index >= values.length ? fallbackValue : values[index]
    }

    function navigationFrameValue(name, fallbackValue) {
        const value = navigationAnimationFrame[name]
        return value === undefined ? fallbackValue : value
    }

    function navigationListValue(name, index, fallbackValue) {
        const values = navigationAnimationFrame[name]
        return values === undefined || index >= values.length ? fallbackValue : values[index]
    }

    function animateEntrance() {
        sourceClip.stop()
        contentClip.stop()
        animationPhase = 1
        if (displaySettings.reduceMotion) {
            animationTime = sourceWatchAnimation.openingDuration
            contentAnimationTime = sourceWatchAnimation.openingDuration
            animationPhase = 0
            return
        }
        animationTime = 0
        contentAnimationTime = 0
        sourceClip.from = 0
        sourceClip.to = sourceWatchAnimation.openingDuration
        sourceClip.duration = Math.round(sourceWatchAnimation.openingDuration * 1000)
        sourceClip.restart()
        contentClip.restart()
    }

    function animateExit() {
        sourceClip.stop()
        contentClip.stop()
        animationPhase = 2
        if (displaySettings.reduceMotion) {
            animationTime = sourceWatchAnimation.closingDuration
            return
        }
        animationTime = 0
        sourceClip.from = 0
        sourceClip.to = sourceWatchAnimation.closingDuration
        sourceClip.duration = Math.round(sourceWatchAnimation.closingDuration * 1000)
        sourceClip.restart()
    }

    function retargetPointer(normalizedX, normalizedY) {
        gyroStart = gyroRotation
        gyroEnd = displaySettings.reduceMotion
                ? sourceWatchAnimation.neutralGyro
                : sourceWatchAnimation.gyroTarget(normalizedX, normalizedY, motionIntensity)
        gyroProgress = 0
        gyroMotion.restart()
    }

    NumberAnimation {
        id: gyroMotion
        target: shell
        property: "gyroProgress"
        from: 0
        to: 1
        duration: 500
        easing.type: Easing.OutQuad
    }

    NumberAnimation {
        id: sourceClip
        target: shell
        property: "animationTime"
        easing.type: Easing.Linear
    }

    SequentialAnimation {
        id: contentClip

        PauseAnimation {
            duration: Math.max(0, Math.round((sourceWatchAnimation.openingDuration
                                             - shell.contentOverlapDuration) * 1000))
        }

        NumberAnimation {
            target: shell
            property: "contentAnimationTime"
            from: 0
            to: sourceWatchAnimation.openingDuration
            duration: Math.round(sourceWatchAnimation.openingDuration * 1000)
            easing.type: Easing.Linear
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
        enabled: !shell.transitioning

        transform: [
            Rotation {
                origin.x: 500
                origin.y: 320
                axis.x: shell.sceneAxisX
                axis.y: shell.sceneAxisY
                axis.z: shell.sceneAxisZ
                angle: shell.sceneAngle
            }
        ]

    Image {
        id: outerRing
        x: 150
        y: -30
        width: 700
        height: 700
        source: "qrc:/assets/watch/ui_out_ring.png"
        fillMode: Image.PreserveAspectFit
        opacity: shell.frameValue("outerOpacity", 1)
        scale: shell.frameValue("outerScale", 1)
        rotation: shell.frameValue("outerRotation", 0)
        transformOrigin: Item.Center
        smooth: true
        mipmap: true
    }

    Image {
        id: secondaryRing
        x: 200
        y: 20
        width: 600
        height: 600
        source: "qrc:/assets/watch/ui_out1_ring.png"
        fillMode: Image.PreserveAspectFit
        opacity: shell.frameValue("secondaryOpacity", 1)
        scale: shell.frameValue("secondaryScale", 1)
        rotation: shell.frameValue("secondaryRotation", 0)
        transformOrigin: Item.Center
        smooth: true
        mipmap: true
    }

    Canvas {
        anchors.fill: parent
        antialiasing: true
        scale: shell.frameValue("meshScale", 1)
        rotation: shell.frameValue("meshRotation", 0)
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
        transform: [
            Scale {
                origin.x: 500
                origin.y: 320
                xScale: 0.82
                yScale: 0.82
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
            scale: shell.frameValue("mainScale", 1)
            transformOrigin: Item.Center
        }

        Image {
            id: mainRingImage
            x: 235
            y: 55
            width: 530
            height: 530
            source: "qrc:/assets/watch/ui_main_loop_bg.png"
            fillMode: Image.PreserveAspectFit
            opacity: shell.frameValue("mainOpacity", 1)
            scale: shell.frameValue("mainScale", 1)
            rotation: shell.frameValue("mainRotation", 0)
            transformOrigin: Item.Center
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
            scale: shell.frameValue("yellowScale", 1)
            transformOrigin: Item.Center
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
            opacity: shell.frameValue("blueOpacity", 1)
            scale: shell.frameValue("blueScale", 1)
            rotation: shell.frameValue("blueRotation", 0)
            transformOrigin: Item.Center
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
            scale: shell.frameValue("triangleScale", 1)
            rotation: shell.frameValue("triangleRotation", 0)
            transformOrigin: Item.Center
            smooth: true
            mipmap: true
        }

        Item {
            x: 270
            y: 2
            width: 600
            height: 74
            opacity: shell.contentFrameValue("nativeOpacity", 1)

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
            opacity: shell.baseContentFrameValue("nativeOpacity", 1)

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
            opacity: shell.baseContentFrameValue("nativeOpacity", 1)
        }

        DisplayPage {
            x: 303.6
            y: 116.9
            scale: 0.66576
            transformOrigin: Item.TopLeft
            visible: shell.selectedModule === "display"
            opacity: shell.baseContentFrameValue("nativeOpacity", 1)
            settings: shell.displaySettings
        }

        Repeater {
            model: shell.leftButtons

            delegate: HudNavButton {
                required property int index
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
                opacity: shell.navigationListValue("leftAlpha", index,
                                                   shell.navigationFrameValue("nativeOpacity", 1))
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
                    opacity: shell.navigationListValue("rightAlpha", index,
                                                       shell.navigationFrameValue("nativeOpacity", 1))
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
            opacity: (shell.rightScrollOffset > 0 ? 0.82 : 0.18)
                     * shell.contentFrameValue("nativeOpacity", 1)

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
            opacity: (shell.rightScrollOffset < shell.maximumRightScrollOffset ? 0.82 : 0.18)
                     * shell.contentFrameValue("nativeOpacity", 1)

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
            opacity: shell.contentFrameValue("storageOpacity", 1)
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
            opacity: shell.contentFrameValue("activityOpacity", 1)
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
            opacity: shell.contentFrameValue("nativeOpacity", 1)

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
            opacity: shell.contentFrameValue("nativeOpacity", 1)

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
            opacity: shell.contentFrameValue("identityOpacity", 1)

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
            opacity: shell.contentFrameValue("closeOpacity", 1)

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
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                cursorShape: Qt.PointingHandCursor
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
            opacity: shell.contentFrameValue("wordmarkOpacity", 1)
            smooth: true
            mipmap: true
        }
    }

    Text {
        x: 275
        y: 622
        width: 450
        height: 18
        text: "ESC TO CLOSE"
        color: "#747879"
        font.family: harmonyRegular.name
        font.pixelSize: 8
        font.letterSpacing: 0.5
        opacity: shell.contentFrameValue("nativeOpacity", 1)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    }

    Component.onCompleted: updateClock()
}
