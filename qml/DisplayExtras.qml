import QtQuick
import QtQuick.Shapes
import QtQuick.Controls
import "Appearance.js" as Appearance

Item {
    id: root
    required property var settings
    property string section: "battery"
    property string openMenu: ""
    readonly property var metrics: [qsTr("电池"), "RAM", "CPU", qsTr("网络"), qsTr("磁盘"), "GPU"]
    readonly property var methods: [qsTr("充电状态变化"), qsTr("始终显示"), qsTr("不显示")]
    readonly property var placements: [qsTr("顶部居中"), qsTr("自定")]
    signal backRequested()
    width: 590; height: 510
    onVisibleChanged: openMenu = ""
    SettingsHeader {
        title: root.section === "battery" ? qsTr("电池提醒") : qsTr("应用 / 菜单栏图标")
        english: root.section === "battery" ? "BATTERY ALERT" : "APP / MENU BAR ICON"
        crumbs: "DISPLAY / " + (root.section === "battery" ? "BATTERY ALERT" : "APP ICON")
        code: "SYS-02"; count: root.section === "battery" ? "06" : String(root.settings.icons.length).padStart(2, "0")
        back: true; onBackRequested: root.backRequested()
    }
    Item {
        y: 81; width: 590; height: 429; visible: root.section === "battery"
        HudSettingsRow { title: qsTr("显示数据"); mode: "dropdown"; value: root.metrics[root.settings.batteryMetric]; onActivated: root.openMenu = root.openMenu === "metric" ? "" : "metric" }
        HudSettingsRow { y: 56; title: qsTr("显示方式"); mode: "dropdown"; value: root.methods[root.settings.batteryMethod]; onActivated: root.openMenu = root.openMenu === "method" ? "" : "method" }
        HudSettingsRow { y: 112; title: qsTr("显示时长"); mode: "slider"; sliderMinimum: 1; sliderMaximum: 60; sliderStep: 1; sliderFormat: "seconds"; sliderValue: root.settings.batteryDuration; onSliderMoved: (value, final) => root.settings.set("batteryDuration", value) }
        HudSettingsRow { y: 168; title: qsTr("显示位置"); mode: "dropdown"; value: root.placements[root.settings.batteryPlacement]; onActivated: root.openMenu = root.openMenu === "placement" ? "" : "placement" }
        HudSettingsRow { y: 224; title: qsTr("编辑位置"); mode: "nav"; onActivated: root.settings.editBatteryPosition() }
        HudSettingsRow { y: 280; title: qsTr("大小"); mode: "slider"; sliderMinimum: 65; sliderMaximum: 160; sliderStep: 1; sliderValue: root.settings.batteryScale * 100; onSliderMoved: (value, final) => root.settings.set("batteryScale", value / 100) }
    }
    Flickable {
        id: icons; y: 81; width: 590; height: 429; visible: root.section === "appIcon"; clip: true; boundsBehavior: Flickable.StopAtBounds
        contentHeight: Math.ceil(root.settings.icons.length / 4) * 106 - 10
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
        Repeater {
            model: root.settings.icons
            delegate: Item {
                id: tile; required property var modelData; required property int index
                x: (index % 4) * 147.5; y: Math.floor(index / 4) * 106; width: 137.5; height: 96
                readonly property bool selected: root.settings.applicationIcon === modelData.id
                Shape {
                    x: 4; y: pointer.containsMouse ? 2 : 4; width: 129.5; height: 88; preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        strokeWidth: -1
                        fillGradient: LinearGradient { x1: 0; y1: 0; x2: 0; y2: 88; GradientStop { position: 0; color: Appearance.tone("#2E2E2E") } GradientStop { position: 1; color: Appearance.tone("#1C1C1C") } }
                        PathSvg { path: "M0 0H120.5L129.5 9V88H0Z" }
                    }
                    Behavior on y { NumberAnimation { duration: root.settings.reduceMotion ? 0 : 220; easing.type: Easing.BezierSpline; easing.bezierCurve: [.25, .1, .25, 1, 1, 1] } }
                }
                Image { x: (parent.width - 60) / 2; y: pointer.containsMouse ? 15 : 18; width: 60; height: 60; source: modelData.source; fillMode: Image.PreserveAspectFit; mipmap: true; Behavior on y { NumberAnimation { duration: root.settings.reduceMotion ? 0 : 300 } } }
                Shape {
                    visible: tile.selected; anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
                    ShapePath { fillColor: "transparent"; strokeColor: Appearance.accent("#FFFA00"); strokeWidth: 2; PathSvg { path: "M9 0H0V96H9M128.5 0H137.5V96H128.5" } }
                }
                Accessible.name: modelData.title; Accessible.role: Accessible.Button
                MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.settings.set("applicationIcon", tile.modelData.id) }
                ToolTip.visible: pointer.containsMouse; ToolTip.text: modelData.title; ToolTip.delay: 600
            }
        }
    }
    MouseArea { anchors.fill: parent; z: 20; visible: root.openMenu !== ""; onClicked: root.openMenu = "" }
    SettingsMenu {
        x: 400; y: root.openMenu === "metric" ? 125 : root.openMenu === "method" ? 181 : 293; z: 21; visible: root.openMenu !== ""
        entries: root.openMenu === "metric" ? root.metrics : root.openMenu === "method" ? root.methods : root.placements
        selected: root.openMenu === "metric" ? root.settings.batteryMetric : root.openMenu === "method" ? root.settings.batteryMethod : root.settings.batteryPlacement
        onChosen: index => { root.settings.set(root.openMenu === "metric" ? "batteryMetric" : root.openMenu === "method" ? "batteryMethod" : "batteryPlacement", index); root.openMenu = "" }
    }
}
