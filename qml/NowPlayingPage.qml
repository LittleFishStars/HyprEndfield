import QtQuick
import QtQuick.Shapes
import QtQuick.Effects

// Colours, paths and motion are ported from EndfieldHUD/NowPlayingCanvas.swift.
// User-approved compact layout: 400 px cover; controls moved inside the dial.
Item {
    id: playerPage
    width: 590; height: 510
    property bool presented: false
    property bool reduceMotion: false
    property var viewState: nowPlayingController.state
    property string motionPhase: "idle"
    property int direction: 1
    property int pendingDirection: 1
    property double directionDeadline: 0
    property bool volumeOpen: false
    property bool showsLyrics: true
    property bool dragging: false
    property real preview: 0
    property string gestureTrack: ""
    property real elapsed: -1
    readonly property real displaySeconds: dragging ? preview : elapsed
    readonly property real fraction: viewState.duration > 0 ? Math.max(0, Math.min(1, displaySeconds / viewState.duration)) : 0
    readonly property var lyricLines: splayerLyrics.lines
    readonly property int lyricIndex: currentLyric(displaySeconds)
    property int previousLyricIndex: -1
    property bool slideLyrics: false

    function time(seconds) {
        if (!Number.isFinite(seconds) || seconds < 0) return "—:—";
        const value = Math.floor(seconds);
        return Math.floor(value / 60) + ":" + String(value % 60).padStart(2, "0");
    }
    function currentLyric(seconds) {
        let lo = 0, hi = lyricLines.length;
        while (lo < hi) {
            const mid = Math.floor((lo + hi) / 2);
            if (lyricLines[mid].time <= seconds) lo = mid + 1;
            else hi = mid;
        }
        return lo - 1;
    }
    function refreshProgress() {
        progressRun.stop();
        if (!presented || dragging) return;
        elapsed = nowPlayingController.position;
        if (viewState.playing && elapsed >= 0 && viewState.duration > 0 && !reduceMotion) {
            progressRun.from = elapsed;
            progressRun.to = Math.min(viewState.duration, elapsed + viewState.rate);
            progressRun.start();
        }
    }
    function updateState() {
        const next = nowPlayingController.state;
        if (next.trackKey !== viewState.trackKey) {
            dragging = false;
            volumeOpen = false;
            if (presented && !reduceMotion && viewState.hasTrack && next.hasTrack) {
                if (motionPhase !== "leave") {
                    settle.stop();
                    direction = Date.now() < directionDeadline ? pendingDirection : 1;
                    directionDeadline = 0;
                    motionPhase = "leave";
                    swap.restart();
                }
                return;
            }
        }
        if (motionPhase !== "leave") viewState = next;
        refreshProgress();
    }
    function cancelMotion() {
        swap.stop(); settle.stop(); progressRun.stop();
        motionPhase = "idle"; dragging = false; volumeOpen = false;
    }
    onPresentedChanged: {
        cancelMotion();
        nowPlayingController.active = presented;
        viewState = nowPlayingController.state;
        refreshProgress();
    }
    onLyricIndexChanged: {
        slideLyrics = !reduceMotion && previousLyricIndex >= 0 && Math.abs(lyricIndex - previousLyricIndex) <= 2;
        previousLyricIndex = lyricIndex;
    }
    onLyricLinesChanged: { previousLyricIndex = -1; slideLyrics = false; }
    onReduceMotionChanged: if (reduceMotion) { cancelMotion(); viewState = nowPlayingController.state; refreshProgress(); }
    Component.onDestruction: nowPlayingController.active = false
    Connections {
        target: nowPlayingController
        function onStateChanged() { playerPage.updateState(); }
        function onPositionChanged() { playerPage.refreshProgress(); }
    }
    Timer {
        id: swap
        interval: 130
        onTriggered: {
            playerPage.viewState = nowPlayingController.state;
            playerPage.motionPhase = "enter";
            playerPage.refreshProgress();
            settle.restart();
        }
    }
    Timer { id: settle; interval: 584; onTriggered: playerPage.motionPhase = "idle" }
    NumberAnimation { id: progressRun; target: playerPage; property: "elapsed"; duration: 1000 }
    Shortcut {
        sequence: "Space"
        enabled: playerPage.presented && playerPage.enabled && playerPage.viewState.canToggle
        onActivated: nowPlayingController.playPause()
    }
    Shortcut {
        sequence: "Left"
        enabled: playerPage.presented && playerPage.enabled && playerPage.viewState.canSeek
        onActivated: nowPlayingController.seek(nowPlayingController.position - 5, playerPage.viewState.trackKey)
    }
    Shortcut {
        sequence: "Right"
        enabled: playerPage.presented && playerPage.enabled && playerPage.viewState.canSeek
        onActivated: nowPlayingController.seek(nowPlayingController.position + 5, playerPage.viewState.trackKey)
    }

    NowPlayingSection {
        page: playerPage
        sectionIndex: 0
        x: 95; y: -44; width: 400; height: 400
        Rectangle {
            id: cover
            anchors.fill: parent
            color: "#151515"
            clip: true
            property url frontArt: ""
            property url backArt: ""
            property real artBlend: 1
            function hatch() {
                let path = "";
                const step = 3.5 * Math.SQRT2, thick = Math.SQRT2;
                for (let x = -400; x < 400; x += step)
                    path += "M" + x + " 400L" + (x + thick) + " 400L" + (x + thick + 400) + " 0L" + (x + 400) + " 0Z";
                return path;
            }
            Shape {
                visible: !playerPage.viewState.hasTrack
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeWidth: -1
                    fillColor: Qt.rgba(1, 1, 1, 0.14)
                    PathSvg { path: cover.hatch() }
                }
            }
            Image {
                id: artLoader
                source: playerPage.viewState.artUrl
                asynchronous: true
                visible: false
                onStatusChanged: {
                    if (status === Image.Ready) {
                        cover.backArt = cover.frontArt;
                        cover.frontArt = source;
                        artFade.restart();
                    } else if (status === Image.Null || status === Image.Error) {
                        cover.frontArt = ""; cover.backArt = "";
                    }
                }
            }
            Image {
                anchors.fill: parent
                source: cover.backArt
                fillMode: Image.PreserveAspectCrop
                mipmap: true
                opacity: 1 - cover.artBlend
            }
            Image {
                anchors.fill: parent
                source: cover.frontArt
                fillMode: Image.PreserveAspectCrop
                mipmap: true
                opacity: cover.artBlend
            }
            NumberAnimation {
                id: artFade
                target: cover; property: "artBlend"; from: 0; to: 1
                duration: playerPage.reduceMotion ? 0 : 200
                easing.type: Easing.BezierSpline
                easing.bezierCurve: [0.42, 0, 0.58, 1, 1, 1]
                onFinished: cover.backArt = ""
            }
            Rectangle {
                visible: cover.frontArt.toString() === ""
                x: 164; y: playerPage.viewState.hasTrack ? 164 : 152
                width: 72; height: 72; radius: 36
                color: "transparent"
                border.width: 2
                border.color: Qt.rgba(0.6, 0.6, 0.6, 0.4)
            }
            Text {
                visible: !playerPage.viewState.hasTrack
                x: 0; y: 238; width: 400; height: 10
                text: "未在播放音乐"
                color: "#8A8A8A"
                font.family: harmonyRegular.name
                font.pixelSize: 10
                font.letterSpacing: 1
                renderType: Text.CurveRendering
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            Rectangle {
                visible: playerPage.viewState.hasTrack
                x: 0; y: 284; width: 400; height: 116
                gradient: Gradient {
                    GradientStop { position: 0; color: "#00000000" }
                    GradientStop { position: 0.3; color: "#C2000000" }
                    GradientStop { position: 1; color: "#E6000000" }
                }
            }
            Text {
                visible: playerPage.viewState.hasTrack
                x: 20; y: 336.8; width: 360; height: 25.2
                text: playerPage.viewState.title
                textFormat: Text.PlainText
                color: "white"
                font.family: harmonyBold.name
                font.pixelSize: 21
                font.weight: Font.Bold
                renderType: Text.CurveRendering
                elide: Text.ElideRight
            }
            Text {
                visible: playerPage.viewState.hasTrack
                x: 20; y: 367.2; width: 360; height: 16.8
                text: playerPage.viewState.artist
                textFormat: Text.PlainText
                color: "#CCFFFFFF"
                font.family: harmonyRegular.name
                font.pixelSize: 14
                renderType: Text.CurveRendering
                elide: Text.ElideRight
            }
        }
    }

    NowPlayingSection {
        page: playerPage
        sectionIndex: 1
        x: 95; y: 360; width: 400; height: 60
        Item {
            anchors.fill: parent
            clip: true
            opacity: playerPage.showsLyrics && playerPage.viewState.hasTrack && playerPage.lyricLines.length > 0 ? 1 : 0
            Behavior on opacity {
                NumberAnimation {
                    duration: playerPage.reduceMotion ? 0 : 240
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1]
                }
            }
            Repeater {
                model: playerPage.lyricLines
                Text {
                    required property int index
                    required property var modelData
                    readonly property int distance: Math.abs(index - playerPage.lyricIndex)
                    x: 0; y: (index - playerPage.lyricIndex + 1) * 20
                    width: 400; height: 20
                    visible: distance <= 2
                    opacity: distance === 0 ? 1 : distance === 1 ? 0.36 : 0
                    text: modelData.text
                    textFormat: Text.PlainText
                    color: "#F0F0F0"
                    font.family: distance === 0 ? harmonyMedium.name : harmonyRegular.name
                    font.pointSize: (distance === 0 ? 14 : 12.5) * 0.75
                    renderType: Text.CurveRendering
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    Behavior on y { enabled: playerPage.slideLyrics; NumberAnimation { duration: 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.42, 0, 0.58, 1, 1, 1] } }
                    Behavior on opacity { enabled: playerPage.slideLyrics; NumberAnimation { duration: 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.42, 0, 0.58, 1, 1, 1] } }
                    Behavior on font.pointSize { enabled: playerPage.slideLyrics; NumberAnimation { duration: 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.42, 0, 0.58, 1, 1, 1] } }
                }
            }
        }
    }

    NowPlayingSection {
        page: playerPage
        sectionIndex: 2
        x: 95; y: 424; width: 400; height: 34
        Rectangle { x: 0; y: 8; width: 400; height: 3; color: "#3A3A3A" }
        Rectangle {
            x: 0; y: 8; width: playerPage.fraction * 400; height: 3
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "#E9CF12" }
                GradientStop { position: 1; color: "#FFFA00" }
            }
        }
        Rectangle {
            x: playerPage.fraction * 400 - 4.5; y: 3; width: 9; height: 13; color: "#EDEDED"
            Rectangle { x: 3.5; y: 3; width: 2; height: 7; color: "#111111" }
        }
        Text {
            x: 0; y: 20; width: 200; height: 12.6
            text: playerPage.time(playerPage.displaySeconds)
            color: "#999999"
            font.family: harmonyMedium.name
            font.pointSize: 7.875
            font.features: { "tnum": 1 }
            renderType: Text.CurveRendering
        }
        Text {
            x: 200; y: 20; width: 200; height: 12.6
            text: playerPage.time(playerPage.viewState.duration > 0 ? playerPage.viewState.duration : -1)
            color: "#999999"
            font.family: harmonyMedium.name
            font.pointSize: 7.875
            font.features: { "tnum": 1 }
            renderType: Text.CurveRendering
            horizontalAlignment: Text.AlignRight
        }
        MouseArea {
            x: -5; y: -3; width: 410; height: 28
            enabled: playerPage.viewState.canSeek && playerPage.motionPhase !== "leave"
            cursorShape: Qt.PointingHandCursor
            function scrub(mouseX) { playerPage.preview = Math.max(0, Math.min(1, (mouseX - 5) / 400)) * playerPage.viewState.duration; }
            onPressed: mouse => {
                progressRun.stop(); playerPage.gestureTrack = playerPage.viewState.trackKey;
                scrub(mouse.x); playerPage.dragging = true;
            }
            onPositionChanged: mouse => { if (pressed && playerPage.dragging) scrub(mouse.x); }
            onReleased: {
                if (playerPage.dragging) nowPlayingController.seek(playerPage.preview, playerPage.gestureTrack);
                playerPage.dragging = false; playerPage.refreshProgress();
            }
            onCanceled: { playerPage.dragging = false; playerPage.refreshProgress(); }
        }
    }

    NowPlayingSection {
        page: playerPage
        sectionIndex: 3
        x: 95; y: 470; width: 162; height: 54
        NowPlayingButton {
            x: 0; y: 5; kind: "previous"; label: "上一首"
            enabled: playerPage.viewState.canPrevious && playerPage.motionPhase !== "leave"
            reduceMotion: playerPage.reduceMotion
            onTriggered: {
                if (!(playerPage.viewState.canSeek && nowPlayingController.position > 3)) {
                    playerPage.pendingDirection = -1; playerPage.directionDeadline = Date.now() + 5000;
                }
                nowPlayingController.previous();
            }
        }
        NowPlayingButton {
            x: 54; y: 0; kind: playerPage.viewState.playing ? "pause" : "play"
            label: playerPage.viewState.playing ? "暂停" : "播放"
            enabled: playerPage.viewState.canToggle && playerPage.motionPhase !== "leave"
            reduceMotion: playerPage.reduceMotion
            onTriggered: nowPlayingController.playPause()
        }
        NowPlayingButton {
            x: 118; y: 5; kind: "next"; label: "下一首"
            enabled: playerPage.viewState.canNext && playerPage.motionPhase !== "leave"
            reduceMotion: playerPage.reduceMotion
            onTriggered: {
                playerPage.pendingDirection = 1; playerPage.directionDeadline = Date.now() + 5000;
                nowPlayingController.next();
            }
        }
    }
    NowPlayingSection {
        page: playerPage
        sectionIndex: 4
        x: 427; y: 481; width: 68; height: 30
        NowPlayingButton {
            kind: "volume"; label: "SPlayer 应用音量"
            enabled: playerPage.viewState.canVolume && playerPage.motionPhase !== "leave"
            selected: playerPage.volumeOpen
            reduceMotion: playerPage.reduceMotion
            onTriggered: playerPage.volumeOpen = !playerPage.volumeOpen
        }
        NowPlayingButton {
            x: 38; kind: "lyrics"; label: "歌词"
            enabled: playerPage.viewState.hasTrack
            selected: playerPage.showsLyrics
            reduceMotion: playerPage.reduceMotion
            onTriggered: playerPage.showsLyrics = !playerPage.showsLyrics
        }
    }

    MouseArea {
        x: -1000; y: -1000; width: 2600; height: 2600; z: 4
        visible: playerPage.volumeOpen
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: playerPage.volumeOpen = false
    }
    Item {
        id: volumePopup
        x: 417; y: 318; width: 44; height: 150; z: 5
        visible: playerPage.volumeOpen
        property real reveal: 1
        property real preview: playerPage.viewState.volume
        opacity: 0.4 + reveal * 0.6
        onVisibleChanged: if (visible) popupWipe.restart()
        NumberAnimation {
            id: popupWipe
            target: volumePopup; property: "reveal"; from: 0; to: 1
            duration: playerPage.reduceMotion ? 0 : 180
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1]
        }
        Rectangle {
            anchors.fill: parent
            visible: volumePopup.reveal >= 1
            color: "#1A1A1A"
            layer.enabled: playerPage.volumeOpen
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.6)
                shadowVerticalOffset: 18
                shadowBlur: 1
                blurMax: 20
            }
        }
        Item {
            width: 44; height: 150 * volumePopup.reveal; clip: true
            Rectangle {
                width: 44; height: 150
                color: "#1A1A1A"
                border.width: 1; border.color: "#3D3D3D"
                Text {
                    x: 0; y: 9; width: 44; height: 10
                    text: Math.round((volumePointer.pressed ? volumePopup.preview : playerPage.viewState.volume) * 100) + "%"
                    font.family: harmonyBold.name
                    font.pixelSize: 10
                    font.weight: Font.Bold
                    color: "#EEEEEE"
                    horizontalAlignment: Text.AlignHCenter
                    renderType: Text.CurveRendering
                }
                Rectangle { x: 19; y: 30; width: 6; height: 106; color: "#333333" }
                Rectangle {
                    readonly property real fraction: volumePointer.pressed ? volumePopup.preview : playerPage.viewState.volume
                    x: 19; y: 136 - fraction * 106; width: 6; height: fraction * 106
                    color: "#FFFA00"
                }
                Rectangle {
                    x: 14; y: 133 - (volumePointer.pressed ? volumePopup.preview : playerPage.viewState.volume) * 106
                    width: 16; height: 6; color: "#EEEEEE"
                }
                MouseArea {
                    id: volumePointer
                    anchors.fill: parent
                    property string gestureTrack: ""
                    cursorShape: Qt.PointingHandCursor
                    function adjust(mouseY) {
                        volumePopup.preview = Math.round(Math.max(0, Math.min(1, 1 - (mouseY - 30) / 106)) * 100) / 100;
                        nowPlayingController.setVolume(volumePopup.preview, gestureTrack);
                    }
                    onPressed: mouse => { gestureTrack = playerPage.viewState.trackKey; adjust(mouse.y); }
                    onPositionChanged: mouse => { if (pressed) adjust(mouse.y); }
                    onReleased: mouse => adjust(mouse.y)
                }
            }
        }
    }
    Item {
        id: seekBubble
        z: 40
        x: 95 + playerPage.fraction * 400 - width / 2; y: 399
        width: bubbleText.implicitWidth + 16; height: 25
        opacity: playerPage.dragging ? 1 : 0
        scale: playerPage.dragging ? 1 : 0.9
        transformOrigin: Item.Bottom
        Behavior on opacity { NumberAnimation { duration: playerPage.reduceMotion ? 0 : 140; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1] } }
        Behavior on scale { NumberAnimation { duration: playerPage.reduceMotion ? 0 : 180; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.9, 0.1, 1, 1, 1] } }
        Rectangle { y: 2; width: parent.width; height: 21; radius: 10; color: "#111111" }
        Rectangle {
            width: parent.width; height: 21; radius: 10; color: "#FAFAFA"
            layer.enabled: playerPage.dragging
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.45)
                shadowVerticalOffset: 6
                shadowBlur: 1
                blurMax: 8
            }
        }
        Shape {
            x: seekBubble.width / 2 - 4; y: 21
            preferredRendererType: Shape.CurveRenderer
            ShapePath { fillColor: "#FAFAFA"; strokeWidth: -1; PathSvg { path: "M0 0L8 0L4 4Z" } }
        }
        Text {
            id: bubbleText
            anchors.horizontalCenter: parent.horizontalCenter
            y: 5; height: 11
            text: playerPage.time(playerPage.preview)
            font.family: harmonyBold.name
            font.pixelSize: 11
            font.weight: Font.Bold
            color: "#111111"
            renderType: Text.CurveRendering
        }
    }
}
