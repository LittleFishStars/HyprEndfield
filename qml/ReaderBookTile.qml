import QtQuick
import "Appearance.js" as Appearance
import QtQuick.Shapes

Item {
    id: tile
    required property var entry
    property bool selected: false
    property bool reduceMotion: false
    signal opened()
    signal moreRequested(real x, real y)
    height: 144

    Rectangle {
        id: well; x: 0; y: hover.hovered ? -2 : 0; width: tile.width; height: 120; clip: true
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: Appearance.tone("#8A8885") }
            GradientStop { position: 0.55; color: Appearance.tone("#BDBDBB") }
            GradientStop { position: 1; color: Appearance.tone("#D9D9D8") }
        }
        Behavior on y { NumberAnimation { duration: tile.reduceMotion ? 0 : 250; easing.type: Easing.OutCubic } }
        Rectangle { anchors.fill: parent; color: "#1FFFFFFF"; opacity: hover.hovered ? 1 : 0; Behavior on opacity { NumberAnimation { duration: tile.reduceMotion ? 0 : 120 } } }
        Rectangle { x: 0; y: 0; width: 34; height: 4; color: Appearance.accent("#FFFA00") }
        Text { x: 38; y: -1; text: "#" + String(tile.entry.number || 1).padStart(2, "0"); color: Appearance.tone("#222222"); font.family: harmonyBold.name; font.pixelSize: 6; font.weight: Font.Bold; renderType: Text.CurveRendering }
        Rectangle { width: 58; height: 82; anchors.centerIn: parent; anchors.verticalCenterOffset: 8; rotation: -4; color: "#59000000" }
        Rectangle {
            id: cover; width: 58; height: 82; anchors.centerIn: parent; rotation: -4
            color: tile.entry.cover || "#2A5D7A"
            Text {
                x: 6; y: 82 - 6 - 9 * 1.05 * 4; width: 46; height: 9 * 1.05 * 4
                text: tile.entry.title || ""; color: Appearance.tone("#FFFFFF"); wrapMode: Text.Wrap; maximumLineCount: 4; elide: Text.ElideRight
                font.family: harmonyBold.name; font.pixelSize: 9; font.weight: Font.Black; lineHeight: 1.05; lineHeightMode: Text.ProportionalHeight; renderType: Text.CurveRendering
            }
        }
        Rectangle {
            x: tile.width - width - 4; y: 101; width: formatText.width + 8; height: 12; color: "#BF141414"
            Text { id: formatText; x: 4; y: 1.75; text: tile.entry.format || ""; color: Appearance.tone("#EAEAEA"); font.family: harmonyBold.name; font.pointSize: 8.5 * 0.75; font.weight: Font.Bold; renderType: Text.CurveRendering }
        }
        Rectangle { x: 0; y: 117; width: tile.width; height: 3; color: "#40000000"; Rectangle { width: parent.width * Math.max(0, Math.min(1, tile.entry.progress || 0)); height: 3; color: Appearance.accent("#FFFA00") } }
        Rectangle {
            anchors.fill: parent; visible: !tile.entry.available; color: "#8C000000"
            Text { anchors.centerIn: parent; text: "MISSING"; color: Appearance.accent("#FFFA00"); font.family: harmonyBold.name; font.pixelSize: 10; font.weight: Font.Bold; font.letterSpacing: 1; renderType: Text.CurveRendering }
        }
    }
    Rectangle {
        x: 0; y: 120; width: tile.width; height: 24; color: tile.selected ? Appearance.tone("#1A1A1A") : Appearance.tone("#C4C4C6")
        Text { x: 6; y: 6.25; width: parent.width - 12; text: tile.entry.title || ""; color: tile.selected ? Appearance.accent("#FFFA00") : Appearance.tone("#1A1A1A"); horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight; font.family: harmonyMedium.name; font.pointSize: 11.5 * 0.75; font.weight: Font.Medium; renderType: Text.CurveRendering }
    }
    Shape {
        x: -4; y: -4; width: tile.width + 8; height: tile.height + 8; visible: tile.selected
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1; fillColor: Appearance.accent("#FFFA00")
            PathSvg { path: { const w = tile.width + 8, h = tile.height + 8; return "M0 0h9v2H0zM0 0h2v" + h + "H0zM0 " + (h - 2) + "h9v2H0zM" + (w - 9) + " 0h9v2h-9zM" + (w - 2) + " 0h2v" + h + "h-2zM" + (w - 9) + " " + (h - 2) + "h9v2h-9z" } }
        }
    }
    Rectangle {
        id: more; x: tile.width - 26; y: 8; width: 22; height: 22; radius: 11
        color: moreHover.hovered ? "#F2141414" : "#CC141414"; scale: moreHover.hovered ? 1.08 : 1
        Text { anchors.horizontalCenter: parent.horizontalCenter; y: 2; text: "⋯"; color: Appearance.tone("#FFFFFF"); font.family: harmonyBold.name; font.pixelSize: 12; font.weight: Font.Black; renderType: Text.CurveRendering }
        HoverHandler { id: moreHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: tile.moreRequested(more.x + more.width / 2, more.y + more.height) }
        Behavior on scale { NumberAnimation { duration: tile.reduceMotion ? 0 : 120 } }
    }
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { acceptedButtons: Qt.LeftButton; onTapped: tile.opened() }
}
