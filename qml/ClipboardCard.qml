import QtQuick
import QtQuick.Shapes

// EndfieldHUD HUDListCard: 58 px row, 10 px top-right chamfer.
Item {
    id: card
    required property var entry
    property bool focused: false
    property bool reduceMotion: false
    property string age: ""
    property real liftX: (hover.hovered || focused) ? 3 : 0
    signal copied(string id)
    signal pinToggled(string id)
    signal removed(string id)
    signal contextRequested(string id, real x, real y)
    height: 58
    transform: Translate { x: card.liftX }
    Behavior on liftX { NumberAnimation { duration: card.reduceMotion ? 0 : 220; easing.type: Easing.InOutQuad } }

    function platePath() { return "M0 0H" + (width - 10) + "L" + width + " 10V58H0Z" }
    function flashCopied() { flash.opacity = 1; flashFade.restart() }

    Shape {
        anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: card.width; y2: 58
                GradientStop { position: 0; color: "#E2E3DE" }
                GradientStop { position: 1; color: "#CFD0CB" }
            }
            PathSvg { path: card.platePath() }
        }
        ShapePath {
            strokeWidth: -1; fillColor: Qt.rgba(1, 1, 1, hover.hovered || card.focused ? 0.07 : 0)
            PathSvg { path: card.platePath() }
        }
        ShapePath {
            id: flashPath; strokeWidth: -1; fillColor: Qt.rgba(1, 0.98, 0, flash.opacity * 0.55)
            PathSvg { path: card.platePath() }
        }
    }
    QtObject { id: flash; property real opacity: 0 }
    NumberAnimation { id: flashFade; target: flash; property: "opacity"; from: 1; to: 0; duration: card.reduceMotion ? 0 : 600; easing.type: Easing.OutQuad }
    Rectangle { width: 3; height: 58; color: hover.hovered || card.focused ? "#181818" : "transparent" }

    Item {
        x: 8; y: 7; width: 44; height: 44
        Rectangle { anchors.fill: parent; color: "#1B1B1B" }
        Image {
            id: thumbnailImage
            x: 0; y: 0; width: 44; height: 35; visible: card.entry.thumbnail !== undefined && card.entry.thumbnail !== ""
            source: visible ? card.entry.thumbnail : ""; fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: true
        }
        ClipboardGlyph {
            x: 13; y: 11; width: 18; height: 18
            visible: !thumbnailImage.visible; kind: card.entry.kind === "url" ? "link" : card.entry.kind === "files" ? "file" : card.entry.kind
        }
        Rectangle { x: 0; y: 35; width: 44; height: 10; color: "#FFFA00" }
        Text {
            x: 0; y: 35; width: 44; height: 10; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter
            text: card.entry.kind === "url" ? "LINK" : card.entry.kind === "files" ? "FILE" : card.entry.kind.toUpperCase()
            color: "#181818"; font.family: harmonyBold.name; font.pointSize: 6.5 * 0.75; font.weight: Font.Black; font.letterSpacing: 0.65
            renderType: Text.CurveRendering
        }
    }
    Text {
        x: 64; y: 13; width: Math.max(80, card.width - 64 - 80 - 62); height: 17
        text: card.entry.kind === "image" ? "图片" : card.entry.preview; color: "#1A1A1A"; elide: Text.ElideRight
        font.family: harmonyMedium.name; font.pointSize: 13.5 * 0.75; font.weight: Font.Medium; renderType: Text.CurveRendering
    }
    Text {
        x: 64; y: 32.9; width: Math.max(80, card.width - 64 - 80 - 62); height: 13
        text: card.entry.meta || ""; color: "#6C6D68"; elide: Text.ElideRight
        font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; renderType: Text.CurveRendering
    }
    Text {
        x: card.width - 80; y: 13; width: 70; height: 16; horizontalAlignment: Text.AlignRight
        text: card.age; color: "#55564F"; font.family: harmonyBold.name; font.pixelSize: 10; font.weight: Font.Bold
        renderType: Text.CurveRendering
    }
    MouseArea {
        id: leadingClickArea
        x: 0; width: card.width - 134; height: card.height
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: mouse => {
            if (mouse.button === Qt.RightButton) card.contextRequested(card.entry.id, mouse.x, mouse.y)
            else card.copied(card.entry.id)
        }
    }
    MouseArea {
        id: trailingClickArea
        x: card.width - 80; width: 80; height: card.height
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: mouse => {
            if (mouse.button === Qt.RightButton) card.contextRequested(card.entry.id, trailingClickArea.x + mouse.x, mouse.y)
            else card.copied(card.entry.id)
        }
    }
    Item {
        id: actionStrip; x: card.width - 134; width: 54; height: 58
        opacity: hover.hovered || card.focused ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: card.reduceMotion ? 0 : 140 } }
        Repeater {
            model: [{kind: "pin", enabled: true}, {kind: "trash", enabled: !card.entry.pinned}]
            delegate: Item {
                required property var modelData; required property int index
                x: index * 30; y: 17; width: 24; height: 24; opacity: modelData.enabled ? 1 : 0.35
                scale: actionMouse.pressed ? 0.94 : actionHover.hovered ? 1.06 : 1
                Rectangle { anchors.fill: parent; color: index === 0 && card.entry.pinned ? "#FFFA00" : "#E1E2DD"; border.color: index === 0 && card.entry.pinned ? "transparent" : "#B9BAB5" }
                ClipboardGlyph { x: 5.5; y: 5.5; width: 13; height: 13; kind: modelData.kind; ink: "#33342F" }
                HoverHandler { id: actionHover }
                MouseArea {
                    id: actionMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; enabled: modelData.enabled
                    onClicked: index === 0 ? card.pinToggled(card.entry.id) : card.removed(card.entry.id)
                }
            }
        }
    }
    Item {
        x: card.width - 72; y: 29; width: 62; height: 16
        opacity: hover.hovered || card.focused ? 1 : 0
        transform: Translate { x: hover.hovered || card.focused ? 0 : 6 }
        Behavior on opacity { NumberAnimation { duration: card.reduceMotion ? 0 : 140 } }
        ClipboardGlyph { x: 0; y: 1; width: 14; height: 14; kind: "copy"; ink: "#55564F" }
        Text { x: 18; width: 44; height: 16; text: "复制"; color: "#55564F"; font.family: harmonyBold.name; font.pixelSize: 10; font.weight: Font.Bold; renderType: Text.CurveRendering }
    }
    Shape {
        x: card.width - 14; width: 14; height: 14; visible: card.entry.pinned; preferredRendererType: Shape.CurveRenderer
        ShapePath { strokeWidth: -1; fillColor: "#FFFA00"; PathSvg { path: "M0 0H14V14Z" } }
    }
    HoverHandler { id: hover }
}
