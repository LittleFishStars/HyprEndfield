import QtQuick

Item {
    id: tile
    required property var entry
    property real leanX: 0
    property real leanY: 0
    property real rowX: 0
    property real rowY: 0
    property bool secondColumn: false
    property real lift: pointer.containsMouse ? 1 : 0
    signal triggered
    width: 107
    height: 104
    Behavior on lift {
        NumberAnimation {
            duration: 160
            easing.type: Easing.OutCubic
        }
    }
    Item {
        anchors.fill: parent
        scale: (1 + tile.lift * 0.0063) * (pointer.pressed ? 0.955 : 1)
        transform: Translate {
            x: tile.lift * ((tile.secondColumn ? 4.73 : 3.71) + tile.rowX * .0067)
            y: tile.lift * (tile.rowY * .0076 - 6.75)
        }
        Rectangle {
            x: -5 - 12.5 - .024 * tile.rowX + tile.leanX * 36
            y: -5 + 16.17 - .0207 * tile.rowY + tile.leanY * 36
            width: 117
            height: 114
            radius: 13
            color: "#63ffffff"
            border.width: 2.5
            border.color: "#52ffffff"
            antialiasing: true
        }
        Rectangle {
            x: -5
            y: -5
            width: 117
            height: 114
            radius: 13
            color: "transparent"
            border.width: 2.5
            border.color: "#9eeef1f5"
            antialiasing: true
        }
        MenuArt {
            anchors.fill: parent
            asset: "tile-face"
        }
        Rectangle {
            anchors.fill: parent
            radius: 8
            color: "#f6f6f4"
            opacity: tile.lift * .75
            antialiasing: true
        }
        Image {
            x: 31.5
            y: 17
            width: 44
            height: 44
            source: tile.entry.icon || ""
            visible: source.toString().length > 0
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
        }
        HudGlyph {
            x: 31.5
            y: 17
            width: 44
            height: 44
            kind: tile.entry.glyph || ""
            visible: kind.length > 0
            ink: "#313131"
        }
        Text {
            x: 2
            y: 70
            width: 103
            height: 29
            text: tile.entry.label
            color: "#313131"
            font.family: harmonyMedium.name
            font.pixelSize: text.length > 4 ? 17 : 21
            horizontalAlignment: Text.AlignHCenter
            renderType: Text.CurveRendering
        }
    }
    MouseArea {
        id: pointer
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: tile.triggered()
    }
}
