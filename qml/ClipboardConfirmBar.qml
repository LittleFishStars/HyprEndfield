import QtQuick

// EndfieldHUD HUDConfirmBar, standard danger request.
Item {
    id: confirm
    property string title: ""
    property string message: ""
    property string confirmTitle: "清空"
    property bool reduceMotion: false
    signal accepted()
    signal cancelled()
    width: 620; height: 112
    opacity: visible ? 1 : 0
    transform: Scale { origin.x: confirm.width / 2; origin.y: confirm.height / 2; yScale: confirm.visible ? 1 : 0.04 }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "#00181818" }
            GradientStop { position: 0.06; color: "#F7181818" }
            GradientStop { position: 0.94; color: "#F7181818" }
            GradientStop { position: 1; color: "#00181818" }
        }
    }
    Rectangle { x: 31; width: 558; height: 1; color: "#38FFFFFF" }
    Rectangle { x: 31; y: 111; width: 558; height: 1; color: "#38FFFFFF" }
    Item {
        x: 46; y: 18; width: 370; height: 80
        Rectangle { x: 0; y: 1.5; width: 4; height: 18; color: "#E5383B" }
        Rectangle { x: 8; y: 1.5; width: 4; height: 18; color: "#E5383B" }
        Text {
            x: 22; width: 348; height: 22; text: confirm.title; color: "#F2F2F2"; elide: Text.ElideRight
            font.family: harmonyMedium.name; font.pixelSize: 17; font.weight: Font.Medium; renderType: Text.CurveRendering
        }
        Text {
            x: 0; y: 29; width: 370; height: 36; text: confirm.message; color: "#9A9A9A"
            maximumLineCount: 2; elide: Text.ElideRight; font.family: harmonyRegular.name; font.pixelSize: 12
            lineHeight: 1.5; renderType: Text.CurveRendering
        }
    }
    Column {
        x: 436; y: 18; spacing: 11
        Repeater {
            model: [{title: "取消", danger: false, glyph: "x"}, {title: confirm.confirmTitle, danger: true, glyph: "trash"}]
            delegate: Rectangle {
                required property var modelData
                width: 150; height: 38; radius: 19
                color: mouse.containsMouse ? (modelData.danger ? "#FFFFFF" : "#606060") : (modelData.danger ? "#E2E0E1" : "#4A4A4A")
                border.width: modelData.danger ? 1 : 0; border.color: "#E5383B"
                Rectangle {
                    x: parent.width - 33; y: 5; width: 28; height: 28; radius: 14
                    color: modelData.danger ? "#E5383B" : "#EDEDED"
                    ClipboardGlyph { anchors.centerIn: parent; width: 13; height: 13; kind: modelData.glyph; ink: modelData.danger ? "white" : "#2A2A2A" }
                }
                Text {
                    x: 18; width: 92; height: parent.height; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignRight
                    text: modelData.title; color: modelData.danger ? "#161616" : "#ECECEC"
                    font.family: harmonyBold.name; font.pixelSize: 12; font.weight: Font.Bold; renderType: Text.CurveRendering
                }
                MouseArea {
                    id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: modelData.danger ? confirm.accepted() : confirm.cancelled()
                }
            }
        }
    }
    Behavior on opacity {
        NumberAnimation {
            duration: confirm.reduceMotion ? 0 : (confirm.visible ? 300 : 180)
            easing.type: Easing.BezierSpline
            easing.bezierCurve: confirm.visible ? [0.2, 0.9, 0.1, 1, 1, 1] : [0.42, 0, 1, 1, 1, 1]
        }
    }
}
