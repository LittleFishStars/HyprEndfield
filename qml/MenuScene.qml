import QtQuick
import QtQuick.Shapes
import "MenuGeometry.js" as Geometry
import "WatchSurfaces.js" as Source

Item {
    id: menu
    required property var shell
    width: 1920
    height: 1080
    enabled: !shell.transitioning
    readonly property real leanX: Math.round(-Math.tan(shell.cameraYaw * Math.PI / 180) * 160) / 100
    readonly property real leanY: Math.round(Math.tan((shell.cameraPitch - 20) * Math.PI / 180) * 160) / 100
    property real flagSway: -30

    function translation(x, y, z) {
        return Qt.matrix4x4(1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z, 0, 0, 0, 1);
    }
    function turnZ(degrees) {
        const a = degrees * Math.PI / 180, c = Math.cos(a), s = Math.sin(a);
        return Qt.matrix4x4(c, -s, 0, 0, s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    }
    function scaling(value) {
        return Qt.matrix4x4(value, 0, 0, 0, 0, value, 0, 0, 0, 0, value, 0, 0, 0, 0, 1);
    }
    function cssMatrix(value) {
        const m = value.slice(9, -1).split(',').map(Number);
        return Qt.matrix4x4(m[0], m[4], m[8], m[12], m[1], m[5], m[9], m[13], m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15]);
    }
    function ringMotion(scaleKey, rotationKey) {
        return translation(960, 540, 0).times(turnZ(shell.frameValue(rotationKey, 0))).times(scaling(shell.frameValue(scaleKey, 1))).times(translation(-960, -540, 0));
    }
    function flagPose(degrees) {
        const a = degrees * Math.PI / 180;
        return ringMotion("triangleScale", "triangleRotation").times(translation(960, 540, 0)).times(turnZ(flagSway)).times(translation(-960, -540, 0)).times(translation(960 + .96 * Geometry.data.flagRadius * Math.cos(a), 540 + .96 * Geometry.data.flagRadius * Math.sin(a), 0)).times(turnZ(degrees + 90)).times(Qt.matrix4x4(1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1));
    }

    SequentialAnimation on flagSway {
        running: appController.hudVisible && menu.shell.displaySettings.ambientAnimation && !menu.shell.displaySettings.reduceMotion && !menu.shell.displaySettings.lowPowerVisualMode
        loops: Animation.Infinite
        NumberAnimation {
            from: -30
            to: 30
            duration: 45000
        }
        NumberAnimation {
            from: 30
            to: -30
            duration: 45000
        }
    }

    MenuPlane {
        scene: menu.shell
        elevation: -130
        compensated: false
        opacity: menu.shell.ringOpacity("mainOpacity")
        localMatrix: menu.ringMotion("mainScale", "mainRotation")
        MenuArt {
            x: 513
            y: 93
            width: 894
            height: 894
            asset: "well"
        }
    }
    Repeater {
        model: Geometry.data.rings
        delegate: MenuPlane {
            required property int index
            required property var modelData
            scene: menu.shell
            elevation: modelData.height
            compensated: false
            readonly property string scaleKey: ["mainScale", "secondaryScale", "blueScale", "outerScale"][index]
            readonly property string rotationKey: ["mainRotation", "secondaryRotation", "blueRotation", "outerRotation"][index]
            opacity: menu.shell.ringOpacity(["mainOpacity", "secondaryOpacity", "blueOpacity", "outerOpacity"][index])
            localMatrix: menu.ringMotion(scaleKey, rotationKey)
            MenuArt {
                x: 480 - 180 * .96
                y: 60 - 180 * .96
                width: 1360 * .96
                height: 1360 * .96
                asset: modelData.id
            }
        }
    }
    Repeater {
        model: Source.data.outer
        delegate: SourceSurface {
            required property var modelData
            shell: menu.shell
            spec: modelData
            ambientTime: menu.shell.sourceAmbientSampleTime
            z: -200
            opacity: menu.shell.ringOpacity(modelData.group === "outer" ? "outerOpacity" : "secondaryOpacity")
        }
    }
    Repeater {
        model: Geometry.data.flags
        delegate: MenuPlane {
            required property real modelData
            scene: menu.shell
            elevation: 22
            compensated: false
            z: 55
            localMatrix: menu.flagPose(modelData)
            opacity: menu.shell.ringOpacity("blueOpacity")
            MenuArt {
                x: -30.72
                y: -59.52
                width: 61.44
                height: 61.44
                asset: "flag"
            }
        }
    }
    Repeater {
        model: Geometry.data.marks
        delegate: MenuPlane {
            required property var modelData
            scene: menu.shell
            compensated: false
            localMatrix: menu.cssMatrix(modelData.m)
            z: Number(modelData.m.slice(9, -1).split(',')[14])
            opacity: menu.shell.contentFrameValue("nativeOpacity", 1) * modelData.a
            Rectangle {
                x: modelData.g[0]
                y: -modelData.wh / 2
                width: modelData.g[1]
                height: modelData.wh
                color: "white"
            }
            Rectangle {
                x: -modelData.g[0] - modelData.g[1]
                y: -modelData.wh / 2
                width: modelData.g[1]
                height: modelData.wh
                color: "white"
            }
            Rectangle {
                x: -modelData.wv / 2
                y: modelData.g[2]
                width: modelData.wv
                height: modelData.g[3]
                color: "white"
            }
            Rectangle {
                x: -modelData.wv / 2
                y: -modelData.g[2] - modelData.g[3]
                width: modelData.wv
                height: modelData.g[3]
                color: "white"
            }
        }
    }
    // Cables live on the deck, independently of the raised cards.
    Repeater {
        model: 4
        delegate: MenuPlane {
            required property int index
            scene: menu.shell
            elevation: 0
            opacity: menu.shell.navigationListValue("leftAlpha", index, 1)
            MenuArt {
                anchors.fill: parent
                asset: "card-" + index + "-links"
            }
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 0
        opacity: menu.shell.baseContentFrameValue("nativeOpacity", 1)
        SystemPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "system"
        }
        DisplayPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "display"
            settings: menu.shell.displaySettings
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 0
        opacity: menu.shell.baseContentFrameValue("nativeOpacity", 1)
        NowPlayingPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "nowPlaying"
            presented: visible && appController.hudVisible
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        VolumePage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "volume"
            presented: visible && appController.hudVisible
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        TerminalPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "hotkeys"
            presented: visible && appController.hudOpen && !menu.shell.transitioning
            rasterScale: menu.shell.rasterScale * scale
        }
        ActivityPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "activity"
            presented: visible && appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        CalendarPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "calendar"
            presented: visible && appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        ClipboardPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "clipboard"
            presented: visible && appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        NotesPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "notes"
            presented: visible && appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        FileShelfPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "fileShelf"
            presented: visible && appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 34
        Repeater {
            model: menu.shell.leftButtons
            delegate: MenuCard {
                required property int index
                required property var modelData
                cardIndex: index
                label: modelData.label
                glyph: modelData.key
                leanX: menu.leanX
                leanY: menu.leanY
                opacity: menu.shell.navigationListValue("leftAlpha", index, 1)
                onTriggered: menu.shell.selectedModule = modelData.key
            }
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 28
        Item {
            id: viewport
            x: 1290
            y: 200
            width: 512
            height: 805
            clip: true
            // This is the only flattened interactive surface, for the source fade mask.
            // Render above its physical output resolution, including monitor scaling.
            layer.enabled: true
            layer.smooth: true
            layer.samples: 4
            layer.textureSize: Qt.size(Math.ceil(width * menu.shell.rasterScale), Math.ceil(height * menu.shell.rasterScale))
            layer.effect: ShaderEffect {
                fragmentShader: "qrc:/shaders/viewport-mask.frag.qsb"
            }
            Repeater {
                model: 6
                delegate: Item {
                    id: row
                    required property int index
                    readonly property real angle: (22.52 - index * 15.5 + menu.shell.rightScrollOffset) * Math.PI / 180
                    x: 960 + 582.32 * Math.cos(angle) - viewport.x
                    y: 468 - 490 * angle - viewport.y
                    width: 235
                    height: 104
                    transform: Rotation {
                        origin.x: 0
                        origin.y: 52
                        angle: -3
                    }
                    Repeater {
                        model: 2
                        delegate: MenuTile {
                            required property int index
                            readonly property int slot: row.index * 2 + index
                            x: index * 128
                            entry: menu.shell.rightButtons[slot]
                            secondColumn: index === 1
                            rowX: row.x
                            rowY: row.y
                            leanX: menu.leanX
                            leanY: menu.leanY
                            opacity: menu.shell.navigationListValue("rightAlpha", slot, 1)
                            onTriggered: menu.shell.selectedModule = entry.key
                            DropArea {
                                anchors.fill: parent
                                enabled: parent.entry.key === "fileShelf"
                                onEntered: drag => {
                                    drag.accepted = drag.hasUrls && drag.urls.every(url => String(url).startsWith("file:"))
                                }
                                onDropped: drop => {
                                    if (fileShelfController.addUrls(drop.urls)) {
                                        menu.shell.selectedModule = "fileShelf"
                                        drop.action = Qt.CopyAction
                                        drop.accepted = true
                                    }
                                }
                            }
                        }
                    }
                }
            }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                onWheel: wheel => {
                    menu.shell.scrollRightList(wheel.angleDelta.y < 0 ? 1 : -1);
                    wheel.accepted = true;
                }
            }
        }
        Repeater {
            model: 2
            delegate: Item {
                required property int index
                x: index === 0 ? 1600 : 1500
                y: index === 0 ? 220 : 980
                width: 70
                height: 30
                opacity: menu.shell.contentFrameValue("nativeOpacity", 1) * ((index === 0 ? menu.shell.rightScrollOffset > 0 : menu.shell.rightScrollOffset < 15.5) ? 0.82 : 0.18)
                rotation: -3
                Shape {
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        strokeColor: "#e5e5e5"
                        strokeWidth: 4
                        fillColor: "transparent"
                        PathSvg {
                            path: index === 0 ? "M20 22L35 8L50 22" : "M20 8L35 22L50 8"
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: menu.shell.scrollRightList(index === 0 ? -1 : 1)
                }
            }
        }
    }
    SourceSurface {
        shell: menu.shell
        spec: Source.data.shadow
        z: 25
        opacity: menu.shell.remainingContentFrameValue("storageOpacity", 1)
    }
    Repeater {
        model: Source.data.bottom
        delegate: SourceBottomButton {
            required property int index
            required property var modelData
            shell: menu.shell
            spec: modelData
            z: 26
            label: index === 0 ? "存储" : "活动监视器"
            opacity: menu.shell.remainingContentFrameValue(index === 0 ? "storageOpacity" : "activityOpacity", 1)
            onTriggered: menu.shell.selectedModule = index === 0 ? "storage" : "activity"
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 72
        opacity: menu.shell.baseContentFrameValue("nativeOpacity", 1)
        MenuClock {
            x: 1306.06
            y: 58.31
            timeText: menu.shell.clockText
            dateText: menu.shell.dateText
            seconds: menu.shell.clockSeconds
            transform: Matrix4x4 {
                matrix: Qt.matrix4x4(.9995, .0739, 0, 0, -.0306, .9972, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)
            }
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 64
        opacity: menu.shell.contentFrameValue("identityOpacity", 1)
        MenuBadge {
            x: 289
            y: 850
            leanX: menu.leanX
            leanY: menu.leanY
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 34
        opacity: menu.shell.contentFrameValue("closeOpacity", 1)
        Item {
            x: 202
            y: 875.5
            width: 53
            height: 53
            transform: Matrix4x4 {
                matrix: Qt.matrix4x4(1, -.02, 0, 0, -.029, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)
            }
            MenuArt {
                anchors.fill: parent
                asset: "power-plate"
            }
            HudGlyph {
                x: 6
                y: 6
                width: 41
                height: 41
                kind: "power"
                ink: "#c7edede8"
            }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
            }
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 144
        opacity: menu.shell.contentFrameValue("nativeOpacity", 1)
        Text {
            x: 620
            y: 72
            text: "HYPRENDFIELD"
            font.family: harmonyBold.name
            font.pixelSize: 32
            color: "#f2f2f2"
            renderType: Text.CurveRendering
        }
        Text {
            x: 620
            y: 114
            text: "SYSTEM INTERFACE"
            font.family: harmonyMedium.name
            font.pixelSize: 14
            font.letterSpacing: 1.4
            color: "#8f9496"
            renderType: Text.CurveRendering
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 0
        opacity: menu.shell.contentFrameValue("wordmarkOpacity", 1)
        Image {
            x: 835
            y: 1015
            width: 250
            height: 48.25
            source: "qrc:/assets/watch/endfield_industries.png"
            sourceClipRect: Qt.rect(0, 142, 430, 83)
            smooth: true
            mipmap: true
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 34
        z: 500
        opacity: menu.shell.baseContentFrameValue("nativeOpacity", 1)
        NotesWorkspace {
            selectedModule: menu.shell.selectedModule
            presented: appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
    }
    Text {
        x: 760
        y: 1120
        width: 400
        height: 30
        text: "SUPER + ESC TO CLOSE"
        color: "#8a8d8e"
        font.family: harmonyRegular.name
        font.pixelSize: 14
        horizontalAlignment: Text.AlignHCenter
        renderType: Text.CurveRendering
        opacity: menu.shell.contentFrameValue("nativeOpacity", 1)
    }
}
