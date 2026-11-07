import QtQuick
import "Appearance.js" as Appearance
import QtQuick.Shapes

// EndfieldHUD WorkModeCanvas and WorkModeController, adapted to Focus Mode.
Item {
    id: focusPage
    width: 590
    height: 610
    focus: presented
    property bool presented: false
    property bool reduceMotion: false
    readonly property var session: focusModeController.state
    property real dialFraction: 1
    property bool animateDial: false
    property bool editingCustom: false
    property bool customInvalid: false
    property var laps: []
    property int lapCount: 0
    property string previousPhase: "idle"
    readonly property int litTicks: Math.max(0, Math.min(120, Math.ceil(dialFraction * 120 - 0.000001)))
    readonly property bool sessionLocked: session.sessionActive

    function formatTime(seconds) {
        const value = Math.max(0, Math.floor(seconds));
        if (value >= 3600)
            return Math.floor(value / 3600) + ":" + String(Math.floor(value / 60) % 60).padStart(2, "0") + ":" + String(value % 60).padStart(2, "0");
        return String(Math.floor(value / 60)).padStart(2, "0") + ":" + String(value % 60).padStart(2, "0");
    }
    function durationTitle(seconds) {
        return seconds % 60 === 0 ? Math.floor(seconds / 60) + qsTr(" 分") : formatTime(seconds);
    }
    function syncDial() {
        const next = Number(session.dialFraction || 0);
        animateDial = !reduceMotion && next >= dialFraction - 0.5;
        dialFraction = next;
    }
    function chooseKind(index) {
        if (sessionLocked) return;
        if (index === 0 && session.kind !== "countdown") focusModeController.chooseCountdown(session.duration || 1800);
        else if (index === 1 && session.kind !== "stopwatch") focusModeController.chooseStopwatch();
        laps = []; lapCount = 0; editingCustom = false; customInvalid = false;
        rootFocus.forceActiveFocus();
    }
    function choosePreset(index) {
        if (index < 3) {
            focusModeController.chooseCountdown([300, 1800, 3600][index]);
            editingCustom = false; customInvalid = false;
            rootFocus.forceActiveFocus();
        } else openCustom();
    }
    function openCustom() {
        if (session.kind !== "countdown") return;
        const seconds = Number(session.duration || 1800);
        customInput.text = seconds % 60 === 0 ? String(seconds / 60) : focusModeController.durationEditText();
        editingCustom = true; customInvalid = false;
        Qt.callLater(() => { customInput.forceActiveFocus(); customInput.selectAll(); });
    }
    function closeCustom() {
        editingCustom = false; customInvalid = false; rootFocus.forceActiveFocus();
    }
    function commitCustom() {
        if (focusModeController.chooseCustomDuration(customInput.text)) closeCustom();
        else { customInvalid = true; invalidShake.restart(); }
    }
    function primaryAction() {
        if (session.phase === "running") focusModeController.pause();
        else if (session.phase === "paused") focusModeController.resume();
        else {
            if (session.kind === "stopwatch") { laps = []; lapCount = 0; }
            focusModeController.start();
        }
        rootFocus.forceActiveFocus();
    }
    function secondaryAction() {
        if (session.kind === "stopwatch" && session.phase === "running") {
            lapCount += 1;
            laps = [{ number: lapCount, seconds: session.displayedSeconds }].concat(laps).slice(0, 3);
        } else if (session.sessionActive) {
            focusModeController.stop();
        } else {
            focusModeController.reset();
            laps = []; lapCount = 0;
        }
        rootFocus.forceActiveFocus();
    }

    onPresentedChanged: {
        focusModeController.active = presented;
        if (presented) { focusModeController.refresh(); rootFocus.forceActiveFocus(); syncDial(); }
        else editingCustom = false;
    }
    onReduceMotionChanged: syncDial()
    Component.onCompleted: { previousPhase = session.phase; syncDial(); }
    Component.onDestruction: focusModeController.active = false

    Connections {
        target: focusModeController
        function onStateChanged() {
            focusPage.syncDial();
            focusPage.previousPhase = focusPage.session.phase;
        }
    }

    Item { id: rootFocus; anchors.fill: parent; focus: true }
    Shortcut { sequence: "Space"; enabled: focusPage.presented && !focusPage.editingCustom; onActivated: focusPage.primaryAction() }
    Shortcut { sequence: "Return"; enabled: focusPage.presented && !focusPage.editingCustom; onActivated: focusPage.primaryAction() }
    Shortcut { sequence: "Q"; enabled: focusPage.presented && !focusPage.editingCustom && !focusPage.sessionLocked; onActivated: focusPage.chooseKind(0) }
    Shortcut { sequence: "E"; enabled: focusPage.presented && !focusPage.editingCustom && !focusPage.sessionLocked; onActivated: focusPage.chooseKind(1) }
    Shortcut { sequence: "S"; enabled: focusPage.presented && focusPage.session.sessionActive && !focusPage.editingCustom; onActivated: focusModeController.stop() }
    Shortcut { sequence: "Escape"; enabled: focusPage.presented && focusPage.editingCustom; onActivated: focusPage.closeCustom() }

    Item {
        id: dial
        x: -15; y: -5; width: 620; height: 620

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: "transparent"; strokeColor: "#2EFFFFFF"; strokeWidth: 1
                PathAngleArc { centerX: 310; centerY: 310; radiusX: 304; radiusY: 304; startAngle: 0; sweepAngle: 360 }
            }
            ShapePath {
                fillColor: "transparent"; strokeColor: Appearance.tone("#2A2A2A"); strokeWidth: 10
                capStyle: ShapePath.FlatCap
                PathAngleArc { centerX: 310; centerY: 310; radiusX: 252; radiusY: 252; startAngle: -90; sweepAngle: 360 }
            }
            ShapePath {
                fillColor: "transparent"; strokeColor: Appearance.accent("#FFFA00"); strokeWidth: 10
                capStyle: ShapePath.FlatCap
                PathAngleArc { centerX: 310; centerY: 310; radiusX: 252; radiusY: 252; startAngle: -90; sweepAngle: 360 * focusPage.dialFraction }
            }
            ShapePath {
                fillColor: "transparent"; strokeColor: "#1FFFFFFF"; strokeWidth: 1
                dashPattern: [2, 7]
                PathAngleArc { centerX: 310; centerY: 310; radiusX: 232; radiusY: 232; startAngle: 0; sweepAngle: 360 }
            }
        }

        Repeater {
            model: 120
            delegate: Rectangle {
                required property int index
                readonly property bool major: index % 10 === 0
                x: 310 - width / 2; y: 18
                width: major ? 4 : 2.4; height: major ? 24 : 14
                color: index < focusPage.litTicks ? Appearance.accent("#FFFA00") : Appearance.tone("#3D3D3D")
                antialiasing: true
                transform: Rotation { origin.x: width / 2; origin.y: 292; angle: index * 3 }
                Behavior on color { ColorAnimation { duration: focusPage.reduceMotion ? 0 : 160 } }
            }
        }

        Repeater {
            model: ["00", "15", "30", "45"]
            delegate: Text {
                required property string modelData
                required property int index
                readonly property real angle: index * Math.PI / 2 - Math.PI / 2
                x: 310 + Math.cos(angle) * 322 - 15
                y: 310 + Math.sin(angle) * 322 - 7
                width: 30; height: 14
                text: modelData; color: Appearance.tone("#8A8A8A")
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering
            }
        }

        Item {
            anchors.fill: parent
            rotation: focusPage.dialFraction * 360
            transformOrigin: Item.Center
            Behavior on rotation {
                enabled: focusPage.animateDial
                NumberAnimation { duration: 600; easing.type: Easing.InOutCubic }
            }
            Shape {
                anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
                ShapePath { strokeWidth: -1; fillColor: Appearance.accent("#FFFA00"); PathSvg { path: "M310 41L319 56L301 56Z" } }
            }
        }
        Shape {
            anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
            ShapePath { strokeWidth: -1; fillColor: Appearance.accent("#FFFA00"); PathSvg { path: "M310 4L317 14L303 14Z" } }
        }
    }

    Row {
        id: titleGroup
        y: 118; anchors.horizontalCenter: parent.horizontalCenter; spacing: 12
        Text {
            id: titleChinese
            text: qsTr("专注模式"); color: Appearance.tone("#F4F4F4")
            font.family: harmonyBold.name; font.pixelSize: 20; font.weight: Font.Bold; font.letterSpacing: 1.2
            renderType: Text.CurveRendering
        }
        Text {
            anchors.baseline: titleChinese.baseline
            text: "FOCUS MODE"; color: Appearance.tone("#8A8A8A")
            font.family: harmonyMedium.name; font.pixelSize: 10; font.weight: Font.Medium; font.letterSpacing: 1.8
            renderType: Text.CurveRendering
        }
    }

    Item {
        id: kindTabs
        x: 160; y: 150; width: 270; height: 30
        Rectangle { anchors.fill: parent; color: Appearance.tone("#191919"); border.color: Appearance.tone("#434343") }
        Repeater {
            model: [qsTr("倒计时"), qsTr("正计时")]
            delegate: Item {
                id: tab
                required property string modelData
                required property int index
                readonly property bool selected: focusPage.session.kind === (index === 0 ? "countdown" : "stopwatch")
                x: index * 135; width: 135; height: 30
                opacity: focusPage.sessionLocked && !selected ? 0.35 : 1
                Rectangle { anchors.fill: parent; color: tab.selected ? Appearance.accent("#FFFA00") : tabHover.hovered ? Appearance.tone("#2A2A2A") : "transparent" }
                Text {
                    anchors.centerIn: parent; text: tab.modelData
                    color: tab.selected ? Appearance.tone("#171717") : Appearance.tone("#B6B6B6")
                    font.family: tab.selected ? harmonyBold.name : harmonyMedium.name
                    font.pixelSize: 12; font.weight: tab.selected ? Font.Bold : Font.Medium; renderType: Text.CurveRendering
                }
                HoverHandler { id: tabHover; cursorShape: focusPage.sessionLocked && !tab.selected ? Qt.ArrowCursor : Qt.PointingHandCursor }
                TapHandler { enabled: !focusPage.sessionLocked && !tab.selected; onTapped: focusPage.chooseKind(tab.index) }
            }
        }
        Rectangle { x: 134.5; y: 8; width: 1; height: 14; color: Appearance.tone("#4A4A4A"); visible: false }
    }

    Row {
        id: stateGroup
        y: 196; anchors.horizontalCenter: parent.horizontalCenter; spacing: 8
        Rectangle {
            id: stateDot
            anchors.verticalCenter: parent.verticalCenter
            width: 7; height: 7
            color: focusPage.session.sessionActive ? "#48D17C" : Appearance.tone("#666666")
            SequentialAnimation on opacity {
                running: focusPage.presented && focusPage.session.running && !focusPage.reduceMotion
                loops: Animation.Infinite
                NumberAnimation { to: 0.2; duration: 800; easing.type: Easing.InOutQuad }
                NumberAnimation { to: 1; duration: 800; easing.type: Easing.InOutQuad }
            }
        }
        Text {
            text: focusPage.session.sessionActive ? qsTr("专注中") : focusPage.session.phase === "completed" ? qsTr("专注结束") : qsTr("待机")
            color: Appearance.tone("#A8A8A8"); font.family: harmonyMedium.name; font.pixelSize: 10; font.letterSpacing: 1
            renderType: Text.CurveRendering
        }
    }

    Text {
        id: timerText
        objectName: "focusModeTimer"
        x: 115; y: 218; width: 360; height: 101
        text: focusPage.session.timeText
        color: Appearance.tone("#F6F6F6")
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        font.family: harmonyRegular.name; font.pixelSize: 112; font.weight: Font.Light
        font.letterSpacing: -3.36; font.features: { "tnum": 1 }
        fontSizeMode: Text.Fit; minimumPixelSize: 62; renderType: Text.CurveRendering
    }

    Item {
        id: presetControl
        x: 145; y: 331; width: 300; height: 30
        visible: focusPage.session.kind === "countdown" && !focusPage.editingCustom
        Repeater {
            model: [300, 1800, 3600, -1]
            delegate: Item {
                id: preset
                required property int modelData
                required property int index
                readonly property bool selected: index < 3 ? focusPage.session.duration === modelData
                    : focusPage.session.duration !== 300 && focusPage.session.duration !== 1800 && focusPage.session.duration !== 3600
                x: index * 75; width: 75; height: 30
                Rectangle { anchors.fill: parent; color: preset.selected ? Appearance.accent("#FFFA00") : presetHover.hovered ? Appearance.tone("#303030") : Appearance.tone("#202020"); border.color: Appearance.tone("#444444") }
                Text {
                    anchors.centerIn: parent; text: preset.index < 3 ? focusPage.durationTitle(preset.modelData) : qsTr("自定义")
                    color: preset.selected ? Appearance.tone("#171717") : Appearance.tone("#D2D2D2")
                    font.family: preset.selected ? harmonyBold.name : harmonyMedium.name; font.pixelSize: 11
                    font.weight: preset.selected ? Font.Bold : Font.Medium; renderType: Text.CurveRendering
                }
                HoverHandler { id: presetHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: focusPage.choosePreset(preset.index) }
            }
        }
    }

    Rectangle {
        id: customEditor
        x: 145; y: 331; width: 300; height: 30
        visible: focusPage.session.kind === "countdown" && focusPage.editingCustom
        color: Appearance.tone("#191919"); border.width: 1; border.color: focusPage.customInvalid ? "#E45A5A" : customInput.activeFocus ? Appearance.accent("#FFFA00") : Appearance.tone("#555555")
        transform: Translate { id: customShift }
        TextInput {
            id: customInput
            objectName: "focusModeCustomInput"
            x: 10; width: 280; height: 30
            verticalAlignment: TextInput.AlignVCenter
            color: Appearance.tone("#F2F2F2"); selectionColor: Appearance.accent("#FFFA00"); selectedTextColor: Appearance.tone("#151515")
            font.family: harmonyMedium.name; font.pixelSize: 12; maximumLength: 7
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            onAccepted: focusPage.commitCustom()
            onTextChanged: focusPage.customInvalid = false
        }
        Text {
            anchors.centerIn: parent; visible: customInput.text.length === 0 && !customInput.activeFocus
            text: qsTr("分钟或 分:秒"); color: Appearance.tone("#777777"); font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering
        }
        SequentialAnimation {
            id: invalidShake
            NumberAnimation { target: customShift; property: "x"; to: -5; duration: 45 }
            NumberAnimation { target: customShift; property: "x"; to: 5; duration: 70 }
            NumberAnimation { target: customShift; property: "x"; to: -3; duration: 60 }
            NumberAnimation { target: customShift; property: "x"; to: 0; duration: 45 }
        }
    }

    Column {
        x: 115; y: 331; width: 360; spacing: 0
        visible: focusPage.session.kind === "stopwatch"
        Repeater {
            model: focusPage.laps
            delegate: Text {
                required property var modelData
                width: 360; height: 20
                text: "#" + String(modelData.number).padStart(2, "0") + "   " + focusPage.formatTime(modelData.seconds)
                color: Appearance.tone("#BBBBBB"); horizontalAlignment: Text.AlignHCenter
                font.family: harmonyRegular.name; font.pixelSize: 13; font.features: { "tnum": 1 }; renderType: Text.CurveRendering
            }
        }
    }

    Row {
        id: actions
        y: focusPage.session.kind === "countdown" ? 373 : 385
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 8
        FocusModeButton {
            id: primaryButton
            objectName: "focusModePrimary"
            width: 150; height: 40; kind: "primary"; reduceMotion: focusPage.reduceMotion
            title: focusPage.session.phase === "running" ? qsTr("暂停") : focusPage.session.phase === "paused" ? qsTr("继续") : qsTr("开始")
            icon: focusPage.session.phase === "running" ? "pause" : "play"
            onTriggered: focusPage.primaryAction()
        }
        FocusModeButton {
            id: secondaryButton
            objectName: "focusModeSecondary"
            width: 92; height: 40; kind: "secondary"; reduceMotion: focusPage.reduceMotion
            readonly property bool lapAction: focusPage.session.kind === "stopwatch" && focusPage.session.phase === "running"
            title: lapAction ? qsTr("计次") : focusPage.session.sessionActive ? qsTr("停止") : qsTr("重置")
            icon: lapAction ? "plus" : focusPage.session.sessionActive ? "stop" : "reset"
            enabled: !(focusPage.session.kind === "stopwatch" && focusPage.session.phase === "idle")
            onTriggered: focusPage.secondaryAction()
        }
    }
}
