import QtQuick
import "Appearance.js" as Appearance

// HUDSettingsCanvas uses the same 590 × 81 HUDModuleHeader on every subpage.
Item {
    id: header
    property string title: ""
    property string english: ""
    property string crumbs: english
    property string code: "SYS-01"
    property string count: "06"
    property bool back: false
    signal backRequested()
    width: 590; height: 81
    Text { y: 0; height: 14; text: "// HUD  /  " + header.crumbs; color: Appearance.tone("#8A8A8A"); font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering }
    Text { x: 500; width: 49; height: 14; text: header.code; color: Appearance.tone("#8A8A8A"); font.family: harmonyRegular.name; font.pixelSize: 11; horizontalAlignment: Text.AlignRight; renderType: Text.CurveRendering }
    Item {
        x: 556; y: 3.5; width: 34; height: 7; clip: true
        Repeater { model: 12; Rectangle { required property int index; x: index * 3.5 - 7; y: -5; width: 1; height: 18; rotation: 45; color: Appearance.tone("#777777") } }
    }
    Text {
        id: heading; x: header.back ? 36 : 0; y: 18; height: 34; width: Math.min(425, implicitWidth)
        text: header.title.toUpperCase() === header.english ? header.english : header.title; color: Appearance.tone("#F4F4F4"); font.family: harmonyBold.name; font.pixelSize: 26; font.weight: Font.Bold
        elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; renderType: Text.CurveRendering
    }
    Text { x: heading.x + heading.width + 12; y: 21; height: 29; visible: header.title.toUpperCase() !== header.english && x + implicitWidth < 590; text: header.english; color: Appearance.tone("#8A8A8A"); font.family: harmonyMedium.name; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter; renderType: Text.CurveRendering }
    Text { visible: header.back; y: 18; width: 26; height: 30; text: "‹"; color: Appearance.tone("#F4F4F4"); font.pixelSize: 28; horizontalAlignment: Text.AlignHCenter; renderType: Text.CurveRendering; MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: header.backRequested() } }
    Rectangle { y: 64; width: 590; height: 1; color: Appearance.tone("#38FFFFFF") }
    Rectangle { y: 63; width: 56; height: 3; color: Appearance.accent("#FFFA00") }
    Repeater { model: 11; Rectangle { required property int index; x: 454 + index * 9; y: 60; width: 1; height: 9; color: Appearance.tone("#59FFFFFF") } }
    Rectangle { x: 590 - width; y: 58; width: Math.max(27, counter.implicitWidth + 8); height: 12; color: "#151515"
        Text { id: counter; anchors.centerIn: parent; text: header.count; color: Appearance.accent("#FFFA00"); font.family: harmonyBold.name; font.pixelSize: 11; renderType: Text.CurveRendering }
    }
}
