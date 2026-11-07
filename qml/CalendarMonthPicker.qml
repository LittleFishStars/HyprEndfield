import QtQuick
import "Appearance.js" as Appearance

Item {
    id: picker
    property int year: 2026
    property int shownYear: 2026
    property int shownMonth: 1
    property string today: ""
    property bool reduceMotion: false
    signal picked(int year, int month)
    signal closed()
    width: 270; height: 190

    Rectangle { anchors.fill: parent; color: Appearance.tone("#181818"); border.color: Appearance.tone("#484848") }
    Rectangle { width: parent.width; height: 3; color: Appearance.accent("#FFFA00") }
    Text {
        x: 12; y: 10; text: qsTr("选择月份"); color: Appearance.tone("#F2F2F2")
        font.family: harmonyBold.name; font.pixelSize: 15; font.weight: Font.Bold; renderType: Text.CurveRendering
    }
    Text {
        x: 93; y: 13; text: "SELECT MONTH"; color: Appearance.tone("#777777")
        font.family: harmonyMedium.name; font.pixelSize: 9; font.letterSpacing: 1.3; renderType: Text.CurveRendering
    }
    Item {
        x: 12; y: 38; width: 246; height: 30
        CalendarButton { width: 30; kind: "icon"; glyph: "‹"; enabled: picker.year > 1900; reduceMotion: picker.reduceMotion; onClicked: --picker.year }
        Text {
            x: 30; width: 186; height: 30; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter
            text: picker.year; color: Appearance.tone("#EDEDED"); font.family: harmonyMedium.name; font.pixelSize: 17; font.features: { "tnum": 1 }; renderType: Text.CurveRendering
        }
        CalendarButton { x: 216; width: 30; kind: "icon"; glyph: "›"; enabled: picker.year < 9999; reduceMotion: picker.reduceMotion; onClicked: ++picker.year }
    }
    Grid {
        x: 12; y: 78; columns: 4; columnSpacing: 4; rowSpacing: 4
        Repeater {
            model: 12
            CalendarDayCell {
                required property int index
                width: 58.5; height: 32; value: String(index + 1) + qsTr("月")
                today: {
                    const p = picker.today.split("-")
                    return Number(p[0]) === picker.year && Number(p[1]) === index + 1
                }
                selected: picker.shownYear === picker.year && picker.shownMonth === index + 1
                reduceMotion: picker.reduceMotion
                onClicked: picker.picked(picker.year, index + 1)
            }
        }
    }
    MouseArea { anchors.fill: parent; z: -1 }
}
