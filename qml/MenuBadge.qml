import QtQuick

// endfield_menu/player-badge.js. The content is local desktop identity only.
Item {
    id: badge
    property real leanX: 0
    property real leanY: 0
    width: 389
    height: 116
    Rectangle {
        x: 8 + badge.leanX * 16
        y: 9 + badge.leanY * 16
        width: 389
        height: 116
        radius: 20
        color: "#1fd6dbe1"
        antialiasing: true
    }
    MenuArt {
        anchors.fill: parent
        asset: "badge"
    }
    Text {
        x: 16
        y: 45
        width: 84
        text: "LOCAL"
        horizontalAlignment: Text.AlignHCenter
        font.family: harmonyMedium.name
        font.pixelSize: 10
        font.letterSpacing: 1
        color: "#8cffffff"
        renderType: Text.CurveRendering
    }
    Text {
        x: 120
        y: 8
        width: 195
        text: appController.localUserName
        elide: Text.ElideRight
        font.family: harmonyBold.name
        font.pixelSize: 24
        color: "white"
        renderType: Text.CurveRendering
    }
    Text {
        x: 120
        y: 37
        text: "[ LOCAL ]"
        font.family: harmonyRegular.name
        font.pixelSize: 11
        font.letterSpacing: 1.9
        color: "#66ffffff"
        renderType: Text.CurveRendering
    }
    Text {
        x: 120
        y: 47
        text: "ARCH"
        font.family: harmonyBold.name
        font.pixelSize: 36
        color: "white"
        renderType: Text.CurveRendering
    }
    Text {
        x: 120
        y: 96
        text: "Hyprland"
        font.family: harmonyRegular.name
        font.pixelSize: 13
        color: "#80ffffff"
        renderType: Text.CurveRendering
    }
}
