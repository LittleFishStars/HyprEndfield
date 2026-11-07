import QtQuick
import QtQuick.Shapes
import QtQuick.Window

Item {
    id: shell

    required property var displaySettings
    width: 1000
    height: 640

    property string selectedModule: "system"
    property real rightScrollOffset: 0
    readonly property real maximumRightScrollOffset: 15.5
    property string clockText: "00:00:00"
    property string dateText: ""
    property int clockSeconds: 0
    property quaternion sourceGyroStart: sourceWatchAnimation.neutralGyro
    property quaternion sourceGyroTarget: sourceWatchAnimation.neutralGyro
    property real sourceGyroProgress: 1
    readonly property quaternion sourceGyro: sourceWatchAnimation.interpolateRotation(sourceGyroStart, sourceGyroTarget, sourceGyroProgress)
    readonly property real cameraPitch: 20 + Math.asin(Math.max(-1, Math.min(1, 2 * (sourceGyro.scalar * sourceGyro.x - sourceGyro.y * sourceGyro.z)))) * 180 / Math.PI
    readonly property real cameraYaw: -Math.atan2(2 * (sourceGyro.scalar * sourceGyro.y + sourceGyro.x * sourceGyro.z), 1 - 2 * (sourceGyro.x * sourceGyro.x + sourceGyro.y * sourceGyro.y)) * 180 / Math.PI
    readonly property real rasterScale: Math.max(1, scale * 1000 / 1920 * 1.10) * Screen.devicePixelRatio * 2
    property int animationPhase: 0
    property real animationTime: sourceWatchAnimation.openingDuration
    property real contentAnimationTime: sourceWatchAnimation.openingDuration
    property real sourceAmbientTime: 0
    readonly property bool sourceAmbientEnabled: displaySettings.ambientAnimation && !displaySettings.reduceMotion && !displaySettings.lowPowerVisualMode
    readonly property bool sourceEntranceFinished: animationPhase !== 1 || animationTime >= sourceWatchAnimation.openingDuration
    readonly property real sourceAmbientSampleTime: sourceAmbientEnabled && sourceEntranceFinished ? sourceAmbientTime : -1
    readonly property real contentAdvanceDuration: 0.40
    readonly property real closingLeadDuration: sourceWatchAnimation.contentTransitionDuration
    readonly property real closingDurationScale: 0.80
    readonly property real closingSequenceDuration: closingDurationScale * (closingLeadDuration + sourceWatchAnimation.closingDuration)
    readonly property real sourceAnimationTime: animationPhase === 2 ? Math.max(0, animationTime) : animationTime
    readonly property real centralClosingTime: animationPhase === 2 ? Math.max(0, Math.min(closingLeadDuration, animationTime + closingLeadDuration)) : sourceAnimationTime
    readonly property var animationFrame: sourceWatchAnimation.frame(animationPhase, sourceAnimationTime)
    readonly property var settledFrame: sourceWatchAnimation.frame(0, sourceWatchAnimation.openingDuration)
    readonly property var baseContentAnimationFrame: animationPhase === 1 ? sourceWatchAnimation.frame(animationPhase, contentAnimationTime) : animationPhase === 2 ? sourceWatchAnimation.frame(animationPhase, centralClosingTime) : animationFrame
    readonly property var contentAnimationFrame: animationPhase === 1 ? sourceWatchAnimation.frame(animationPhase, Math.min(sourceWatchAnimation.openingDuration, contentAnimationTime + contentAdvanceDuration)) : animationFrame
    readonly property var navigationAnimationFrame: animationPhase === 1 ? sourceWatchAnimation.frame(animationPhase, Math.min(sourceWatchAnimation.openingDuration, contentAnimationTime + contentAdvanceDuration)) : animationFrame
    readonly property real backdropOpacity: frameValue("backdropOpacity", 1)
    readonly property bool transitioning: sourceClip.running || contentClip.running
    readonly property real contentOverlapDuration: 0.40

    readonly property var leftButtons: [
        {
            key: "system",
            label: "系统"
        },
        {
            key: "display",
            label: "显示"
        },
        {
            key: "hotkeys",
            label: "快捷键"
        },
        {
            key: "about",
            label: "关于"
        }
    ]

    readonly property var rightButtons: [
        {
            key: "notes",
            label: "便笺",
            icon: "qrc:/assets/icons/notes.png"
        },
        {
            key: "fileShelf",
            label: "文件暂存架",
            icon: "qrc:/assets/icons/file-shelf.png"
        },
        {
            key: "clipboard",
            label: "剪贴板",
            icon: "qrc:/assets/icons/clipboard.png"
        },
        {
            key: "archive",
            label: "档案库",
            icon: "qrc:/assets/icons/archive.png"
        },
        {
            key: "nowPlaying",
            label: "当前播放",
            glyph: "nowPlaying"
        },
        {
            key: "volume",
            label: "音量",
            glyph: "volume"
        },
        {
            key: "reader",
            label: "阅读器",
            icon: "qrc:/assets/icons/reader.png"
        },
        {
            key: "workMode",
            label: "工作模式",
            icon: "qrc:/assets/icons/work-mode.png"
        },
        {
            key: "calendar",
            label: "日历",
            icon: "qrc:/assets/icons/calendar.png"
        },
        {
            key: "map",
            label: "地图",
            icon: "qrc:/assets/icons/map.png"
        },
        {
            key: "eventLog",
            label: "事件日志",
            icon: "qrc:/assets/icons/event-log.png"
        },
        {
            key: "addApp",
            label: "+ 添加应用",
            glyph: "addApp"
        }
    ]

    function updateClock() {
        const now = new Date();
        clockText = Qt.formatTime(now, displaySettings.clock24Hour ? "HH:mm:ss" : "hh:mm:ss AP");
        dateText = Qt.formatDate(now, "ddd MMM d");
        clockSeconds = now.getSeconds();
    }

    function scrollRightList(direction) {
        rightScrollOffset = Math.max(0, Math.min(maximumRightScrollOffset, rightScrollOffset + direction * 15.5));
    }

    function frameValue(name, fallbackValue) {
        const value = animationFrame[name];
        return value === undefined ? fallbackValue : value;
    }

    function ringOpacity(name) {
        // The original texture's material alpha is already in the imported SVG.
        // Apply the clip's relative fade, so it is not multiplied in a second time.
        const settled = settledFrame[name];
        return Math.max(0, Math.min(1, frameValue(name, 1) / (settled > .00001 ? settled : 1)));
    }

    function listValue(name, index, fallbackValue) {
        const values = animationFrame[name];
        return values === undefined || index >= values.length ? fallbackValue : values[index];
    }

    function contentFrameValue(name, fallbackValue) {
        const value = contentAnimationFrame[name];
        return value === undefined ? fallbackValue : value;
    }

    function remainingContentFrameValue(name, fallbackValue) {
        const value = contentFrameValue(name, fallbackValue);
        return animationPhase === 2 ? value * contentFrameValue("nativeOpacity", 1) : value;
    }

    function baseContentFrameValue(name, fallbackValue) {
        const value = baseContentAnimationFrame[name];
        return value === undefined ? fallbackValue : value;
    }

    function contentListValue(name, index, fallbackValue) {
        const values = contentAnimationFrame[name];
        return values === undefined || index >= values.length ? fallbackValue : values[index];
    }

    function navigationFrameValue(name, fallbackValue) {
        const value = navigationAnimationFrame[name];
        return value === undefined ? fallbackValue : value;
    }

    function navigationListValue(name, index, fallbackValue) {
        const values = navigationAnimationFrame[name];
        return values === undefined || index >= values.length ? fallbackValue : values[index];
    }

    function animateEntrance() {
        sourceClip.stop();
        contentClip.stop();
        animationPhase = 1;
        sourceAmbientTime = 0;
        sourceWatchAnimation.restartAmbientMotion();
        if (displaySettings.reduceMotion) {
            animationTime = sourceWatchAnimation.openingDuration;
            contentAnimationTime = sourceWatchAnimation.openingDuration;
            animationPhase = 0;
            return;
        }
        animationTime = 0;
        contentAnimationTime = 0;
        sourceClip.from = 0;
        sourceClip.to = sourceWatchAnimation.openingDuration;
        sourceClip.duration = Math.round(sourceWatchAnimation.openingDuration * 1000);
        sourceClip.restart();
        contentClip.restart();
    }

    function animateExit() {
        sourceClip.stop();
        contentClip.stop();
        animationPhase = 2;
        if (displaySettings.reduceMotion) {
            animationTime = sourceWatchAnimation.closingDuration;
            return;
        }
        animationTime = -closingLeadDuration;
        sourceClip.from = -closingLeadDuration;
        sourceClip.to = sourceWatchAnimation.closingDuration;
        sourceClip.duration = Math.round(closingSequenceDuration * 1000);
        sourceClip.restart();
    }

    function retargetPointer(normalizedX, normalizedY) {
        const intensity = displaySettings.reduceMotion ? 0 : Math.max(0, Math.min(2, displaySettings.parallaxIntensity)) * Math.max(0, Math.min(2, displaySettings.perspectiveIntensity));
        const current = sourceGyro;
        sourceGyroMotion.stop();
        sourceGyroStart = current;
        sourceGyroTarget = sourceWatchAnimation.gyroTarget(normalizedX, normalizedY, intensity * 1.25);
        if (displaySettings.reduceMotion) sourceGyroProgress = 1;
        else { sourceGyroProgress = 0; sourceGyroMotion.restart(); }
    }

    NumberAnimation {
        id: sourceGyroMotion
        target: shell
        property: "sourceGyroProgress"
        from: 0
        to: 1
        duration: 500
        easing.type: Easing.OutQuad
    }

    FrameAnimation {
        // The original loop begins when the dial entrance finishes. It keeps
        // its clock through closing and stops when the HUD is concealed.
        running: appController.hudVisible && shell.sourceAmbientEnabled && shell.sourceEntranceFinished
        onTriggered: shell.sourceAmbientTime += frameTime
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
            duration: Math.max(0, Math.round((sourceWatchAnimation.openingDuration - shell.contentOverlapDuration) * 1000))
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

    MenuScene {
        shell: parent
        y: (640 - 1080 * (1000 / 1920)) / 2
        scale: 1000 / 1920
        transformOrigin: Item.TopLeft
    }

    Component.onCompleted: updateClock()
}
