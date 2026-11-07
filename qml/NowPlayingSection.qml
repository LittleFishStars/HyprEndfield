import QtQuick

// Source HUDMotion.enter / leave: 44 px travel, 130 ms leave, 380 ms entrance,
// 60 + index*36 ms stagger. The move's easing is nested inside the group's easing.
Item {
    id: section
    required property var page
    property int sectionIndex: 0
    property real progress: 1
    function sourceEase(value) {
        let lo = 0, hi = 1;
        for (let i = 0; i < 16; ++i) {
            const t = (lo + hi) / 2, u = 1 - t;
            const x = 0.6 * u * u * t + 0.6 * u * t * t + t * t * t;
            if (x < value) lo = t; else hi = t;
        }
        const t = (lo + hi) / 2, u = 1 - t;
        return 2.4 * u * u * t + 3 * u * t * t + t * t * t;
    }
    opacity: page.motionPhase === "leave" ? 1 - progress
        : page.motionPhase === "enter" ? Math.min(1, progress / 0.6) : 1
    transform: Translate {
        x: section.page.motionPhase === "leave" ? -26.4 * section.page.direction * section.progress
            : section.page.motionPhase === "enter" ? 44 * section.page.direction * (1 - section.sourceEase(section.progress)) : 0
    }
    Connections {
        target: section.page
        function onMotionPhaseChanged() {
            entrance.stop(); leave.stop();
            if (section.page.motionPhase === "enter") entrance.start();
            else if (section.page.motionPhase === "leave") leave.start();
            else section.progress = 1;
        }
    }
    SequentialAnimation {
        id: entrance
        PropertyAction { target: section; property: "progress"; value: 0 }
        PauseAnimation { duration: 60 + section.sectionIndex * 36 }
        NumberAnimation {
            target: section; property: "progress"; from: 0; to: 1; duration: 380
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1]
        }
    }
    NumberAnimation {
        id: leave
        target: section; property: "progress"; from: 0; to: 1; duration: 130
        easing.type: Easing.BezierSpline
        easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1]
    }
}
