import QtQuick

Item {
    id: root

    required property var settings
    property string openMenu: ""
    property var menuItems: []
    property real menuX: 0
    property real menuY: 0
    property bool bubbleShown: false
    property string bubbleText: ""
    property real bubbleX: 0
    property real bubbleY: 0
    property bool scrollBarShown: false

    signal pageRequested(string page)

    readonly property var rows: [
        { key: "uiScale", title: "界面缩放", mode: "slider", minimum: 0.2, maximum: 2, step: 0.01, format: "scale" },
        { key: "positionX", title: "界面位置 X", mode: "slider", minimum: -50, maximum: 50, step: 1, format: "signedPercent", zero: 0 },
        { key: "positionY", title: "界面位置 Y", mode: "slider", minimum: -50, maximum: 50, step: 1, format: "signedPercent", zero: 0 },
        { key: "parallax", title: "视差强度", mode: "slider", minimum: 0, maximum: 200, step: 1, format: "percent" },
        { key: "perspective", title: "透视强度", mode: "slider", minimum: 0, maximum: 200, step: 1, format: "percent" },
        { key: "brightness", title: "背景亮度", mode: "slider", minimum: 0, maximum: 100, step: 1, format: "percent" },
        { key: "blur", title: "模糊程度", mode: "slider", minimum: 0, maximum: 100, step: 1, format: "percent" },
        { key: "theme", title: "主题", mode: "dropdown" },
        { key: "clockFormat", title: "时间格式", mode: "segmented" },
        { key: "clockStyle", title: "时钟样式", mode: "dropdown" },
        { key: "centerLogo", title: "中心标志", mode: "dropdown" },
        { key: "appIcon", title: "应用 / 菜单栏图标", mode: "nav" },
        { key: "palette", title: "主题颜色", mode: "swatches" },
        { key: "battery", title: "电池提醒", mode: "nav", chip: "电池" },
        { key: "ambient", title: "持续环境动画", mode: "toggle" },
        { key: "reduceMotion", title: "减少动态效果", mode: "toggle" },
        { key: "lowPower", title: "低功耗视觉模式", mode: "toggle" },
        { key: "recording", title: "录制模式", mode: "toggle" },
        { key: "bootLog", title: "每日启动日志", mode: "toggle" },
        { key: "facility", title: "设施警报", mode: "toggle" }
    ]

    readonly property var themes: ["深色", "浅色", "跟随系统"]
    readonly property var clockStyles: ["01  数字", "02  分栏", "03  表盘", "04  轨道", "05  堆叠"]
    readonly property var centerLogos: ["Endfield", "Rhodes Island", "Babel", "Rhine Lab", "自定"]

    width: 590
    height: 510

    function sliderValue(key) {
        switch (key) {
        case "uiScale": return settings.uiScale
        case "positionX": return settings.hudOffsetX * 100
        case "positionY": return settings.hudOffsetY * 100
        case "parallax": return settings.parallaxIntensity * 100
        case "perspective": return settings.perspectiveIntensity * 100
        case "brightness": return settings.backgroundBrightness * 100
        case "blur": return settings.blurAmount * 100
        }
        return 0
    }

    function writeSlider(key, value) {
        switch (key) {
        case "uiScale": settings.uiScale = value; break
        case "positionX": settings.hudOffsetX = value / 100; break
        case "positionY": settings.hudOffsetY = value / 100; break
        case "parallax": settings.parallaxIntensity = value / 100; break
        case "perspective": settings.perspectiveIntensity = value / 100; break
        case "brightness": settings.backgroundBrightness = value / 100; break
        case "blur": settings.blurAmount = value / 100; break
        }
    }

    function toggleValue(key) {
        switch (key) {
        case "ambient": return settings.ambientAnimation
        case "reduceMotion": return settings.reduceMotion
        case "lowPower": return settings.lowPowerVisualMode
        case "recording": return settings.recordingMode
        case "bootLog": return settings.dailyBootLog
        case "facility": return settings.facilityAlerts
        }
        return false
    }

    function writeToggle(key, value) {
        switch (key) {
        case "ambient": settings.ambientAnimation = value; break
        case "reduceMotion": settings.reduceMotion = value; break
        case "lowPower": settings.lowPowerVisualMode = value; break
        case "recording": settings.recordingMode = value; break
        case "bootLog": settings.dailyBootLog = value; break
        case "facility": settings.facilityAlerts = value; break
        }
    }

    function dropdownValue(key) {
        if (key === "theme")
            return themes[settings.themeIndex]
        if (key === "clockStyle")
            return clockStyles[settings.clockStyleIndex]
        if (key === "centerLogo")
            return centerLogos[settings.centerLogoIndex]
        return ""
    }

    function optionsFor(key) {
        if (key === "theme")
            return themes
        if (key === "clockStyle")
            return clockStyles
        if (key === "centerLogo")
            return centerLogos
        return []
    }

    function selectedOption(key) {
        if (key === "theme")
            return settings.themeIndex
        if (key === "clockStyle")
            return settings.clockStyleIndex
        if (key === "centerLogo")
            return settings.centerLogoIndex
        return 0
    }

    function pickOption(index) {
        if (openMenu === "theme")
            settings.themeIndex = index
        else if (openMenu === "clockStyle")
            settings.clockStyleIndex = index
        else if (openMenu === "centerLogo")
            settings.centerLogoIndex = index
        openMenu = ""
    }

    function activate(key, point) {
        if (key === "appIcon" || key === "battery") {
            pageRequested(key)
            return
        }
        const options = optionsFor(key)
        if (!options.length)
            return
        if (openMenu === key) {
            openMenu = ""
            return
        }
        menuItems = options
        menuX = Math.min(width - 178, point.x)
        const menuHeight = options.length * 30 + 8
        const below = point.y + 34
        menuY = below + menuHeight > height + 20 ? point.y - 4 - menuHeight : below
        openMenu = key
    }

    function showBubble(shown, text, x, y) {
        bubbleShown = shown
        bubbleText = text
        bubbleX = x
        bubbleY = y
    }

    Text {
        id: displayTitle
        x: 0
        y: 0
        height: 14
        text: "// HUD  /  "
        color: "#8A8A8A"
        font.family: harmonyRegular.name
        font.pixelSize: 11
        font.letterSpacing: 0.06
        verticalAlignment: Text.AlignVCenter
    }

    Text {
        x: 69
        y: 0
        height: 14
        text: "DISPLAY"
        color: "#E6E6E6"
        font.family: harmonyRegular.name
        font.pixelSize: 11
        font.letterSpacing: 0.06
        verticalAlignment: Text.AlignVCenter
    }

    Text {
        x: 500
        y: 0
        width: 49
        height: 14
        text: "SYS-02"
        color: "#8A8A8A"
        font.family: harmonyRegular.name
        font.pixelSize: 11
        font.letterSpacing: 0.06
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
    }

    Item {
        x: 556
        y: 3.5
        width: 34
        height: 7
        clip: true

        Repeater {
            model: 12
            Rectangle {
                required property int index
                x: index * 3.5 - 7
                y: -5
                width: 1
                height: 18
                rotation: 45
                color: "#777777"
            }
        }
    }

    Text {
        x: 0
        y: 18
        width: 140
        height: 34
        text: "显示"
        color: "#F4F4F4"
        font.family: harmonyBold.name
        font.pixelSize: 26
        font.weight: Font.Bold
        font.letterSpacing: 0.08
        verticalAlignment: Text.AlignVCenter
    }

    Text {
        x: displayTitle.implicitWidth + 12
        y: 21
        width: 100
        height: 29
        text: "DISPLAY"
        color: "#8A8A8A"
        font.family: harmonyMedium.name
        font.pixelSize: 13
        font.weight: Font.Medium
        font.letterSpacing: 0.2
        verticalAlignment: Text.AlignVCenter
    }

    Rectangle { x: 0; y: 64; width: 590; height: 1; color: "#38FFFFFF" }
    Rectangle { x: 0; y: 63; width: 56; height: 3; color: "#FFFA00" }

    Repeater {
        model: 11
        Rectangle {
            required property int index
            x: 454 + index * 9
            y: 60
            width: 1
            height: 9
            color: "#59FFFFFF"
        }
    }

    Rectangle {
        x: 563
        y: 58
        width: 27
        height: 12
        color: "#151515"
        Text {
            anchors.fill: parent
            text: "20"
            color: "#FFFA00"
            font.family: harmonyBold.name
            font.pixelSize: 11
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    Flickable {
        id: rowsViewport
        x: 0
        y: 81
        width: 590
        height: 416
        clip: true
        contentWidth: 590
        contentHeight: root.rows.length * 50 + (root.rows.length - 1) * 6
        boundsBehavior: Flickable.StopAtBounds
        flickDeceleration: 3200

        onContentYChanged: {
            root.scrollBarShown = true
            scrollHideTimer.restart()
            root.openMenu = ""
            root.bubbleShown = false
        }

        Repeater {
            model: root.rows

            delegate: HudSettingsRow {
                required property int index
                required property var modelData
                x: 0
                y: index * 56
                width: 580
                title: modelData.title
                mode: modelData.mode
                value: root.dropdownValue(modelData.key)
                switched: root.toggleValue(modelData.key)
                dropdownOpened: root.openMenu === modelData.key
                chipText: modelData.chip || ""
                sliderValue: root.sliderValue(modelData.key)
                sliderMinimum: modelData.minimum === undefined ? 0 : modelData.minimum
                sliderMaximum: modelData.maximum === undefined ? 100 : modelData.maximum
                sliderStep: modelData.step === undefined ? 1 : modelData.step
                sliderZero: modelData.zero === undefined ? Number.NaN : modelData.zero
                sliderFormat: modelData.format || "percent"
                sliderBubbleHost: bubbleLayer
                segmentIndex: root.settings.clock24Hour ? 0 : 1
                swatchIndex: root.settings.accentIndex

                onActivated: root.activate(modelData.key, mapToItem(root, 400, 10))
                onToggleRequested: value => root.writeToggle(modelData.key, value)
                onSliderMoved: (value, final) => root.writeSlider(modelData.key, value)
                onSliderBubbleRequested: (shown, text, x, y) => root.showBubble(shown, text, x, y)
                onSegmentRequested: index => root.settings.clock24Hour = index === 0
                onSwatchRequested: index => root.settings.accentIndex = index
            }
        }

        Rectangle {
            id: scrollTrack
            x: 586
            y: rowsViewport.contentY
            width: 4
            height: rowsViewport.height
            color: "#222222"
            opacity: root.scrollBarShown || scrollBarHover.hovered ? 1 : 0

            Behavior on opacity { NumberAnimation { duration: opacity > 0 ? 150 : 300 } }

            Rectangle {
                x: 0
                y: rowsViewport.visibleArea.yPosition * parent.height
                width: 4
                height: Math.max(18, rowsViewport.visibleArea.heightRatio * parent.height)
                color: "#D7BC18"
            }

            HoverHandler { id: scrollBarHover }
        }
    }

    Timer {
        id: scrollHideTimer
        interval: 1000
        repeat: false
        onTriggered: root.scrollBarShown = false
    }

    MouseArea {
        anchors.fill: parent
        visible: root.openMenu.length > 0
        z: 20
        onClicked: root.openMenu = ""
    }

    Item {
        id: menuPanel
        visible: root.openMenu.length > 0
        x: root.menuX
        y: root.menuY
        width: 178
        height: root.menuItems.length * 30 + 8
        z: 21

        Rectangle {
            anchors.fill: parent
            color: "#1A1A1A"
            border.width: 1
            border.color: "#3D3D3D"
        }
        Rectangle { x: 0; y: 0; width: 10; height: 2; color: "#D7BC18" }
        Rectangle { x: 0; y: 0; width: 2; height: 10; color: "#D7BC18" }

        Repeater {
            model: root.menuItems

            delegate: Item {
                required property int index
                required property string modelData
                x: 4
                y: 4 + index * 30
                width: 170
                height: 30

                Rectangle { anchors.fill: parent; color: menuPointer.containsMouse ? "#D3D4CF" : "transparent" }
                Rectangle {
                    x: 9
                    y: 11
                    width: 8
                    height: 8
                    color: index === root.selectedOption(root.openMenu) ? "#FFFA00" : "transparent"
                    border.width: index === root.selectedOption(root.openMenu) ? 0 : 1
                    border.color: "#555555"
                }
                Rectangle {
                    visible: menuPointer.containsMouse
                    x: 0
                    y: 0
                    width: 3
                    height: 30
                    color: "#111111"
                }
                Text {
                    x: 28
                    y: 0
                    width: 132
                    height: 30
                    text: modelData
                    color: menuPointer.containsMouse ? "#111111" : "#D9D9D9"
                    font.family: harmonyRegular.name
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                MouseArea {
                    id: menuPointer
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.pickOption(index)
                }
            }
        }
    }

    Item {
        id: bubbleLayer
        anchors.fill: parent
        z: 40

        Item {
            visible: root.bubbleShown
            x: root.bubbleX - width / 2
            y: root.bubbleY - height - 6
            width: bubbleLabel.implicitWidth + 16
            height: 25
            opacity: root.bubbleShown ? 1 : 0
            scale: root.bubbleShown ? 1 : 0.9

            Behavior on opacity { NumberAnimation { duration: 140 } }
            Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutBack } }

            Rectangle {
                x: 0
                y: 0
                width: parent.width
                height: 21
                radius: 10
                color: "#FAFAFA"
            }
            Text {
                id: bubbleLabel
                x: 8
                y: 0
                height: 21
                text: root.bubbleText
                color: "#111111"
                font.family: harmonyBold.name
                font.pixelSize: 11
                font.weight: Font.Bold
                verticalAlignment: Text.AlignVCenter
            }
            Canvas {
                x: parent.width / 2 - 4
                y: 21
                width: 8
                height: 4
                onPaint: {
                    const context = getContext("2d")
                    context.reset()
                    context.beginPath()
                    context.moveTo(0, 0)
                    context.lineTo(8, 0)
                    context.lineTo(4, 4)
                    context.closePath()
                    context.fillStyle = "#FAFAFA"
                    context.fill()
                }
            }
        }
    }
}
