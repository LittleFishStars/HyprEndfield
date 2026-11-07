import QtQuick
import QtQuick.Shapes
import "Appearance.js" as Appearance

// HUDBootLog: source geometry, reveal cadence, centre-X entrance and upward exit.
Item {
    id: boot
    width: 570; height: 440; visible: playing
    property bool playing: false
    property bool finishing: false
    property var lines: []
    property int revealed: 0
    property real unfold: 0
    property real closeProgress: 0
    signal finished()
    function entry(key, value, color, trailing) {
        let result = [{text: ">", color: "#8A8A8A"}, {text: " " + key + " " + ".".repeat(Math.max(2, 22 - key.length)) + " ", color: "#CFCFCF"}, {text: value, color: color || Appearance.accent("#FFFA00")}];
        if (trailing) result.push({text: "  " + trailing, color: "#CFCFCF"});
        return result;
    }
    function play() {
        cancel();
        const facts = facilityController.bootFacts(), battery = batteryController.state;
        let rows = [[{text: ">", color: "#8A8A8A"}, {text: " BOOT SEQUENCE  ", color: "#CFCFCF"}, {text: Qt.formatDateTime(new Date(), "yyyy.MM.dd HH:mm"), color: Appearance.accent("#FFFA00")}]];
        if (facts.uptime !== undefined) { const s = Math.floor(facts.uptime); rows.push(entry("UPTIME", Math.floor(s / 86400) + "D " + String(Math.floor(s % 86400 / 3600)).padStart(2, "0") + "H " + String(Math.floor(s % 3600 / 60)).padStart(2, "0") + "M")); }
        if (battery.available) rows.push(entry("POWER", battery.percentage + "%", "#8CE03C", battery.plugged ? "CHARGE MODE" : "POWER MODE"));
        const day = Qt.formatDate(new Date(), "yyyy-MM-dd"), events = calendarController.state.events.filter(event => event.day === day);
        rows.push(entry("CALENDAR", events.length + " EVENT" + (events.length === 1 ? "" : "S") + " TODAY"));
        for (const event of events.slice(0, 2)) rows.push([{text: "   " + (settingsController.recordingMode ? "[ REDACTED ]" : event.title.replace(/[\r\n]/g, " ")), color: "#9A9A9A"}]);
        rows.push(entry("WORK MODE", (focusModeController.state.trackedWorkSeconds / 3600).toFixed(1) + " H TOTAL"));
        if (facts.disk) { const percent = Math.round(facts.disk.available / facts.disk.total * 100); rows.push(entry("FACILITY", percent < 10 ? "DISK " + percent + "% FREE" : "NOMINAL", percent < 10 ? "#F97501" : "#8CE03C")); }
        rows.push([{text: "ALL SYSTEMS ONLINE.", color: "#8CE03C"}, {text: " WELCOME BACK, " + (settingsController.recordingMode ? "ENDMINISTRATOR" : appController.localUserName.toUpperCase()) + ".", color: "#CFCFCF"}]);
        lines = rows.slice(0, 15); playing = true; forceActiveFocus(); enter.restart(); revealTimer.interval = 280; revealTimer.restart();
    }
    function skip() { if (!playing || finishing) return; finishing = true; revealTimer.stop(); leave.restart(); }
    function cancel() { enter.stop(); leave.stop(); revealTimer.stop(); playing = false; finishing = false; revealed = 0; unfold = 0; closeProgress = 0; }
    Keys.onPressed: event => { skip(); event.accepted = true }
    MouseArea { x: -3000; y: -3000; width: 7000; height: 7000; acceptedButtons: Qt.AllButtons; onPressed: boot.skip() }
    NumberAnimation { id: enter; target: boot; property: "unfold"; from: 0; to: 1; duration: 300; easing.type: Easing.BezierSpline; easing.bezierCurve: [.2, .9, .1, 1, 1, 1] }
    NumberAnimation { id: leave; target: boot; property: "closeProgress"; from: 0; to: 1; duration: 380; easing.type: Easing.BezierSpline; easing.bezierCurve: [.2, .8, .2, 1, 1, 1]; onFinished: { boot.cancel(); boot.finished() } }
    Timer { id: revealTimer; onTriggered: { if (boot.revealed >= boot.lines.length) { boot.skip(); return; } ++boot.revealed; interval = boot.revealed === boot.lines.length ? 900 : boot.revealed === 1 ? 260 : 190; restart(); } }
    Item {
        x: (boot.width - width) / 2; width: boot.width * boot.unfold; height: boot.height * (1 - boot.closeProgress); clip: true; opacity: 1 - boot.closeProgress * .8
        Item {
            x: -(boot.width - parent.width) / 2; width: boot.width; height: boot.height
            Shape { anchors.fill: parent; preferredRendererType: Shape.CurveRenderer; ShapePath { strokeWidth: -1; fillColor: "#F00A0A0A"; PathSvg { path: "M0 0H554L570 16V440H0Z" } } }
            Rectangle { width: 4; height: 440; color: Appearance.accent("#FFFA00") }
            Shape { anchors.fill: parent; preferredRendererType: Shape.CurveRenderer; ShapePath { strokeWidth: 1; strokeColor: "#2E2E2E"; fillColor: "transparent"; PathSvg { path: "M0.5 .5H554M569.5 16V439.5H.5V.5" } } }
            Rectangle { x: 24; y: 20; width: 4; height: 16; color: Appearance.accent("#FFFA00") }
            Text { x: 38; y: 19; text: "BOOT LOG"; color: "#E8E8E8"; font.family: harmonyBold.name; font.pixelSize: 11; font.weight: Font.Bold; font.letterSpacing: 1.76; renderType: Text.CurveRendering }
            Text { x: 150; y: 19; width: 396; horizontalAlignment: Text.AlignRight; text: "ENDFIELD INDUSTRIES TERMINAL v" + settingsController.version; color: "#8A8A8A"; font.family: harmonyBold.name; font.pixelSize: 11; font.weight: Font.Bold; font.letterSpacing: 1.76; renderType: Text.CurveRendering }
            Rectangle { x: 24; y: 48; width: 522; height: 1; color: "#2E2E2E" }
            Repeater {
                model: boot.lines
                Row {
                    id: row; required property var modelData; required property int index
                    x: 24 + (index < boot.revealed ? 0 : -6); y: 61 + index * 22; height: 22; spacing: 8; opacity: index < boot.revealed ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.BezierSpline; easing.bezierCurve: [.25, .1, .25, 1, 1, 1] } }
                    Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.BezierSpline; easing.bezierCurve: [.2, .8, .2, 1, 1, 1] } }
                    Repeater { model: row.modelData; Text { required property var modelData; text: modelData.text; color: modelData.color; font.family: harmonyMedium.name; font.pointSize: 12.5 * 0.75; font.weight: Font.Medium; renderType: Text.CurveRendering } }
                }
            }
            Rectangle {
                x: 24; y: 64.5 + boot.revealed * 22; width: 9; height: 15; color: Appearance.accent("#FFFA00")
                SequentialAnimation on opacity { running: boot.playing; loops: Animation.Infinite; PropertyAction { value: 1 } PauseAnimation { duration: 450 } PropertyAction { value: 0 } PauseAnimation { duration: 450 } }
            }
        }
    }
}
