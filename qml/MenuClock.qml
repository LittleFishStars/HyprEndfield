import QtQuick

// EndfieldHUD/SystemHUDView.swift statusPanel + HUDClockStyle.swift digital.
Item {
    id: clock
    required property string timeText
    required property string dateText
    property int seconds: 0
    width: 340
    height: 145
    Rectangle {
        x: .8
        y: .8
        width: 338.4
        height: 120.4
        radius: 17
        color: "transparent"
        border.width: 1.5
        border.color: "#fad41f"
        antialiasing: true
    }
    Rectangle {
        x: 7
        y: 7
        width: 326
        height: 108
        radius: 12
        color: "#c70e0e0e"
        border.width: 1
        border.color: "#59b3b3b3"
        antialiasing: true
    }
    Text {
        x: 22
        y: 10
        width: 296
        height: 58
        text: clock.timeText
        color: "#f5f5f5"
        font.family: harmonyBold.name
        font.pixelSize: 50
        fontSizeMode: Text.Fit
        minimumPixelSize: 32
        renderType: Text.CurveRendering
    }
    Text {
        x: 22
        y: 65
        width: 120
        height: 18
        text: clock.dateText
        color: "#b3b3b3"
        font.family: harmonyMedium.name
        font.pixelSize: 13
        renderType: Text.CurveRendering
    }
    Repeater {
        model: 61
        Rectangle {
            required property int index
            x: 146 + 172 * index / 60 - .5
            y: 80 - height
            width: 1
            height: index % 15 === 0 ? 9 : index % 5 === 0 ? 6 : 3
            color: index <= clock.seconds ? "#fad41f" : "#404040"
        }
    }
    Rectangle {
        x: 22
        y: 89
        width: 296
        height: 1
        color: "#404040"
    }
    Text {
        x: 22
        y: 94
        text: "WORK MODE"
        color: "#737373"
        font.family: harmonyMedium.name
        font.pixelSize: 12
        renderType: Text.CurveRendering
    }
    Repeater {
        model: 5
        Rectangle {
            required property int index
            x: 14 + index * 64
            y: 129.5
            width: 49
            height: 3
            color: index === 0 ? "#fad41f" : "#777777"
        }
    }
}
