import QtQuick
import "Appearance.js" as Appearance

Item {
    id: root

    property real value: 0
    property real inputValue: 0
    property string sourceKey: ""
    property string gestureKey: ""
    property bool reduceMotion: false
    signal touched()
    property real minimum: 0
    property real maximum: 100
    property real stepSize: 1
    property real zeroValue: Number.NaN
    property string valueFormat: "percent"
    property Item bubbleHost: null
    readonly property real trackWidth: Math.max(40, width - 66)
    readonly property real fraction: maximum > minimum ? Math.max(0, Math.min(1, (value - minimum) / (maximum - minimum))) : 0
    readonly property real zeroFraction: maximum > minimum && Number.isFinite(zeroValue) ? Math.max(0, Math.min(1, (zeroValue - minimum) / (maximum - minimum))) : 0
    property bool dragging: false

    signal valueModified(real value, bool final)
    signal bubbleRequested(bool shown, string text, real x, real y)

    width: 366
    height: 30
    opacity: enabled ? 1 : 0.35
    Binding { target: root; property: "value"; value: root.clampedValue(root.inputValue); when: !root.dragging; restoreMode: Binding.RestoreNone }
    onSourceKeyChanged: cancelDrag()
    onEnabledChanged: if (!enabled) cancelDrag()
    function cancelDrag() { dragging = false; reportBubble(false); }
    function nudge(delta) {
        if (!enabled) return;
        gestureKey = sourceKey;
        touched();
        value = clampedValue(value + delta);
        valueModified(value, true);
    }

    function clampedValue(candidate) {
        const bounded = Math.max(minimum, Math.min(maximum, candidate));
        if (stepSize <= 0)
            return bounded;
        return Math.max(minimum, Math.min(maximum, Math.round((bounded - minimum) / stepSize) * stepSize + minimum));
    }

    function formattedValue(candidate) {
        if (valueFormat === "scale")
            return Number(candidate).toFixed(2) + "×";
        const rounded = Math.round(candidate);
        if (valueFormat === "balance")
            return Math.abs(candidate) < 3 ? "C" : (rounded < 0 ? "L " : "R ") + Math.abs(rounded);
        if (valueFormat === "signedPercent")
            return (rounded > 0 ? "+" : "") + rounded + "%";
        return rounded + "%";
    }

    function valueAt(pointerX) {
        return clampedValue(minimum + Math.max(0, Math.min(1, pointerX / Math.max(1, trackWidth))) * (maximum - minimum));
    }

    function reportBubble(shown) {
        if (!bubbleHost) {
            bubbleRequested(false, "", 0, 0);
            return;
        }
        const point = mapToItem(bubbleHost, fraction * trackWidth, 2);
        bubbleRequested(shown, formattedValue(value), point.x, point.y);
    }

    Rectangle {
        x: 0
        y: 11
        width: root.trackWidth
        height: 5
        color: Appearance.tone("#252525")
        border.width: 1
        border.color: Appearance.tone("#111111")
    }

    Rectangle {
        x: Number.isFinite(root.zeroValue) ? Math.min(root.fraction, root.zeroFraction) * root.trackWidth : 0
        y: 11
        width: Number.isFinite(root.zeroValue) ? Math.abs(root.fraction - root.zeroFraction) * root.trackWidth : root.fraction * root.trackWidth
        height: 5
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: 0
                color: Appearance.accent("#E9CF12")
            }
            GradientStop {
                position: 1
                color: Appearance.accent("#FFFA00")
            }
        }
    }

    Rectangle {
        visible: Number.isFinite(root.zeroValue)
        x: root.zeroFraction * root.trackWidth - 0.5
        y: 8
        width: 1
        height: 11
        color: Appearance.tone("#111111")
    }

    Repeater {
        model: 11

        Rectangle {
            required property int index
            x: index * root.trackWidth / 10
            y: 21
            width: 1
            height: 4
            color: Appearance.tone("#8D8E89")
        }
    }

    Rectangle {
        id: thumb
        x: root.fraction * root.trackWidth - 4.5
        y: 5
        width: 9
        height: 17
        color: Appearance.tone("#151515")
        property real growth: root.dragging || sliderPointer.containsMouse ? 21 / 17 : 1
        transform: Scale { origin.x: 4.5; origin.y: 8.5; yScale: thumb.growth }
        Behavior on growth {
            NumberAnimation { duration: root.reduceMotion ? 0 : 120; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] }
        }

        Rectangle {
            x: 3.5
            y: (parent.height - 11) / 2
            width: 2
            height: 11
            color: Appearance.accent("#FFFA00")
        }
    }

    Item {
        x: root.width - 58
        y: 9
        width: 58
        height: 12
        Text {
            anchors.right: parent.right
            renderType: Text.CurveRendering
            text: "<font color='#8A8B86'>[ </font>" + root.formattedValue(root.value) + "<font color='#8A8B86'> ]</font>"
            textFormat: Text.RichText
            color: Appearance.tone("#121212")
            font.family: harmonyMedium.name
            font.pixelSize: 12
            font.weight: Font.Bold
        }
    }

    MouseArea {
        id: sliderPointer
        x: -4.5
        y: 0
        width: root.trackWidth + 9
        height: 30
        hoverEnabled: true
        preventStealing: true
        cursorShape: Qt.PointingHandCursor

        onPressed: mouse => {
            root.gestureKey = root.sourceKey;
            root.touched();
            root.dragging = true;
            root.value = root.valueAt(mouse.x - 4.5);
            root.valueModified(root.value, false);
            root.reportBubble(true);
        }
        onPositionChanged: mouse => {
            if (!pressed || !root.dragging)
                return;
            root.value = root.valueAt(mouse.x - 4.5);
            root.valueModified(root.value, false);
            root.reportBubble(true);
        }
        onReleased: {
            if (!root.dragging) return;
            root.valueModified(root.value, true);
            root.dragging = false;
            root.reportBubble(false);
        }
        onCanceled: {
            root.dragging = false;
            root.reportBubble(false);
        }
    }
}
