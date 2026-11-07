import QtQuick
import "Appearance.js" as Appearance

Item {
    id: tabs
    property int currentIndex: 0
    property bool reduceMotion: false
    property var entries: [
        { title: qsTr("打开"), icon: "open" }, { title: qsTr("书库"), icon: "apps" },
        { title: qsTr("阅读"), icon: "text" }, { title: qsTr("书签"), icon: "bookmark" }
    ]
    signal selected(int index, int direction)
    width: 590; height: 40

    Rectangle { x: 0; y: 0; width: 590; height: 34; color: Appearance.tone("#191919"); border.color: Appearance.tone("#434343") }
    Repeater {
        model: ["Q", "E"]
        delegate: Rectangle {
            required property string modelData; required property int index
            x: index === 0 ? 6.5 : tabs.width - 21.5; y: 9.5; width: 15; height: 15
            color: "transparent"; border.color: Appearance.tone("#6A6A6A")
            Text { anchors.centerIn: parent; text: parent.modelData; color: Appearance.tone("#8A8A8A"); font.family: harmonyMedium.name; font.pixelSize: 9; font.weight: Font.Medium; renderType: Text.CurveRendering }
        }
    }
    Repeater {
        model: tabs.entries
        delegate: Item {
            id: tab
            required property var modelData; required property int index
            readonly property bool active: index === tabs.currentIndex
            x: 28 + index * 133.5; y: active ? -3 : 0; width: 133.5; height: active ? 37 : 34
            z: active ? 2 : 1
            Rectangle {
                anchors.fill: parent
                visible: tab.active
                gradient: Gradient {
                    GradientStop { position: 0; color: "#FFFF58" }
                    GradientStop { position: 0.45; color: Appearance.accent("#FFFA00") }
                    GradientStop { position: 1; color: Appearance.accent("#FFFA00") }
                }
            }
            Rectangle { anchors.fill: parent; color: "#0FFFFFFF"; opacity: hover.hovered && !tab.active ? 1 : 0; Behavior on opacity { NumberAnimation { duration: tabs.reduceMotion ? 0 : 120 } } }
            Row {
                anchors.centerIn: parent; spacing: 7
                ReaderGlyph { width: 14; height: 14; name: tab.modelData.icon; color: tab.active ? Appearance.tone("#1B1B1B") : Appearance.tone("#B6B6B6") }
                Text { text: tab.modelData.title; color: tab.active ? Appearance.tone("#1B1B1B") : hover.hovered ? Appearance.tone("#FFFFFF") : Appearance.tone("#B6B6B6"); font.family: tab.active ? harmonyBold.name : harmonyMedium.name; font.pixelSize: 13; font.weight: tab.active ? Font.Bold : Font.Medium; renderType: Text.CurveRendering }
            }
            Rectangle { visible: tab.active; x: width * 0.32; y: height + 3; width: width * 0.36; height: 3; color: "#252000" }
            HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: if (!tab.active) tabs.selected(tab.index, tab.index > tabs.currentIndex ? 1 : -1) }
            Behavior on y { NumberAnimation { duration: tabs.reduceMotion ? 0 : 120; easing.type: Easing.OutCubic } }
        }
    }
    Repeater {
        model: 3
        delegate: Rectangle {
            required property int index
            readonly property int separator: index + 1
            visible: separator !== tabs.currentIndex && separator - 1 !== tabs.currentIndex
            x: 28 + separator * 133.5; y: 10; width: 1; height: 14; color: Appearance.tone("#4A4A4A"); z: 1
        }
    }
}
