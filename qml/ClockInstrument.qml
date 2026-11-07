import QtQuick
import QtQuick.Shapes
import "Appearance.js" as Appearance

// HUDClockStyleArtwork.update: source coordinates, ruler and hand geometry.
Item {
    id: face
    property string timeText: "00:00:00"
    property string dateText: ""
    property int styleIndex: 0
    readonly property string digits: timeText.split(" ")[0]
    readonly property string meridiem: timeText.split(" ")[1] || ""
    readonly property var parts: digits.split(":")
    readonly property int hours: (Number(parts[0]) % 12) + (meridiem === "PM" ? 12 : meridiem ? 0 : (Number(parts[0]) >= 12 ? 12 : 0))
    readonly property int minutes: Number(parts[1]) || 0
    readonly property int seconds: Number(parts[2]) || 0
    readonly property color accent: Appearance.accent("#FAD41F")
    width: 340; height: 122
    Text {
        id: time
        x: face.styleIndex === 2 ? 104 : 22
        readonly property real capTop: [17, 20, 22, 17, 46][face.styleIndex]
        y: capTop - (timeMetrics.ascent - timeMetrics.capitalHeight)
        width: [296 - (face.meridiem ? 44 : 0), 196, 214 - (face.meridiem ? 30 : 0), 200, 222][face.styleIndex]
        height: timeMetrics.height + 2
        text: face.styleIndex === 1 ? face.parts.slice(0, 2).join(":") : face.digits
        font.family: harmonyBold.name; font.weight: Font.Bold
        font.pixelSize: [50, 62, 38, 40, 44][face.styleIndex]; fontSizeMode: Text.HorizontalFit; minimumPixelSize: 10
        color: "#F5F5F5"; renderType: Text.CurveRendering
    }
    FontMetrics { id: timeMetrics; font: time.font }
    Text {
        id: date
        x: [22, 238, 104, 228, 30][face.styleIndex]
        readonly property real capTop: [68, 58, 64, 20, 20][face.styleIndex]
        y: capTop - (dateMetrics.ascent - dateMetrics.capitalHeight)
        width: [120, 80, 214, 90, 150][face.styleIndex]; height: dateMetrics.height + 2
        text: face.dateText; font.family: face.styleIndex === 4 ? harmonyBold.name : harmonyMedium.name
        font.pixelSize: face.styleIndex === 1 ? 12 : 13; fontSizeMode: Text.HorizontalFit; minimumPixelSize: 10
        horizontalAlignment: face.styleIndex === 3 ? Text.AlignRight : Text.AlignLeft
        color: face.styleIndex === 4 ? "#111111" : "#A3A3A3"; z: 1; renderType: Text.CurveRendering
    }
    FontMetrics { id: dateMetrics; font: date.font }
    Text {
        visible: face.styleIndex === 1; x: 238; y: 15; width: 46; height: 40
        text: face.parts[2] || "00"; color: face.accent; font.family: harmonyBold.name; font.pixelSize: 30; renderType: Text.CurveRendering
    }
    Text {
        visible: face.meridiem.length > 0
        x: [278, 286, 288, 228, 270][face.styleIndex]; y: [14, 19, 19, 37, 17][face.styleIndex]
        width: face.styleIndex === 3 ? 90 : 40; height: 20; text: face.meridiem
        color: face.styleIndex === 4 ? "#111111" : "#A3A3A3"; horizontalAlignment: Text.AlignRight; z: 1
        font.family: harmonyBold.name; font.pixelSize: face.styleIndex === 0 ? 14 : 12; renderType: Text.CurveRendering
    }
    Repeater {
        model: face.styleIndex === 0 ? 61 : face.styleIndex === 3 ? 25 : face.styleIndex === 4 ? 7 : 0
        Rectangle {
            required property int index
            readonly property bool day: face.styleIndex === 4
            readonly property bool selected: day && index === ["MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"].indexOf(face.dateText.slice(0, 3).toUpperCase())
            readonly property real rulerStart: 22 + Math.min(124, date.contentWidth + 14)
            x: day ? 252 + index * 9.5 : face.styleIndex === 3 ? 22 + 296 * index / 24 - .5 : rulerStart + (318 - rulerStart) * index / 60 - .5
            width: day ? 7 : 1
            height: day ? selected ? 24 : 16 : face.styleIndex === 3 ? index % 6 === 0 ? 8 : 4 : index % 15 === 0 ? 9 : index % 5 === 0 ? 6 : 3
            y: day ? selected ? 58 : 62 : (face.styleIndex === 3 ? 71 : 80) - height
            color: selected || face.styleIndex === 0 && index <= face.seconds ? face.accent : "#24FFFFFF"
        }
    }
    Rectangle { visible: face.styleIndex === 1 || face.styleIndex === 3; x: face.styleIndex === 1 ? 238 : 22; y: 74; width: face.styleIndex === 1 ? 80 : 296; height: face.styleIndex === 1 ? 3 : 4; color: "#24FFFFFF"
        Rectangle { width: parent.width * (face.styleIndex === 1 ? (face.seconds + 1) / 60 : (face.hours * 60 + face.minutes) / 1440); height: parent.height; color: face.accent }
    }
    Shape {
        anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: "transparent"; strokeColor: Qt.rgba(face.accent.r, face.accent.g, face.accent.b, .55); strokeWidth: 1.5
            PathSvg { path: {
                if (face.styleIndex === 1) return "M226 18V82";
                if (face.styleIndex !== 2) return "";
                let p = "M95 49.5A36 36 0 1 0 23 49.5A36 36 0 1 0 95 49.5M104 56.5H318";
                for (let i = 0; i < 60; i += 5) { const a = i * Math.PI / 30; p += `M${59 + Math.sin(a) * 26} ${49.5 - Math.cos(a) * 26}L${59 + Math.sin(a) * 32} ${49.5 - Math.cos(a) * 32}`; }
                return p;
            } }
        }
        ShapePath {
            fillColor: "transparent"; strokeColor: "#F5F5F5"; strokeWidth: 2.5; capStyle: ShapePath.RoundCap
            PathSvg { path: {
                if (face.styleIndex !== 2) return "";
                const hour = (face.hours % 12 + face.minutes / 60) * Math.PI / 6, minute = (face.minutes + face.seconds / 60) * Math.PI / 30;
                return `M59 49.5L${59 + Math.sin(hour) * 17} ${49.5 - Math.cos(hour) * 17}M59 49.5L${59 + Math.sin(minute) * 25} ${49.5 - Math.cos(minute) * 25}`;
            } }
        }
        ShapePath {
            fillColor: "transparent"; strokeColor: face.accent; strokeWidth: 1.2
            PathSvg { path: { const a = face.seconds * Math.PI / 30; return face.styleIndex === 2 ? `M${59 - Math.sin(a) * 7} ${49.5 + Math.cos(a) * 7}L${59 + Math.sin(a) * 30} ${49.5 - Math.cos(a) * 30}` : "" } }
        }
        ShapePath {
            fillColor: face.accent; strokeWidth: -1
            PathSvg { path: {
                if (face.styleIndex === 4) return "M22 14H311L318 21V35H22Z";
                if (face.styleIndex === 2) return "M61.5 49.5A2.5 2.5 0 1 0 56.5 49.5A2.5 2.5 0 1 0 61.5 49.5";
                if (face.styleIndex === 3) { const x = 22 + 296 * (face.hours * 60 + face.minutes) / 1440; return `M${x-4} 82H${x+4}L${x} 78Z`; }
                return "";
            } }
        }
    }
    Repeater {
        model: face.styleIndex === 2 ? 60 : 0
        Rectangle { required property int index; visible: index % 5 !== 0; x: 59 + Math.sin(index * Math.PI / 30) * 31 - .6; y: 49.5 - Math.cos(index * Math.PI / 30) * 31 - .6; width: 1.2; height: 1.2; radius: .6; color: "#24FFFFFF" }
    }
}
