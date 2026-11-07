import QtQuick
import QtQuick.Shapes

// HUDFacilityTagStack: source 272 px stack, 28 px rows and dismiss strike.
Item {
    id: root
    width: 272; height: list.height
    signal openModule(string module)
    ListModel { id: tags }
    function refresh() {
        const alerts = facilityController.alerts;
        for (let i = tags.count - 1; i >= 0; --i) if (!alerts.some(a => a.kind === tags.get(i).kind)) tags.remove(i);
        for (let alert of alerts) {
            let index = -1; for (let i = 0; i < tags.count; ++i) if (tags.get(i).kind === alert.kind) { index = i; break; }
            const item = {kind: alert.kind, moduleName: alert.module, label: alert.tag, reading: alert.value, unit: alert.unit};
            if (index < 0) tags.append(item); else tags.set(index, item);
        }
    }
    Component.onCompleted: refresh()
    Connections { target: facilityController; function onAlertsChanged() { root.refresh() } }
    Column {
        id: list; width: 272; spacing: 6
        Repeater {
            model: tags
            Item {
                id: tag; required property string kind; required property string moduleName; required property string label; required property string reading; required property string unit
                property real entrance: 0
                property real struck: 0
                property real collapse: 0
                width: Math.min(272, text.implicitWidth + 73); height: 28 * (1 - collapse); x: 272 - width + 16 * (1 - entrance) - (pointer.containsMouse ? 3 : 0)
                opacity: entrance * (1 - collapse); enabled: struck === 0
                Behavior on x { enabled: tag.entrance >= 1; NumberAnimation { duration: settingsController.reduceMotion ? 0 : 220 } }
                Component.onCompleted: if (settingsController.reduceMotion) entrance = 1; else enter.start()
                NumberAnimation { id: enter; target: tag; property: "entrance"; to: 1; duration: 350; easing.type: Easing.BezierSpline; easing.bezierCurve: [.2, .8, .2, 1, 1, 1] }
                SequentialAnimation {
                    id: dismiss
                    NumberAnimation { target: tag; property: "struck"; to: 1; duration: settingsController.reduceMotion ? 0 : 280; easing.type: Easing.BezierSpline; easing.bezierCurve: [.2, .8, .2, 1, 1, 1] }
                    PauseAnimation { duration: settingsController.reduceMotion ? 0 : 20 }
                    NumberAnimation { target: tag; property: "collapse"; to: 1; duration: settingsController.reduceMotion ? 0 : 250 }
                    ScriptAction { script: facilityController.dismiss(tag.kind) }
                }
                Item {
                    width: tag.width; height: 28; y: -tag.collapse * 14
                    Shape { anchors.fill: parent; preferredRendererType: Shape.CurveRenderer; ShapePath { strokeColor: "#4A3216"; strokeWidth: 1; fillColor: "#EB121212"; PathSvg { path: "M.5 .5H" + (tag.width - .5) + "V27.5H10L.5 18Z" } } }
                    HazardStripes { x: 1; y: 1; width: 11; height: 17; period: 8 * Math.SQRT2; cycle: 600 }
                    ClipboardGlyph { x: 20; y: 7.5; width: 13; height: 13; kind: "warning"; ink: "#F97501" }
                    Text { id: text; x: 41; y: 6; height: 16; text: tag.label + " <font color='#F97501'>" + tag.reading + "</font> " + tag.unit; textFormat: Text.RichText; color: "#F4F4F4"; font.family: harmonyBold.name; font.pixelSize: 11; font.weight: Font.Bold; renderType: Text.CurveRendering }
                    MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.openModule(tag.moduleName) }
                    Text { x: tag.width - 24; y: 3; width: 20; height: 20; text: "×"; horizontalAlignment: Text.AlignHCenter; color: close.containsMouse ? "white" : "#AAAAAA"; font.pixelSize: 18; renderType: Text.CurveRendering; MouseArea { id: close; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: dismiss.start() } }
                    Rectangle { x: 14; y: 13; width: (tag.width - 42) * tag.struck; height: 2; color: "#F97501" }
                }
            }
        }
    }
}
