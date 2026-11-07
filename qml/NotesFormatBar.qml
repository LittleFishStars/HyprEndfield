import QtQuick

Item {
    id: bar
    property var style: ({ family: "HarmonyOS Sans", size: 14, color: "#EEEEEE", bold: false, italic: false, underline: false, strikeout: false })
    property bool reduceMotion: false
    signal changed(string key, var value)
    width: 570; height: 26; z: 80
    readonly property var families: ["HarmonyOS Sans", "Noto Sans CJK SC", "Noto Sans SC", "Noto Sans", "DejaVu Sans", "Liberation Sans", "Monospace"]
    readonly property var sizes: [9, 10, 11, 12, 13, 14, 16, 18, 20, 24, 28, 32, 36, 48, 64, 72]
    readonly property var colors: ["#EEEEEE", "#FFFA00", "#22A6F2", "#E5383B", "#8CE03C"]
    property string openMenu: ""
    property bool colorPanelOpen: false

    function closeMenus() { openMenu = ""; colorPanelOpen = false }

    Rectangle {
        id: fontButton; x: 0; width: 150; height: 26
        color: fontPointer.containsMouse ? "#303030" : "#262626"
        border.color: bar.openMenu === "font" ? "#FFFA00" : "#4B4B4B"
        Text { x: 9; width: 122; height: 26; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; text: bar.style.family; color: "#EDEDED"; font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering }
        Text { x: 133; height: 26; verticalAlignment: Text.AlignVCenter; text: "▾"; color: "#888888"; font.pixelSize: 10 }
        MouseArea { id: fontPointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { bar.colorPanelOpen = false; bar.openMenu = bar.openMenu === "font" ? "" : "font" } }
    }
    Rectangle {
        id: sizeButton; x: 156; width: 72; height: 26
        color: sizePointer.containsMouse ? "#303030" : "#262626"
        border.color: bar.openMenu === "size" ? "#FFFA00" : "#4B4B4B"
        Text { x: 10; width: 42; height: 26; verticalAlignment: Text.AlignVCenter; text: Math.round(bar.style.size); color: "#EDEDED"; font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering }
        Text { x: 55; height: 26; verticalAlignment: Text.AlignVCenter; text: "▾"; color: "#888888"; font.pixelSize: 10 }
        MouseArea { id: sizePointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { bar.colorPanelOpen = false; bar.openMenu = bar.openMenu === "size" ? "" : "size" } }
    }
    Row {
        x: 234; spacing: 6
        Repeater {
            model: [{key: "bold", title: "B"}, {key: "italic", title: "I"}, {key: "underline", title: "U"}, {key: "strikeout", title: "S"}]
            delegate: Rectangle {
                required property var modelData
                width: 28; height: 26
                color: bar.style[modelData.key] ? "#FFFA00" : traitPointer.containsMouse ? "#353535" : "#262626"
                border.color: "#4B4B4B"
                Text {
                    anchors.centerIn: parent; text: modelData.title
                    color: bar.style[modelData.key] ? "#161616" : "#EEEEEE"
                    font.family: harmonyBold.name; font.pixelSize: 12; font.weight: Font.Bold
                    font.italic: modelData.key === "italic"
                    font.underline: modelData.key === "underline"
                    font.strikeout: modelData.key === "strikeout"
                    renderType: Text.CurveRendering
                }
                MouseArea { id: traitPointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { bar.closeMenus(); bar.changed(modelData.key, !bar.style[modelData.key]) } }
            }
        }
    }
    Row {
        x: 370; y: 5; spacing: 6
        Repeater {
            model: bar.colors
            delegate: Item {
                required property string modelData
                width: 16; height: 16
                Rectangle { anchors.fill: parent; color: modelData; border.width: bar.style.color.toUpperCase() === modelData ? 2 : 1; border.color: bar.style.color.toUpperCase() === modelData ? "#FFFFFF" : "#555555" }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { bar.closeMenus(); bar.changed("color", modelData) } }
            }
        }
        Item {
            width: 16; height: 16
            Rectangle { anchors.fill: parent; color: "#292929"; border.color: bar.colorPanelOpen ? "#FFFA00" : "#777777" }
            Rectangle { x: 3; y: 3; width: 4; height: 4; color: "#F04444" }
            Rectangle { x: 9; y: 3; width: 4; height: 4; color: "#44C4F0" }
            Rectangle { x: 3; y: 9; width: 4; height: 4; color: "#F4D544" }
            Rectangle { x: 9; y: 9; width: 4; height: 4; color: "#A45AF2" }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { bar.openMenu = ""; bar.colorPanelOpen = !bar.colorPanelOpen } }
        }
    }

    Rectangle {
        id: fontMenu; x: 0; y: 30; z: 100; width: 210; height: bar.families.length * 28
        visible: bar.openMenu === "font"; color: "#F51B1B1B"; border.color: "#555555"
        Column {
            anchors.fill: parent
            Repeater {
                model: bar.families
                delegate: Rectangle {
                    required property string modelData; width: 210; height: 28
                    color: menuPointer.containsMouse ? "#343434" : "transparent"
                    Rectangle { width: 3; height: 14; y: 7; color: "#FFFA00"; visible: bar.style.family === modelData }
                    Text { x: 12; width: 190; height: 28; verticalAlignment: Text.AlignVCenter; text: modelData; color: "#EEEEEE"; font.family: modelData; font.pixelSize: 11; elide: Text.ElideRight; renderType: Text.CurveRendering }
                    MouseArea { id: menuPointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { bar.changed("family", modelData); bar.closeMenus() } }
                }
            }
        }
    }
    Rectangle {
        id: sizeMenu; x: 156; y: 30; z: 100; width: 72; height: 196
        visible: bar.openMenu === "size"; color: "#F51B1B1B"; border.color: "#555555"
        ListView {
            anchors.fill: parent; clip: true; model: bar.sizes
            delegate: Rectangle {
                required property real modelData; width: 72; height: 28
                color: sizeRow.containsMouse ? "#343434" : "transparent"
                Rectangle { width: 3; height: 14; y: 7; color: "#FFFA00"; visible: Math.round(bar.style.size) === modelData }
                Text { anchors.centerIn: parent; text: modelData; color: "#EEEEEE"; font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering }
                MouseArea { id: sizeRow; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { bar.changed("size", modelData); bar.closeMenus() } }
            }
        }
    }
    Rectangle {
        id: colorPanel; x: 370; y: 30; z: 100; width: 180; height: 82
        visible: bar.colorPanelOpen; color: "#F51B1B1B"; border.color: "#555555"
        Text { x: 10; y: 8; text: "文字颜色"; color: "#888888"; font.family: harmonyBold.name; font.pixelSize: 9; font.letterSpacing: 1; renderType: Text.CurveRendering }
        Grid {
            x: 10; y: 28; columns: 9; spacing: 4
            Repeater {
                model: ["#FFFFFF", "#CFCFCF", "#FFFA00", "#F4A340", "#E5383B", "#D95CF2", "#7D68F2", "#22A6F2", "#8CE03C",
                        "#A8FFF0", "#7DD3FC", "#93C5FD", "#C4B5FD", "#F0ABFC", "#FDA4AF", "#FDBA74", "#FDE68A", "#BBF7D0"]
                delegate: Rectangle {
                    required property string modelData; width: 14; height: 14; color: modelData; border.color: "#666666"
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { bar.changed("color", modelData); bar.closeMenus() } }
                }
            }
        }
    }
}
