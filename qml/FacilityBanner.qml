import QtQuick

// HUDFacilityBanner: 540 x 70, flat screen-top banner, five seconds with hover pause.
Item {
    id: root
    property bool presented: false
    property var current: ({})
    property real unfold: 1
    property real rise: 0
    property real fade: 1
    property real remaining: 5
    property bool active: false
    signal openModule(string module)
    width: 540; height: 70; visible: presented && active
    function refresh() {
        const alerts = facilityController.alerts;
        if (!presented) return;
        if (active) { const next = alerts.find(alert => alert.kind === current.kind); if (next) current = next; else dismiss(); return; }
        const next = alerts.find(alert => alert.isNew);
        if (!next) return;
        current = next; active = true; remaining = 5; unfold = settingsController.reduceMotion ? 1 : 0; rise = settingsController.reduceMotion ? 0 : -90; fade = 1;
        facilityController.markPresented(next.kind);
        if (!settingsController.reduceMotion) entrance.restart();
    }
    function dismiss() { if (!active || exit.running) return; if (settingsController.reduceMotion) finish(); else exit.restart(); }
    function finish() { active = false; Qt.callLater(refresh); }
    onPresentedChanged: { if (presented) refresh(); else { entrance.stop(); exit.stop(); active = false; } }
    Connections { target: facilityController; function onAlertsChanged() { root.refresh() } }
    ParallelAnimation {
        id: entrance
        NumberAnimation { target: root; property: "unfold"; to: 1; duration: 420; easing.type: Easing.BezierSpline; easing.bezierCurve: [.2, .9, .1, 1, 1, 1] }
        NumberAnimation { target: root; property: "rise"; to: 0; duration: 252; easing.type: Easing.BezierSpline; easing.bezierCurve: [.2, .9, .1, 1, 1, 1] }
    }
    ParallelAnimation {
        id: exit
        NumberAnimation { target: root; property: "rise"; to: -90; duration: 260; easing.type: Easing.InQuad }
        NumberAnimation { target: root; property: "fade"; to: 0; duration: 260; easing.type: Easing.InQuad }
        onFinished: root.finish()
    }
    FrameAnimation { running: root.visible && !pointer.containsMouse && !exit.running; onTriggered: { root.remaining = Math.max(0, root.remaining - frameTime); if (!root.remaining) root.dismiss(); } }
    Item {
        y: root.rise; width: 540 * root.unfold; height: 70; clip: true; opacity: root.fade
        Rectangle { width: 540; height: 70; border.color: "#4A3216"; gradient: Gradient { GradientStop { position: 0; color: "#1D1D1D" } GradientStop { position: 1; color: "#121212" } } }
        HazardStripes { x: 10; width: 78; height: 70 }
        Rectangle { x: 27; y: 13; width: 44; height: 44; color: "#F97501"; Rectangle { x: 2; y: 2; width: 40; height: 40; color: "#111111"; ClipboardGlyph { anchors.centerIn: parent; width: 22; height: 22; kind: "warning"; ink: "#F97501" } } }
        Text { id: label; x: 104; y: 7; text: qsTr("设施警报"); color: "#F97501"; font.family: harmonyBold.name; font.pixelSize: 10; font.weight: Font.Bold; font.letterSpacing: 1.8; renderType: Text.CurveRendering }
        Text { x: label.x + label.width + 8; y: label.y; width: 245 - label.width; text: root.current.english || ""; color: "#777777"; elide: Text.ElideRight; font.family: harmonyBold.name; font.pixelSize: 10; font.weight: Font.Bold; renderType: Text.CurveRendering }
        Text { x: 104; y: 22; width: 310; text: root.current.title || ""; color: "#F4F4F4"; elide: Text.ElideRight; font.family: harmonyBold.name; font.pixelSize: 17; font.weight: Font.Bold; font.letterSpacing: 1.36; renderType: Text.CurveRendering }
        Text { x: 104; y: 46; width: 310; text: root.current.detail || ""; color: "#9A9A9A"; elide: Text.ElideRight; font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering }
        Text { x: 418; y: 5; width: 82; horizontalAlignment: Text.AlignRight; text: root.current.value || ""; color: "#F97501"; font.family: harmonyRegular.name; font.pixelSize: 34; font.weight: Font.Light; renderType: Text.CurveRendering }
        Text { x: 418; y: 43; width: 82; horizontalAlignment: Text.AlignRight; text: root.current.unit || ""; color: "#9A9A9A"; font.family: harmonyBold.name; font.pixelSize: 9; font.weight: Font.Bold; font.letterSpacing: 1.26; renderType: Text.CurveRendering }
        Rectangle { x: 28; y: 68; width: 484 * (settingsController.reduceMotion ? 1 : root.remaining / 5); height: 2; color: "#F97501" }
    }
    MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { root.openModule(root.current.module); root.dismiss() } }
}
