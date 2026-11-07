import QtQuick
import "Appearance.js" as Appearance
import QtQuick.Shapes
import "ActivityFormat.js" as Format

// EndfieldHUD ActivityMonitorCanvas / activity_canvas.cpp, SYS-11.
Item {
    id: activityPage
    width: 590; height: 510
    property bool presented: false
    property bool reduceMotion: false
    readonly property var activity: activityController.state
    readonly property var apps: activity.apps
    property int tab: 0
    property int requestedTab: 0
    property string motionPhase: "idle"
    property int direction: 1
    property real appOffset: 0
    readonly property real maxAppOffset: Math.max(0, apps.length * 46 - 6 - 330)
    readonly property int firstVisible: Math.floor(appOffset / 46)
    property bool scrollActive: false
    property int sortRevision: 0
    readonly property var columns: [
        { key: "name", title: qsTr("应用"), x: 18, w: 274 },
        { key: "gpu", title: "GPU", x: 300, w: 60 },
        { key: "cpu", title: "CPU", x: 368, w: 60 },
        { key: "memory", title: "RAM", x: 436, w: 66 },
        { key: "threads", title: qsTr("线程"), x: 510, w: 62 }
    ]
    function syncDemand() { activityController.applications = presented && requestedTab === 1; activityController.active = presented; }
    function navigate(next, directionHint) {
        if (next === requestedTab || motionPhase !== "idle") return;
        direction = directionHint === undefined ? (next > tab ? 1 : -1) : directionHint;
        requestedTab = next; syncDemand();
        if (reduceMotion) tab = next;
        else { motionPhase = "leave"; swap.restart(); }
    }
    function sort(key) {
        if (tab !== 1 || motionPhase !== "idle") return;
        activityController.setSort(key, activity.sortKey === key ? !activity.descending : key !== "name");
        ++sortRevision;
    }
    function scroll(delta, animated) {
        glide.stop(); const target = Math.max(0, Math.min(maxAppOffset, appOffset + delta));
        if (animated && !reduceMotion) { glide.to = target; glide.start(); } else appOffset = target;
        scrollActive = true; linger.restart();
    }
    onMaxAppOffsetChanged: if (appOffset > maxAppOffset) { glide.stop(); appOffset = maxAppOffset; }
    onPresentedChanged: {
        swap.stop(); settle.stop(); glide.stop(); linger.stop(); scrollActive = false;
        motionPhase = "idle"; tab = requestedTab; syncDemand();
    }
    onReduceMotionChanged: if (reduceMotion) { swap.stop(); settle.stop(); motionPhase = "idle"; tab = requestedTab; }
    Component.onDestruction: { activityController.active = false; activityController.applications = false; }
    Timer { id: swap; interval: 130; onTriggered: { activityPage.tab = activityPage.requestedTab; activityPage.motionPhase = "enter"; settle.restart(); } }
    Timer { id: settle; interval: 440 + 36 * (activityPage.tab === 0 ? 5 : 10); onTriggered: activityPage.motionPhase = "idle" }
    Timer { id: linger; interval: 1000; onTriggered: activityPage.scrollActive = false }
    NumberAnimation { id: glide; target: activityPage; property: "appOffset"; duration: 200; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] }
    Shortcut { sequence: "Q"; enabled: activityPage.presented && activityPage.enabled; onActivated: activityPage.navigate(1 - activityPage.tab, -1) }
    Shortcut { sequence: "E"; enabled: activityPage.presented && activityPage.enabled; onActivated: activityPage.navigate(1 - activityPage.tab, 1) }

    Item {
        width: 590; height: 81
        Row {
            y: 1.75; spacing: 6
            Repeater {
                model: ["// HUD", "/", "ACTIVITY"]
                Text {
                    required property string modelData; required property int index
                    text: modelData; color: index === 2 ? Appearance.tone("#E6E6E6") : Appearance.tone("#8A8A8A")
                    font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; font.letterSpacing: 0.63; renderType: Text.CurveRendering
                }
            }
        }
        Text {
            x: 549 - width; y: 1.75; text: "SYS-11"; color: Appearance.tone("#8A8A8A")
            font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; font.letterSpacing: 0.63; renderType: Text.CurveRendering
        }
        Item {
            x: 556; y: 3.5; width: 34; height: 7; clip: true
            Shape {
                anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
                ShapePath { strokeWidth: -1; fillColor: Qt.rgba(1, 1, 1, 0.14); PathSvg { path: { let p = ""; for (let x = -7; x < 41; x += 3.5 * Math.SQRT2) p += "M" + x + " 7h1.414l7 -7h-1.414Z"; return p; } } }
            }
        }
        Text {
            id: title; y: 20; height: 30; text: qsTr("活动监视器"); color: Appearance.tone("#F4F4F4")
            font.family: harmonyBold.name; font.pixelSize: 26; font.weight: Font.Bold; font.letterSpacing: 2.08; renderType: Text.CurveRendering
        }
        Text {
            x: title.width + 12; y: title.y + title.baselineOffset - baselineOffset; text: "ACTIVITY"; color: Appearance.tone("#8A8A8A")
            font.family: harmonyMedium.name; font.pixelSize: 13; font.letterSpacing: 2.6; font.weight: Font.Medium; renderType: Text.CurveRendering
        }
        Item {
            y: 60; width: 590; height: 9
            Rectangle { y: 4; width: 590; height: 1; color: "#38FFFFFF" }
            Rectangle { y: 3; width: 56; height: 3; color: Appearance.accent("#FFFA00") }
            Repeater { model: 10; Rectangle { required property int index; x: 454 + index * 9; width: 1; height: 9; color: "#59FFFFFF" } }
            Rectangle { x: 590 - width; y: -2; width: counter.width + 7; height: 12; color: Appearance.tone("#151515") }
            Text {
                id: counter; objectName: "activityCounter"; x: 590 - width; y: -1.5
                text: activityPage.tab === 0 ? "LIVE" : String(activityPage.apps.length).padStart(2, "0")
                color: Appearance.accent("#FFFA00"); font.family: harmonyBold.name; font.pixelSize: 11; font.weight: Font.Bold; renderType: Text.CurveRendering
            }
        }
    }
    NowPlayingSection {
        page: activityPage; sectionIndex: 0; y: 81; width: 590; height: 34
        Rectangle { anchors.fill: parent; color: Appearance.tone("#1E1E1E"); border.color: Appearance.tone("#343434") }
        Repeater {
            model: ["Q", "E"]
            Rectangle {
                required property string modelData; required property int index
                x: index ? 568.5 : 6.5; y: 9.5; width: 15; height: 15; color: "transparent"; border.color: Appearance.tone("#6A6A6A")
                Text { anchors.centerIn: parent; text: parent.modelData; color: Appearance.tone("#8A8A8A"); font.family: harmonyBold.name; font.pixelSize: 9; font.weight: Font.Bold; renderType: Text.CurveRendering }
            }
        }
        Repeater {
            model: [qsTr("概览"), qsTr("应用")]
            Item {
                id: tabButton
                required property string modelData; required property int index
                readonly property bool selected: activityPage.requestedTab === index
                property real fillAmount: selected ? 1 : 0
                objectName: index === 0 ? "overviewTab" : "applicationsTab"
                x: 28 + index * 267; y: selected ? -3 : 0; width: 267; height: selected ? 37 : 34
                transform: Translate { y: !tabButton.selected && pointer.containsMouse ? -1 : 0; Behavior on y { NumberAnimation { duration: activityPage.reduceMotion ? 0 : 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] } } }
                Behavior on fillAmount { NumberAnimation { duration: activityPage.reduceMotion ? 0 : 200; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] } }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter; width: parent.width * tabButton.fillAmount; height: parent.height
                    gradient: Gradient { GradientStop { position: 0; color: Appearance.accent("#FFFC6B") } GradientStop { position: 0.45; color: Appearance.accent("#FFFA00") } }
                }
                Rectangle {
                    anchors.fill: parent; color: "white"; opacity: !tabButton.selected && pointer.containsMouse ? 0.06 : 0
                    Behavior on opacity { NumberAnimation { duration: activityPage.reduceMotion ? 0 : 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1] } }
                }
                Rectangle { x: parent.width * 0.32; y: parent.height + 3; width: parent.width * 0.36; height: 3; color: Appearance.accent("#FFFA00"); visible: tabButton.selected }
                Text {
                    anchors.centerIn: parent; text: tabButton.modelData; color: tabButton.selected ? Appearance.tone("#111111") : pointer.containsMouse ? "white" : Appearance.tone("#D8D8D8")
                    font.family: tabButton.selected ? harmonyBold.name : harmonyMedium.name; font.pixelSize: 13; font.letterSpacing: 0.52
                    font.weight: tabButton.selected ? Font.Bold : Font.Medium; renderType: Text.CurveRendering
                }
                MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: activityPage.navigate(tabButton.index) }
            }
        }
    }
    Repeater {
        model: 5
        NowPlayingSection {
            required property int index
            page: activityPage; sectionIndex: index + 1; y: 125 + index * 70; width: 590; height: 64; visible: activityPage.tab === 0
            ActivityMetricRow { metric: index; history: activityPage.activity.history; latest: activityPage.activity.latest; reduceMotion: activityPage.reduceMotion; objectName: "metric" + index }
        }
    }
    NowPlayingSection {
        page: activityPage; sectionIndex: 1; y: 125; width: 590; height: 26; visible: activityPage.tab === 1
        Repeater {
            model: activityPage.columns
            Item {
                required property var modelData
                x: modelData.x; width: modelData.w; height: 26
                Text {
                    x: 0; y: 8; width: parent.width; height: 10
                    text: modelData.title + (activityPage.activity.sortKey === modelData.key ? (activityPage.activity.descending ? " ▼" : " ▲") : "")
                    horizontalAlignment: modelData.key === "name" ? Text.AlignLeft : Text.AlignRight
                    color: activityPage.activity.sortKey === modelData.key ? Appearance.accent("#FFFA00") : Appearance.tone("#8A8A8A")
                    font.family: harmonyRegular.name; font.pixelSize: 10; font.letterSpacing: 0.8; renderType: Text.CurveRendering
                }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: activityPage.sort(modelData.key) }
            }
        }
    }
    Item {
        id: table; objectName: "activityTable"; y: 155; width: 590; height: 330; visible: activityPage.tab === 1; clip: true
        Repeater {
            model: 9 // Source poolSize: ceil(330 / 46) + 1.
            NowPlayingSection {
                id: appRow
                required property int index
                readonly property int itemIndex: activityPage.firstVisible + index
                readonly property var entry: activityPage.apps[itemIndex] || { name: "", icon: "", gpu: null, cpu: null, memory: null, threads: null }
                property real sortProgress: 1
                page: activityPage; sectionIndex: index + 2; width: 584; height: 40
                y: itemIndex * 46 - activityPage.appOffset; visible: itemIndex < activityPage.apps.length
                Connections {
                    target: activityPage
                    function onSortRevisionChanged() { reorder.stop(); if (appRow.visible && !activityPage.reduceMotion) reorder.start(); else appRow.sortProgress = 1; }
                    function onPresentedChanged() { reorder.stop(); appRow.sortProgress = 1; }
                    function onReduceMotionChanged() { if (activityPage.reduceMotion) { reorder.stop(); appRow.sortProgress = 1; } }
                }
                SequentialAnimation {
                    id: reorder
                    PropertyAction { target: appRow; property: "sortProgress"; value: 0 }
                    PauseAnimation { duration: 60 + appRow.index * 36 }
                    NumberAnimation { target: appRow; property: "sortProgress"; from: 0; to: 1; duration: 380; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] }
                }
                Item {
                    anchors.fill: parent; opacity: Math.min(1, appRow.sortProgress / 0.6)
                    transform: Translate { y: 12 * (1 - appRow.sourceEase(appRow.sortProgress)) }
                    VolumeRow {
                        width: 584; height: 40; reduceMotion: activityPage.reduceMotion
                        Rectangle {
                            x: 18; y: 11; width: 20; height: 20; color: Appearance.tone("#1B1B1B"); visible: !appRow.entry.icon || appIcon.status === Image.Error
                            Shape {
                                x: 4; y: 4; width: 12; height: 12; preferredRendererType: Shape.CurveRenderer
                                ShapePath { strokeWidth: -1; fillColor: Appearance.accent("#FFFA00"); PathSvg { path: "M1.5 1.5H5V5H1.5ZM7 1.5h3.5V5H7ZM1.5 7H5v3.5H1.5Z" } }
                                ShapePath { strokeWidth: 1.2; strokeColor: Appearance.accent("#FFFA00"); fillColor: "transparent"; PathSvg { path: "M7 7h3.5v3.5H7Z" } }
                            }
                        }
                        Image {
                            id: appIcon; x: 18; y: 11; width: 20; height: 20
                            source: appRow.entry.icon ? "image://activity-icons/" + encodeURIComponent(appRow.entry.icon) : ""
                            sourceSize: Qt.size(64, 64); mipmap: true; fillMode: Image.PreserveAspectFit
                        }
                        Text {
                            x: 46; y: 14.25; width: 246; height: 13.5; text: appRow.entry.name; elide: Text.ElideRight
                            color: Appearance.tone("#121212"); font.family: harmonyMedium.name; font.pointSize: 13.5 * 0.75; font.weight: Font.Medium; renderType: Text.CurveRendering
                        }
                        Repeater {
                            model: activityPage.columns.slice(1)
                            Text {
                                required property var modelData
                                x: modelData.x; y: 15.25; width: modelData.w; height: 11.5; horizontalAlignment: Text.AlignRight
                                text: modelData.key === "gpu" || modelData.key === "cpu" ? Format.percent(appRow.entry[modelData.key], false)
                                    : modelData.key === "memory" ? Format.memory(appRow.entry.memory) : Format.valid(appRow.entry.threads) ? String(appRow.entry.threads) : "—"
                                color: activityPage.activity.sortKey === modelData.key ? Appearance.tone("#111111") : Appearance.tone("#555555")
                                font.family: harmonyBold.name; font.pointSize: 11.5 * 0.75; font.weight: Font.Bold; font.features: { "tnum": 1 }; renderType: Text.CurveRendering
                            }
                        }
                    }
                }
            }
        }
        Item {
            width: 590; height: 250; visible: activityPage.activity.appsReady && activityPage.apps.length === 0; clip: true
            Rectangle { anchors.fill: parent; color: "transparent"; border.color: Appearance.tone("#3A3A3A") }
            Shape {
                anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
                ShapePath { strokeWidth: -1; fillColor: Qt.rgba(1, 1, 1, 0.14); PathSvg { path: { let p = ""; for (let x = -250; x < 590; x += 3.5 * Math.SQRT2) p += "M" + x + " 250l250 -250h1.414l-250 250Z"; return p; } } }
            }
            Text {
                y: 104.25; width: 590; height: 20; horizontalAlignment: Text.AlignHCenter
                text: "<font color='#FFFA00'>[ </font>NO APP READINGS<font color='#FFFA00'> ]</font>"; textFormat: Text.RichText; color: Appearance.tone("#E9E9E9")
                font.family: harmonyBold.name; font.pixelSize: 20; font.weight: Font.Bold; font.letterSpacing: 4.8; renderType: Text.CurveRendering
            }
            Text { y: 134.25; width: 590; height: 11.5; horizontalAlignment: Text.AlignHCenter; text: qsTr("暂无应用读数"); color: Appearance.tone("#8A8A8A"); font.family: harmonyRegular.name; font.pointSize: 11.5 * 0.75; font.letterSpacing: 1.38; renderType: Text.CurveRendering }
        }
        Item {
            z: 1; anchors.fill: parent
            MouseArea {
                anchors.fill: parent
                property real pressedY: 0
                property real pressedOffset: 0
                onPressed: mouse => { glide.stop(); pressedY = mouse.y; pressedOffset = activityPage.appOffset }
                onPositionChanged: mouse => {
                    if (pressed) activityPage.scroll(pressedOffset - (mouse.y - pressedY) - activityPage.appOffset, false)
                }
                onWheel: wheel => {
                    activityPage.scroll(-(wheel.pixelDelta.y || wheel.angleDelta.y / 3), wheel.pixelDelta.y === 0)
                    wheel.accepted = true
                }
            }
        }
        Item {
            id: scrollStrip; objectName: "activityScrollStrip"; z: 2; x: 578; width: 12; height: 330; visible: activityPage.maxAppOffset > 0
            readonly property real thumbHeight: Math.max(18, height * height / Math.max(1, activityPage.apps.length * 46 - 6))
            readonly property real thumbTravel: height - thumbHeight
            function moveThumb(y) {
                glide.stop()
                activityPage.appOffset = thumbTravel > 0 ? Math.max(0, Math.min(thumbTravel, y)) / thumbTravel * activityPage.maxAppOffset : 0
                activityPage.scrollActive = true
                linger.restart()
            }
            Rectangle {
                x: 8; width: 4; height: 330; color: Appearance.tone("#222222")
                opacity: activityPage.scrollActive || scrollPointer.containsMouse || scrollPointer.pressed ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: activityPage.reduceMotion ? 0 : (activityPage.scrollActive || scrollPointer.containsMouse || scrollPointer.pressed ? 150 : 300); easing.type: Easing.BezierSpline; easing.bezierCurve: activityPage.scrollActive || scrollPointer.containsMouse || scrollPointer.pressed ? [0, 0, 0.58, 1, 1, 1] : [0.42, 0, 1, 1, 1, 1] } }
                Rectangle {
                    objectName: "activityScrollThumb"; width: 4; height: scrollStrip.thumbHeight; color: Appearance.accent("#FFFA00")
                    y: activityPage.maxAppOffset > 0 ? activityPage.appOffset / activityPage.maxAppOffset * scrollStrip.thumbTravel : 0
                }
            }
            MouseArea {
                id: scrollPointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.SizeVerCursor
                property real grabY: 0
                onContainsMouseChanged: {
                    activityPage.scrollActive = true
                    if (containsMouse) linger.stop(); else linger.restart()
                }
                onPressed: mouse => {
                    const thumbY = activityPage.maxAppOffset > 0 ? activityPage.appOffset / activityPage.maxAppOffset * scrollStrip.thumbTravel : 0
                    grabY = mouse.y >= thumbY && mouse.y <= thumbY + scrollStrip.thumbHeight ? mouse.y - thumbY : scrollStrip.thumbHeight / 2
                    scrollStrip.moveThumb(mouse.y - grabY)
                }
                onPositionChanged: mouse => { if (pressed) scrollStrip.moveThumb(mouse.y - grabY) }
                onReleased: linger.restart()
                onCanceled: linger.restart()
            }
        }
    }
}
