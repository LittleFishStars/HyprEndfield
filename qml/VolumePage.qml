import QtQuick
import "ModuleNumbering.js" as ModuleNumbering
import "Appearance.js" as Appearance
import QtQuick.Shapes
import QtQuick.Effects

// EndfieldHUD VolumeCanvas.swift. Geometry and motion use its shared HUD design
// components; PipeWire replaces only the macOS hardware/per-process backend.
Item {
    id: volumePage
    width: 590; height: 510
    property bool presented: false
    property bool reduceMotion: false
    readonly property var audio: audioController.state
    property int tab: 0
    property int requestedTab: 0
    property string motionPhase: "idle"
    property int direction: 1
    property var selectedSlider: null
    property var activeSlider: null
    property bool bubbleShown: false
    property string bubbleText: ""
    property point bubblePoint: Qt.point(0, 0)
    property real appOffset: 0
    readonly property real appHeight: audio.status ? 114 : 174
    readonly property real maxAppOffset: Math.max(0, apps.count * 60 - 6 - appHeight)
    property bool scrollbarActive: false
    readonly property bool menuOpen: output.opened || input.opened
    ListModel { id: apps }
    function cancelInteractions() {
        if (activeSlider) activeSlider.cancelDrag();
        activeSlider = null; bubbleShown = false;
        output.opened = false; input.opened = false;
    }
    function syncApplications() {
        const values = audio.applications;
        const same = apps.count === values.length && values.every((item, index) => apps.get(index).key === item.key);
        if (!same && activeSlider && activeSlider.dragging) {
            if (values.some(item => item.key === activeSlider.sourceKey)) return;
            activeSlider.cancelDrag(); activeSlider = null;
        }
        if (!same) { selectedSlider = null; apps.clear(); }
        values.forEach((item, index) => {
            const row = { key: item.key, label: item.label, pid: item.pid, icon: item.icon, volume: item.volume };
            if (same) apps.set(index, row); else apps.append(row);
        });
        appOffset = Math.max(0, Math.min(maxAppOffset, appOffset));
    }
    function navigate(next) {
        if (next === requestedTab || motionPhase !== "idle") return;
        cancelInteractions(); selectedSlider = null;
        syncApplications();
        direction = next > tab ? 1 : -1; requestedTab = next;
        if (reduceMotion) tab = next;
        else { motionPhase = "leave"; swap.restart(); }
    }
    function bubble(shown, text, x, y) {
        bubbleShown = shown;
        if (shown) { bubbleText = text; bubblePoint = Qt.point(x, y); }
    }
    function nudge(delta) {
        if (menuOpen) { (output.opened ? output : input).move(delta > 0 ? 1 : -1); return; }
        const slider = selectedSlider || (tab === 0 ? master : applicationRows.count ? applicationRows.itemAt(0).slider : null);
        if (slider) slider.nudge(delta);
    }
    function scrollApps(delta, animated) {
        scrollGlide.stop();
        const target = Math.max(0, Math.min(maxAppOffset, appOffset + delta));
        if (animated && !reduceMotion) { scrollGlide.to = target; scrollGlide.start(); }
        else appOffset = target;
        scrollbarActive = true; scrollbarLinger.restart();
    }
    onPresentedChanged: {
        cancelInteractions(); swap.stop(); settle.stop(); motionPhase = "idle"; tab = requestedTab;
        audioController.active = presented;
        syncApplications();
    }
    onEnabledChanged: if (!enabled) cancelInteractions()
    onReduceMotionChanged: if (reduceMotion) { swap.stop(); settle.stop(); motionPhase = "idle"; tab = requestedTab; }
    Component.onDestruction: audioController.active = false
    Connections {
        target: audioController
        function onStateChanged() { volumePage.syncApplications(); }
    }
    Timer { id: swap; interval: 130; onTriggered: { volumePage.tab = volumePage.requestedTab; volumePage.motionPhase = "enter"; settle.restart(); } }
    Timer { id: settle; interval: 620; onTriggered: volumePage.motionPhase = "idle" }
    Timer { id: scrollbarLinger; interval: 1000; onTriggered: volumePage.scrollbarActive = false }
    NumberAnimation { id: scrollGlide; target: volumePage; property: "appOffset"; duration: 200; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] }
    Shortcut { sequence: "Q"; enabled: volumePage.presented && volumePage.enabled && !volumePage.menuOpen; onActivated: volumePage.navigate(1 - volumePage.tab) }
    Shortcut { sequence: "E"; enabled: volumePage.presented && volumePage.enabled && !volumePage.menuOpen; onActivated: volumePage.navigate(1 - volumePage.tab) }
    Shortcut { sequence: "Left"; enabled: volumePage.presented && volumePage.enabled; onActivated: volumePage.nudge(-2) }
    Shortcut { sequence: "Right"; enabled: volumePage.presented && volumePage.enabled; onActivated: volumePage.nudge(2) }
    Shortcut { sequence: "Up"; enabled: volumePage.presented && volumePage.enabled && volumePage.menuOpen; onActivated: (output.opened ? output : input).move(-1) }
    Shortcut { sequence: "Down"; enabled: volumePage.presented && volumePage.enabled && volumePage.menuOpen; onActivated: (output.opened ? output : input).move(1) }
    Shortcut { sequences: ["Return", "Enter"]; enabled: volumePage.presented && volumePage.enabled && volumePage.menuOpen; onActivated: (output.opened ? output : input).choose() }

    Item {
        id: header
        width: 590; height: 81
        Row {
            y: 1.75; spacing: 6
            Repeater {
                model: ["// HUD", "/", "VOLUME"]
                Text {
                    required property string modelData; required property int index
                    text: modelData; color: index === 2 ? Appearance.tone("#E6E6E6") : Appearance.tone("#8A8A8A")
                    font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; font.letterSpacing: 0.63
                    renderType: Text.CurveRendering
                }
            }
        }
        Text {
            x: 549 - width; y: 1.75; text: ModuleNumbering.code("volume"); color: Appearance.tone("#8A8A8A")
            font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; font.letterSpacing: 0.63; renderType: Text.CurveRendering
        }
        Item {
            x: 556; y: 3.5; width: 34; height: 7; clip: true
            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                ShapePath { strokeWidth: -1; fillColor: Qt.rgba(1, 1, 1, 0.14); PathSvg { path: { let p = ""; for (let x = -7; x < 41; x += 3.5 * Math.SQRT2) p += "M" + x + " 7h1.414l7 -7h-1.414Z"; return p; } } }
            }
        }
        Text {
            id: title; y: 20; height: 30
            text: qsTr("音量"); color: Appearance.tone("#F4F4F4"); font.family: harmonyBold.name; font.pixelSize: 26; font.weight: Font.Bold; font.letterSpacing: 2.08
            renderType: Text.CurveRendering
        }
        Text {
            x: title.width + 12; y: title.y + title.baselineOffset - baselineOffset
            text: "VOLUME"; color: Appearance.tone("#8A8A8A"); font.family: harmonyMedium.name; font.pixelSize: 13; font.letterSpacing: 2.6; font.weight: Font.Medium
            renderType: Text.CurveRendering
        }
        Item {
            y: 60; width: 590; height: 9
            Rectangle { y: 4; width: 590; height: 1; color: "#38FFFFFF" }
            Rectangle { y: 3; width: 56; height: 3; color: Appearance.accent("#FFFA00") }
            Repeater { model: 10; Rectangle { required property int index; x: 454 + index * 9; width: 1; height: 9; color: "#59FFFFFF" } }
            Rectangle { x: 590 - width; y: -2; width: counter.width + 7; height: 12; color: Appearance.tone("#151515") }
            Text {
                id: counter; x: 590 - width; y: -1.5
                text: volumePage.audio.muted ? qsTr("已静音") : String(Math.round(master.dragging ? master.value : volumePage.audio.volume)).padStart(2, "0")
                color: Appearance.accent("#FFFA00"); font.family: harmonyBold.name; font.pixelSize: 11; font.weight: Font.Bold; renderType: Text.CurveRendering
            }
        }
    }
    NowPlayingSection {
        page: volumePage; sectionIndex: 0; x: 0; y: 81; width: 590; height: 34
        Rectangle { anchors.fill: parent; color: Appearance.tone("#1E1E1E"); border.color: Appearance.tone("#343434"); border.width: 1 }
        Repeater {
            model: ["Q", "E"]
            Rectangle {
                required property string modelData; required property int index
                x: index ? 568.5 : 6.5; y: 9.5; width: 15; height: 15; color: "transparent"; border.width: 1; border.color: Appearance.tone("#6A6A6A")
                Text { anchors.centerIn: parent; text: parent.modelData; color: Appearance.tone("#8A8A8A"); font.family: harmonyBold.name; font.pixelSize: 9; font.weight: Font.Bold; renderType: Text.CurveRendering }
            }
        }
        Repeater {
            model: [qsTr("设备"), qsTr("应用音量")]
            Item {
                id: tabButton
                required property string modelData; required property int index
                readonly property bool selected: volumePage.requestedTab === index
                property real fillAmount: selected ? 1 : 0
                x: 28 + index * 267; y: selected ? -3 : tabPointer.containsMouse ? -1 : 0; width: 267; height: selected ? 37 : 34
                Behavior on y { NumberAnimation { duration: volumePage.reduceMotion ? 0 : 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] } }
                Behavior on fillAmount { NumberAnimation { duration: volumePage.reduceMotion ? 0 : 200; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] } }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter; width: parent.width * tabButton.fillAmount; height: parent.height
                    gradient: Gradient { GradientStop { position: 0; color: Appearance.accent("#FFFC6B") } GradientStop { position: 0.45; color: Appearance.accent("#FFFA00") } }
                }
                Rectangle {
                    anchors.fill: parent; color: "white"; opacity: !tabButton.selected && tabPointer.containsMouse ? 0.06 : 0
                    Behavior on opacity { NumberAnimation { duration: volumePage.reduceMotion ? 0 : 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1] } }
                }
                Rectangle { x: parent.width * 0.32; y: parent.height + 3; width: parent.width * 0.36; height: 3; color: Appearance.accent("#FFFA00"); visible: tabButton.selected }
                Item {
                    width: 21 + tabLabel.width; height: 16; anchors.centerIn: parent
                    Shape {
                        y: 1; width: 24; height: 24; scale: 14 / 24; transformOrigin: Item.TopLeft; preferredRendererType: Shape.CurveRenderer
                        ShapePath {
                            strokeWidth: -1; fillColor: tabButton.selected ? Appearance.tone("#111111") : Appearance.tone("#D8D8D8")
                            PathSvg { path: tabButton.index === 0 ? "M18 8h4v11h-4Z" : "M3 3h7v7H3ZM14 3h7v7h-7ZM3 14h7v7H3Z" }
                        }
                        ShapePath {
                            strokeWidth: 2.4; strokeColor: tabButton.selected ? Appearance.tone("#111111") : Appearance.tone("#D8D8D8"); fillColor: "transparent"; capStyle: ShapePath.FlatCap; joinStyle: ShapePath.MiterJoin
                            PathSvg { path: tabButton.index === 0 ? "M2 5h14v10H2ZM5 19h8" : "M14 14h7v7h-7Z" }
                        }
                    }
                    Text {
                        id: tabLabel; x: 21; anchors.verticalCenter: parent.verticalCenter; text: tabButton.modelData
                        color: tabButton.selected ? Appearance.tone("#111111") : tabPointer.containsMouse ? "white" : Appearance.tone("#D8D8D8")
                        font.family: tabButton.selected ? harmonyBold.name : harmonyMedium.name; font.pixelSize: 13; font.letterSpacing: 0.52; font.weight: tabButton.selected ? Font.Bold : Font.Medium; renderType: Text.CurveRendering
                    }
                }
                Rectangle { visible: tabButton.index === 1 && apps.count > 0; x: parent.width - 13; y: 6; width: 7; height: 7; rotation: 45; color: "#FF9B00" }
                MouseArea { id: tabPointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; enabled: volumePage.motionPhase === "idle"; onClicked: volumePage.navigate(tabButton.index) }
            }
        }
    }
    NowPlayingSection {
        page: volumePage; sectionIndex: 1; y: 125; width: 590; height: 44; visible: volumePage.tab === 0
        VolumeRow {
            title: qsTr("输出"); reduceMotion: volumePage.reduceMotion
            VolumeDropdown {
                id: output; objectName: "outputSelector"; x: 400; y: 8; overlayHost: volumePage; options: volumePage.audio.outputs; selectedKey: volumePage.audio.outputKey; text: volumePage.audio.outputName
                reduceMotion: volumePage.reduceMotion; enabled: volumePage.audio.ready && options.length > 0
                onSelected: key => audioController.selectDevice(key, false)
            }
        }
    }
    NowPlayingSection {
        page: volumePage; sectionIndex: 2; y: 175; width: 590; height: 44; visible: volumePage.tab === 0
        VolumeRow {
            title: qsTr("输入"); reduceMotion: volumePage.reduceMotion
            VolumeDropdown {
                id: input; objectName: "inputSelector"; x: 400; y: 8; overlayHost: volumePage; options: volumePage.audio.inputs; selectedKey: volumePage.audio.inputKey; text: volumePage.audio.inputName
                reduceMotion: volumePage.reduceMotion; enabled: volumePage.audio.ready && options.length > 0
                onSelected: key => audioController.selectDevice(key, true)
            }
        }
    }
    NowPlayingSection {
        page: volumePage; sectionIndex: 3; y: 225; width: 590; height: 44; visible: volumePage.tab === 0
        VolumeRow {
            title: qsTr("音量"); reduceMotion: volumePage.reduceMotion
            VolumeSlider {
                id: master; objectName: "masterSlider"; x: 202; y: 8; width: 338
                inputValue: volumePage.audio.volume; sourceKey: volumePage.audio.outputKey; enabled: volumePage.audio.ready && sourceKey !== ""; reduceMotion: volumePage.reduceMotion; bubbleHost: volumePage
                onTouched: { volumePage.selectedSlider = master; volumePage.activeSlider = master; }
                onValueModified: (value, final) => audioController.setVolume(value, gestureKey)
                onBubbleRequested: (shown, text, x, y) => volumePage.bubble(shown, text, x, y)
            }
            NowPlayingButton {
                x: 548; y: 8; kind: volumePage.audio.muted ? "mute" : "volume"; selected: volumePage.audio.muted; label: selected ? qsTr("取消静音") : qsTr("静音")
                enabled: master.enabled; reduceMotion: volumePage.reduceMotion
                onTriggered: audioController.setMuted(!volumePage.audio.muted, volumePage.audio.outputKey)
            }
        }
    }
    NowPlayingSection {
        page: volumePage; sectionIndex: 4; y: 275; width: 590; height: 44; visible: volumePage.tab === 0
        VolumeRow {
            title: qsTr("平衡"); reduceMotion: volumePage.reduceMotion
            VolumeSlider {
                id: balance; objectName: "balanceSlider"; x: 202; y: 8; width: 376
                minimum: -100; maximum: 100; zeroValue: 0; valueFormat: "balance"
                inputValue: volumePage.audio.balance; sourceKey: volumePage.audio.outputKey; enabled: volumePage.audio.ready && sourceKey !== "" && volumePage.audio.canBalance; reduceMotion: volumePage.reduceMotion; bubbleHost: volumePage
                onTouched: { volumePage.selectedSlider = balance; volumePage.activeSlider = balance; }
                onValueModified: (value, final) => audioController.setBalance(value, gestureKey)
                onBubbleRequested: (shown, text, x, y) => volumePage.bubble(shown, text, x, y)
            }
        }
    }
    Item {
        id: applicationViewport; y: 125; width: 590; height: volumePage.appHeight; visible: volumePage.tab === 1; clip: true
        MouseArea {
            anchors.fill: parent
            property real origin: 0
            property real offset: 0
            onPressed: mouse => { scrollGlide.stop(); origin = mouse.y; offset = volumePage.appOffset; }
            onPositionChanged: mouse => { if (pressed) volumePage.scrollApps(offset - (mouse.y - origin) - volumePage.appOffset, false); }
            onWheel: wheel => { volumePage.scrollApps(-(wheel.pixelDelta.y || wheel.angleDelta.y / 3), true); wheel.accepted = true; }
        }
        Item {
            y: -volumePage.appOffset; width: volumePage.maxAppOffset > 0 ? 580 : 590; height: apps.count * 60
            Repeater {
                id: applicationRows; model: apps
                delegate: NowPlayingSection {
                    id: appSection
                    required property int index
                    required property string key
                    required property string label
                    required property string pid
                    required property string icon
                    required property real volume
                    property alias slider: appSlider
                    page: volumePage; sectionIndex: index + 1; y: index * 60; width: parent.width; height: 54
                    VolumeRow {
                        width: appSection.width; title: appSection.label; subtitle: "PID " + (appSection.pid || "—"); applicationRow: true; icon: appSection.icon; reduceMotion: volumePage.reduceMotion
                        VolumeSlider {
                            id: appSlider; x: 202; y: 13; width: parent.width - 214
                            sourceKey: appSection.key; inputValue: appSection.volume; enabled: volumePage.audio.ready; reduceMotion: volumePage.reduceMotion; bubbleHost: volumePage
                            onTouched: { volumePage.selectedSlider = appSlider; volumePage.activeSlider = appSlider; }
                            onValueModified: (value, final) => {
                                audioController.setApplicationVolume(value, gestureKey);
                                if (final) Qt.callLater(volumePage.syncApplications);
                            }
                            onBubbleRequested: (shown, text, x, y) => volumePage.bubble(shown, text, x, y)
                        }
                    }
                }
            }
        }
        Rectangle {
            x: 586; width: 4; height: parent.height; visible: volumePage.maxAppOffset > 0
            color: Appearance.tone("#222222"); opacity: volumePage.scrollbarActive || scrollHover.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: volumePage.reduceMotion ? 0 : 220 } }
            Rectangle { width: 4; height: Math.max(18, parent.height * parent.height / (apps.count * 60 - 6)); y: volumePage.appOffset / Math.max(1, volumePage.maxAppOffset) * (parent.height - height); color: Appearance.accent("#FFFA00") }
        }
        Item { x: 578; width: 12; height: parent.height; HoverHandler { id: scrollHover } }
        NowPlayingSection {
            page: volumePage; sectionIndex: 1; anchors.fill: parent; visible: apps.count === 0
            Rectangle { anchors.fill: parent; color: "transparent"; border.width: 1; border.color: Appearance.tone("#3A3A3A") }
            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeWidth: -1; fillColor: Qt.rgba(1, 1, 1, 0.14)
                    PathSvg { path: { let p = ""; const h = volumePage.appHeight; for (let x = -h; x < 590; x += 3.5 * Math.SQRT2) p += "M" + x + " " + h + "l" + h + " -" + h + "h1.414l-" + h + " " + h + "Z"; return p; } }
                }
            }
            Text {
                anchors.centerIn: parent
                text: qsTr("<font color='#FFFA00'>[ </font>暂无可调节音量的应用<font color='#FFFA00'> ]</font>"); textFormat: Text.RichText
                color: Appearance.tone("#E9E9E9"); font.family: harmonyBold.name; font.pixelSize: 20; font.weight: Font.Bold; font.letterSpacing: 4.8; renderType: Text.CurveRendering
            }
        }
    }
    Text {
        visible: volumePage.audio.status !== ""; x: 18; y: volumePage.tab === 0 ? 329 : 125 + volumePage.appHeight + 10; width: 554; height: 30.8
        text: volumePage.audio.status; color: Appearance.tone("#8A8A8A"); font.family: harmonyRegular.name; font.pixelSize: 11; lineHeight: 1.4; wrapMode: Text.Wrap; maximumLineCount: 2; renderType: Text.CurveRendering
    }
    Item {
        id: valueBubble; z: 40; x: volumePage.bubblePoint.x - width / 2; y: volumePage.bubblePoint.y - height - 2; width: bubbleLabel.width + 16; height: 25
        opacity: volumePage.bubbleShown ? 1 : 0; scale: volumePage.bubbleShown ? 1 : 0.9; transformOrigin: Item.Bottom
        Behavior on opacity { NumberAnimation { duration: volumePage.reduceMotion ? 0 : 140; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1] } }
        Behavior on scale { NumberAnimation { duration: volumePage.reduceMotion ? 0 : 180; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.9, 0.1, 1, 1, 1] } }
        Rectangle { y: 2; width: parent.width; height: 21; radius: 10; color: Appearance.tone("#111111") }
        Rectangle {
            width: parent.width; height: 21; radius: 10; color: Appearance.tone("#FAFAFA")
            layer.enabled: volumePage.bubbleShown
            layer.effect: MultiEffect { shadowEnabled: true; shadowColor: Qt.rgba(0, 0, 0, 0.45); shadowVerticalOffset: 6; shadowBlur: 1; blurMax: 8 }
        }
        Shape {
            x: valueBubble.width / 2 - 4; y: 21; width: 8; height: 4
            preferredRendererType: Shape.CurveRenderer
            ShapePath { strokeWidth: -1; fillColor: Appearance.tone("#FAFAFA"); PathSvg { path: "M0 0h8l-4 4Z" } }
        }
        Text { id: bubbleLabel; x: 8; y: 5; text: volumePage.bubbleText; color: Appearance.tone("#111111"); font.family: harmonyBold.name; font.pixelSize: 11; font.weight: Font.Bold; renderType: Text.CurveRendering }
    }
}
