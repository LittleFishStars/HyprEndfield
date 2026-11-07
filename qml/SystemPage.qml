import QtQuick
import QtQuick.Window

Item {
    id: root

    property int languageIndex: 0
    property int displayIndex: 0
    property bool launchAtLogin: true
    property bool closeOnFocusLost: true
    property string openMenu: ""
    property var languageOptions: ["跟随系统", "English", "简体中文", "繁體中文", "日本語", "한국어"]
    property var displayOptions: ["鼠标所在显示器", "主显示器", Screen.name]
    readonly property string languageValue: languageOptions[languageIndex]
    readonly property string displayValue: displayOptions[displayIndex]

    signal modulesRequested

    width: 590
    height: 510

    function resetSettings() {
        languageIndex = 0;
        displayIndex = 0;
        launchAtLogin = true;
        closeOnFocusLost = true;
        openMenu = "";
    }

    Text {
        renderType: Text.CurveRendering
        x: 0
        y: 0
        height: 14
        text: "// HUD  /  "
        color: "#8A8A8A"
        font.family: harmonyRegular.name
        font.pixelSize: 11
        font.letterSpacing: 0.06
        verticalAlignment: Text.AlignVCenter
    }

    Text {
        renderType: Text.CurveRendering
        x: 69
        y: 0
        height: 14
        text: "SYSTEM"
        color: "#E6E6E6"
        font.family: harmonyRegular.name
        font.pixelSize: 11
        font.letterSpacing: 0.06
        verticalAlignment: Text.AlignVCenter
    }

    Text {
        renderType: Text.CurveRendering
        x: 500
        y: 0
        width: 49
        height: 14
        text: "SYS-01"
        color: "#8A8A8A"
        font.family: harmonyRegular.name
        font.pixelSize: 11
        font.letterSpacing: 0.06
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
    }

    Item {
        x: 556
        y: 3.5
        width: 34
        height: 7
        clip: true

        Repeater {
            model: 12

            Rectangle {
                required property int index
                x: index * 3.5 - 7
                y: -5
                width: 1
                height: 18
                rotation: 45
                color: "#777777"
            }
        }
    }

    Text {
        renderType: Text.CurveRendering
        x: 0
        y: 18
        width: 140
        height: 34
        text: "系统设置"
        color: "#F4F4F4"
        font.family: harmonyBold.name
        font.pixelSize: 26
        font.weight: Font.Bold
        font.letterSpacing: 0.08
        verticalAlignment: Text.AlignVCenter
    }

    Text {
        renderType: Text.CurveRendering
        x: 151
        y: 21
        width: 100
        height: 29
        text: "SYSTEM"
        color: "#8A8A8A"
        font.family: harmonyMedium.name
        font.pixelSize: 13
        font.weight: Font.Medium
        font.letterSpacing: 0.2
        verticalAlignment: Text.AlignVCenter
    }

    Rectangle {
        x: 0
        y: 64
        width: 590
        height: 1
        color: "#38FFFFFF"
    }

    Rectangle {
        x: 0
        y: 63
        width: 56
        height: 3
        color: "#FFFA00"
    }

    Repeater {
        model: 11

        Rectangle {
            required property int index
            x: 454 + index * 9
            y: 60
            width: 1
            height: 9
            color: "#59FFFFFF"
        }
    }

    Rectangle {
        x: 563
        y: 58
        width: 27
        height: 12
        color: "#151515"

        Text {
            renderType: Text.CurveRendering
            anchors.fill: parent
            text: "06"
            color: "#FFFA00"
            font.family: harmonyBold.name
            font.pixelSize: 11
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    HudSettingsRow {
        x: 0
        y: 81
        title: "语言"
        mode: "dropdown"
        value: root.languageValue
        dropdownOpened: root.openMenu === "language"
        onActivated: root.openMenu = root.openMenu === "language" ? "" : "language"
    }

    HudSettingsRow {
        x: 0
        y: 137
        title: "登录时启动"
        mode: "toggle"
        switched: root.launchAtLogin
        onToggleRequested: value => root.launchAtLogin = value
    }

    HudSettingsRow {
        x: 0
        y: 193
        title: "失去焦点时关闭"
        mode: "toggle"
        switched: root.closeOnFocusLost
        onToggleRequested: value => root.closeOnFocusLost = value
    }

    HudSettingsRow {
        x: 0
        y: 249
        title: "显示器"
        mode: "dropdown"
        value: root.displayValue
        dropdownOpened: root.openMenu === "display"
        onActivated: root.openMenu = root.openMenu === "display" ? "" : "display"
    }

    HudSettingsRow {
        x: 0
        y: 305
        title: "模块"
        mode: "nav"
        chipText: "12 / 12"
        onActivated: root.modulesRequested()
    }

    HudSettingsRow {
        x: 0
        y: 361
        title: "恢复默认设置"
        mode: "button"
        onActivated: root.resetSettings()
    }

    MouseArea {
        anchors.fill: parent
        visible: root.openMenu.length > 0
        z: 20
        onClicked: root.openMenu = ""
    }

    Item {
        id: languageMenu
        visible: root.openMenu === "language"
        x: 400
        y: 135
        width: 178
        height: root.languageOptions.length * 28 + 8
        z: 21

        Rectangle {
            anchors.fill: parent
            color: "#1A1A1A"
            border.width: 1
            border.color: "#3D3D3D"
        }

        Rectangle {
            x: 0
            y: 0
            width: 10
            height: 2
            color: "#D7BC18"
        }
        Rectangle {
            x: 0
            y: 0
            width: 2
            height: 10
            color: "#D7BC18"
        }

        Repeater {
            model: root.languageOptions

            delegate: Item {
                required property int index
                required property string modelData
                x: 4
                y: 4 + index * 28
                width: 170
                height: 28

                Rectangle {
                    anchors.fill: parent
                    color: optionPointer.containsMouse ? "#2B2B2B" : "transparent"
                }

                Rectangle {
                    x: 9
                    y: 10
                    width: 8
                    height: 8
                    color: index === root.languageIndex ? "#FFFA00" : "transparent"
                    border.width: index === root.languageIndex ? 0 : 1
                    border.color: "#555555"
                }

                Text {
                    renderType: Text.CurveRendering
                    x: 28
                    y: 0
                    width: 132
                    height: 28
                    text: modelData
                    color: "#D9D9D9"
                    font.family: harmonyRegular.name
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                MouseArea {
                    id: optionPointer
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.languageIndex = index;
                        root.openMenu = "";
                    }
                }
            }
        }
    }

    Item {
        id: displayMenu
        visible: root.openMenu === "display"
        x: 400
        y: 303
        width: 178
        height: root.displayOptions.length * 28 + 8
        z: 21

        Rectangle {
            anchors.fill: parent
            color: "#1A1A1A"
            border.width: 1
            border.color: "#3D3D3D"
        }

        Rectangle {
            x: 0
            y: 0
            width: 10
            height: 2
            color: "#D7BC18"
        }
        Rectangle {
            x: 0
            y: 0
            width: 2
            height: 10
            color: "#D7BC18"
        }

        Repeater {
            model: root.displayOptions

            delegate: Item {
                required property int index
                required property string modelData
                x: 4
                y: 4 + index * 28
                width: 170
                height: 28

                Rectangle {
                    anchors.fill: parent
                    color: displayPointer.containsMouse ? "#2B2B2B" : "transparent"
                }

                Rectangle {
                    x: 9
                    y: 10
                    width: 8
                    height: 8
                    color: index === root.displayIndex ? "#FFFA00" : "transparent"
                    border.width: index === root.displayIndex ? 0 : 1
                    border.color: "#555555"
                }

                Text {
                    renderType: Text.CurveRendering
                    x: 28
                    y: 0
                    width: 132
                    height: 28
                    text: modelData
                    color: "#D9D9D9"
                    font.family: harmonyRegular.name
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                MouseArea {
                    id: displayPointer
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.displayIndex = index;
                        root.openMenu = "";
                    }
                }
            }
        }
    }
}
