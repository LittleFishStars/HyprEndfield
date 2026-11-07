import QtQuick
import QtQuick.Controls
import "Appearance.js" as Appearance

Item {
    id: root
    property var settings: settingsController
    property string section: "main"
    property string openMenu: ""
    property bool confirming: false
    property string draggedModule: ""
    property real draggedY: 0
    property var previewOrder: []
    property var languageOptions: [qsTr("跟随系统"), "English", "简体中文", "繁體中文", "日本語", "한국어"]
    readonly property var moduleNames: ({notes: qsTr("便笺"), fileShelf: qsTr("文件暂存架"), clipboard: qsTr("剪贴板"), archive: qsTr("档案库"), nowPlaying: qsTr("当前播放"), volume: qsTr("音量"), reader: qsTr("阅读器"), workMode: qsTr("专注模式"), calendar: qsTr("日历")})
    readonly property var moduleIcons: ({notes: "notes", fileShelf: "file-shelf", clipboard: "clipboard", archive: "archive", reader: "reader", workMode: "work-mode", calendar: "calendar"})
    readonly property string moduleCount: (settings.moduleOrder.length - settings.hiddenModules.length) + " / " + settings.moduleOrder.length
    readonly property int selectedDisplay: settings.displays.findIndex(item => item.id === settings.displayChoice)
    width: 590; height: 510
    onVisibleChanged: { openMenu = ""; confirming = false }
    onConfirmingChanged: if (confirming) forceActiveFocus()
    onOpenMenuChanged: if (openMenu) forceActiveFocus()
    Keys.onEscapePressed: event => { if (confirming || openMenu) { confirming = false; openMenu = ""; event.accepted = true; } else event.accepted = false; }
    SettingsHeader { title: root.section === "main" ? qsTr("系统设置") : qsTr("模块"); english: root.section === "main" ? "SYSTEM" : "MODULES"; crumbs: root.section === "main" ? "SYSTEM" : "SYSTEM / MODULES"; count: root.section === "main" ? "06" : String(root.settings.moduleOrder.length).padStart(2, "0"); back: root.section !== "main"; onBackRequested: root.section = "main" }
    Item {
        visible: root.section === "main"; y: 81; width: 590; height: 429
        HudSettingsRow { objectName: "settingLanguage"; title: qsTr("语言"); mode: "dropdown"; value: root.languageOptions[root.settings.languageIndex]; dropdownOpened: root.openMenu === "language"; onActivated: root.openMenu = root.openMenu === "language" ? "" : "language" }
        HudSettingsRow { objectName: "settingLogin"; y: 56; title: qsTr("登录时启动"); mode: "toggle"; switched: root.settings.launchAtLogin; onToggleRequested: value => root.settings.set("launchAtLogin", value) }
        HudSettingsRow { objectName: "settingFocus"; y: 112; title: qsTr("失去焦点时关闭"); mode: "toggle"; switched: root.settings.closeOnFocusLost; onToggleRequested: value => root.settings.set("closeOnFocusLost", value) }
        HudSettingsRow { objectName: "settingDisplay"; y: 168; title: qsTr("显示器"); mode: "dropdown"; value: root.selectedDisplay >= 0 ? root.settings.displays[root.selectedDisplay].title : "—"; dropdownOpened: root.openMenu === "screen"; onActivated: root.openMenu = root.openMenu === "screen" ? "" : "screen" }
        HudSettingsRow { objectName: "settingModules"; y: 224; title: qsTr("模块"); mode: "nav"; chipText: root.moduleCount; onActivated: root.section = "modules" }
        HudSettingsRow { objectName: "settingRestore"; y: 280; title: qsTr("恢复默认设置"); mode: "button"; onActivated: root.confirming = true }
    }
    Flickable {
        id: modules; objectName: "settingsModuleList"; visible: root.section === "modules"
        y: 81; width: 590; height: 429; contentHeight: root.settings.moduleOrder.length * 56 + 50; clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
        Repeater {
            model: Object.keys(root.moduleNames)
            delegate: HudSettingsRow {
                id: moduleRow
                required property int index; required property string modelData
                readonly property int orderIndex: root.settings.moduleOrder.indexOf(modelData)
                objectName: "module-" + modelData
                y: root.draggedModule === modelData ? root.draggedY : (root.previewOrder.length ? root.previewOrder : root.settings.moduleOrder).indexOf(modelData) * 56
                z: root.draggedModule === modelData ? 10 : 0
                Behavior on y { enabled: root.draggedModule !== moduleRow.modelData; NumberAnimation { duration: root.settings.reduceMotion ? 0 : 240; easing.type: Easing.BezierSpline; easing.bezierCurve: [.2, .9, .1, 1, 1, 1] } }
                width: 580; title: root.moduleNames[modelData] || modelData; titleInset: 74; mode: "toggle"
                switched: !root.settings.hiddenModules.includes(modelData)
                toggleEnabled: !switched || root.settings.hiddenModules.length < root.settings.moduleOrder.length - 1
                onToggleRequested: value => root.settings.showModule(modelData, value)
                Grid { x: 22; y: 22; columns: 2; rowSpacing: 2; columnSpacing: 2; Repeater { model: 6; Rectangle { width: 2; height: 2; radius: 1; color: "#333333" } } }
                Image { x: 42; y: 15; width: 24; height: 24; visible: !!root.moduleIcons[moduleRow.modelData]; source: visible ? "qrc:/assets/icons/" + root.moduleIcons[moduleRow.modelData] + ".png" : ""; fillMode: Image.PreserveAspectFit; mipmap: true; opacity: moduleRow.switched ? 1 : .35 }
                HudGlyph { x: 42; y: 15; width: 24; height: 24; visible: !root.moduleIcons[moduleRow.modelData]; kind: moduleRow.modelData; ink: "#202020"; opacity: moduleRow.switched ? 1 : .35 }
                MouseArea {
                    x: 8; width: 36; height: 50; cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    preventStealing: true
                    property real grabY: 0
                    onPressed: mouse => { grabY = mouse.y; root.previewOrder = [...root.settings.moduleOrder]; root.draggedY = moduleRow.y; root.draggedModule = moduleRow.modelData; }
                    onPositionChanged: mouse => {
                        if (!pressed) return;
                        const point = mapToItem(modules, mouse.x, mouse.y);
                        if (point.y < 36) modules.contentY = Math.max(0, modules.contentY - 10);
                        if (point.y > modules.height - 36) modules.contentY = Math.min(modules.contentHeight - modules.height, modules.contentY + 10);
                        root.draggedY = Math.max(0, Math.min((root.settings.moduleOrder.length - 1) * 56, mapToItem(modules.contentItem, mouse.x, mouse.y).y - grabY));
                        let order = [...root.previewOrder]; order.splice(order.indexOf(moduleRow.modelData), 1); order.splice(Math.round(root.draggedY / 56), 0, moduleRow.modelData); root.previewOrder = order;
                    }
                    onReleased: mouse => {
                        root.settings.moveModule(moduleRow.modelData, root.previewOrder.indexOf(moduleRow.modelData)); root.draggedModule = ""; root.previewOrder = [];
                    }
                    onCanceled: { root.draggedModule = ""; root.previewOrder = []; }
                }
                Repeater {
                    model: [-1, 1]
                    Rectangle {
                        required property int index; required property int modelData
                        x: 414 + index * 32; y: 12; width: 26; height: 26; color: pointer.containsMouse ? "#ECEDE8" : "#CDCEC9"
                        enabled: moduleRow.orderIndex + modelData >= 0 && moduleRow.orderIndex + modelData < root.settings.moduleOrder.length; opacity: enabled ? 1 : .35
                        Text { anchors.centerIn: parent; text: modelData < 0 ? "⌃" : "⌄"; color: "#111111"; font.pixelSize: 18; renderType: Text.CurveRendering }
                        MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.settings.moveModule(moduleRow.modelData, moduleRow.orderIndex + modelData) }
                    }
                }
            }
        }
        HudSettingsRow { y: root.settings.moduleOrder.length * 56; width: 580; title: qsTr("恢复默认布局"); mode: "button"; onActivated: root.confirming = true }
    }
    MouseArea { anchors.fill: parent; visible: root.openMenu !== ""; z: 20; onClicked: root.openMenu = "" }
    SettingsMenu {
        x: 400; y: root.openMenu === "language" ? 125 : 293; z: 21; visible: root.openMenu !== ""
        entries: root.openMenu === "language" ? root.languageOptions : root.settings.displays
        selected: root.openMenu === "language" ? root.settings.languageIndex : root.selectedDisplay
        onChosen: index => { if (root.openMenu === "language") root.settings.set("languageIndex", index); else root.settings.set("displayChoice", root.settings.displays[index].id); root.openMenu = "" }
    }
    MouseArea { anchors.fill: parent; z: 24; visible: root.confirming; onClicked: root.confirming = false }
    ClipboardConfirmBar {
        z: 25; x: -15; y: 200; visible: root.confirming
        title: root.section === "main" ? qsTr("恢复默认设置？") : qsTr("恢复默认布局？")
        message: root.section === "main" ? qsTr("保留便笺、文件与应用快捷方式。") : qsTr("所有模块按原顺序显示。")
        confirmTitle: qsTr("恢复"); confirmGlyph: "refresh"; accentColor: Appearance.accent("#FFFA00")
        reduceMotion: root.settings.reduceMotion
        onAccepted: { if (root.section === "main") root.settings.restoreDefaults(); else root.settings.resetModules(); root.confirming = false }
        onCancelled: root.confirming = false
    }
}
