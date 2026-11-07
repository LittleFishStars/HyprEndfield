import QtQuick
import "Appearance.js" as Appearance

Item {
    id: cell
    property string value: ""
    property bool outside: false
    property bool weekend: false
    property bool event: false
    property bool today: false
    property bool selected: false
    property bool reduceMotion: false
    signal clicked()
    width: 42.57; height: 40
    transform: Translate { y: hover.hovered && !cell.outside ? -1 : 0 }

    Rectangle {
        anchors.fill: parent
        color: cell.outside ? Appearance.tone("#181818") : cell.today ? Appearance.accent("#FFFA00") : Appearance.tone("#1F1F1F")
        border.width: cell.selected && !cell.outside ? 2 : cell.today ? 0 : 1
        border.color: cell.selected ? (cell.today ? Appearance.tone("#111111") : Appearance.tone("#EDEDED")) : Appearance.tone("#343434")
    }
    Rectangle { anchors.fill: parent; color: "white"; opacity: hover.hovered && !cell.outside ? 0.06 : 0 }
    Text {
        y: (parent.height - 14) / 2; width: parent.width; height: 14; horizontalAlignment: Text.AlignHCenter
        text: cell.value; color: cell.outside ? Appearance.tone("#555555") : cell.today ? Appearance.tone("#111111") : cell.weekend ? Appearance.tone("#9A9A9A") : Appearance.tone("#DDDDDD")
        font.family: cell.today ? harmonyBold.name : harmonyRegular.name; font.pixelSize: 14; font.weight: cell.today ? Font.Bold : Font.Normal
        font.features: { "tnum": 1 }; renderType: Text.CurveRendering
    }
    Rectangle {
        visible: cell.event && !cell.outside; width: 5; height: 5; rotation: 45
        x: (parent.width - width) / 2; y: parent.height - 7.5; color: Appearance.accent("#FFFA00")
    }
    HoverHandler { id: hover; enabled: !cell.outside }
    MouseArea { anchors.fill: parent; enabled: !cell.outside; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: cell.clicked() }
}
