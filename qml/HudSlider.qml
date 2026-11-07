import QtQuick
import "Appearance.js" as Appearance

Item {
    id: root

    property real value: 0
    property real minimum: 0
    property real maximum: 100
    property real stepSize: 1
    property real zeroValue: Number.NaN
    property string valueFormat: "percent"
    property Item bubbleHost: null
    readonly property real trackWidth: Math.max(40, width - 66)
    property real dragValue: value
    readonly property real shownValue: dragging ? dragValue : value
    readonly property real fraction: maximum > minimum ? Math.max(0, Math.min(1, (shownValue - minimum) / (maximum - minimum))) : 0
    readonly property real zeroFraction: maximum > minimum && Number.isFinite(zeroValue) ? Math.max(0, Math.min(1, (zeroValue - minimum) / (maximum - minimum))) : 0
    property bool dragging: false

    signal valueModified(real value, bool final)
    signal bubbleRequested(bool shown, string text, real x, real y)

    width: 366
    height: 30

    function clampedValue(candidate) {
        const bounded = Math.max(minimum, Math.min(maximum, candidate));
        if (stepSize <= 0)
            return bounded;
        return Math.max(minimum, Math.min(maximum, Math.round((bounded - minimum) / stepSize) * stepSize + minimum));
    }

    function formattedValue(candidate) {
        if (valueFormat === "scale")
            return Number(candidate).toFixed(2) + "×";
        if (valueFormat === "seconds") return Math.round(candidate) + " s";
        if (valueFormat === "pixels") return Math.round(candidate) + " PX";
        if (valueFormat === "decimal") return Number(candidate).toFixed(stepSize < .1 ? 2 : stepSize < 1 ? 1 : 0);
        const rounded = Math.round(candidate);
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
        bubbleRequested(shown, formattedValue(shownValue), point.x, point.y);
    }

    Rectangle {
        x: 0
        y: 11
        width: root.trackWidth
        height: 5
        color: "#252525"
        border.width: 1
        border.color: "#111111"
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
        color: "#111111"
    }

    Repeater {
        model: 11

        Rectangle {
            required property int index
            x: index * root.trackWidth / 10
            y: 21
            width: 1
            height: 4
            color: "#8D8E89"
        }
    }

    Rectangle {
        id: thumb
        x: root.fraction * root.trackWidth - 4.5
        y: root.dragging || sliderPointer.containsMouse ? 3 : 5
        width: 9
        height: root.dragging || sliderPointer.containsMouse ? 21 : 17
        color: "#151515"

        Behavior on y {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }
        Behavior on height {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }

        Rectangle {
            x: 3.5
            y: (parent.height - 11) / 2
            width: 2
            height: 11
            color: Appearance.accent("#FFFA00")
        }
    }

    Row {
        x: root.width - 58
        y: 9
        width: 58
        height: 12
        layoutDirection: Qt.LeftToRight

        Text {
            renderType: Text.CurveRendering
            text: "[ "
            color: "#8A8B86"
            font.family: harmonyMedium.name
            font.pixelSize: 12
            font.weight: Font.Bold
        }
        Text {
            renderType: Text.CurveRendering
            text: root.formattedValue(root.shownValue)
            color: "#121212"
            font.family: harmonyMedium.name
            font.pixelSize: 12
            font.weight: Font.Bold
        }
        Text {
            renderType: Text.CurveRendering
            text: " ]"
            color: "#8A8B86"
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
        cursorShape: Qt.PointingHandCursor

        onPressed: mouse => {
            root.dragging = true;
            root.dragValue = root.valueAt(mouse.x - 4.5);
            root.valueModified(root.dragValue, false);
            root.reportBubble(true);
        }
        onPositionChanged: mouse => {
            if (!pressed)
                return;
            root.dragValue = root.valueAt(mouse.x - 4.5);
            root.valueModified(root.dragValue, false);
            root.reportBubble(true);
        }
        onReleased: {
            root.valueModified(root.dragValue, true);
            root.dragging = false;
            root.reportBubble(false);
        }
        onCanceled: {
            root.dragging = false;
            root.reportBubble(false);
        }
    }
}
