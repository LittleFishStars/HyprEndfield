import QtQuick
import "ModuleNumbering.js" as ModuleNumbering
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Shapes
import "Appearance.js" as Appearance

// EndfieldHUD AppShortcutCanvas, MOD-18: original row and icon-grid dimensions.
Item {
    id: root
    width: 590; height: 510
    property bool presented: false
    property bool reduceMotion: false
    property var shortcuts: appShortcutsController.state
    property var presets: appShortcutsController.presets
    property string menuId: ""
    property int menuIndex: 0
    property real menuY: 0
    property var removing: null
    property bool editingName: false
    readonly property var menuEntry: shortcuts.items.find(item => item.id === menuId)
    function imageSource(id, preset) { return "image://shortcut-icons/" + id + "/" + preset + "/" + String(Appearance.accent("#FFFA00")).substring(1) }
    function chooseIcon(index) { if (menuId && presets[index]) appShortcutsController.setIcon(menuId, presets[index].id); menuId = "" }
    function moveIcon(offset) { menuIndex = (menuIndex + offset + presets.length) % presets.length }
    onPresentedChanged: {
        if (presented) appShortcutsController.refresh()
        else { chooser.close(); menuId = ""; removing = null }
    }
    Connections { target: appShortcutsController; function onAdded(id) { Qt.callLater(() => rows.positionViewAtEnd()) } }
    SettingsHeader { title: qsTr("应用快捷方式"); english: "SHORTCUTS"; code: ModuleNumbering.code("addApp"); count: String(root.shortcuts.count).padStart(2, "0") }
    CalendarButton { x: 490; y: 22; width: 100; title: qsTr("添加应用"); enabled: root.shortcuts.available; onClicked: chooser.open(); reduceMotion: root.reduceMotion }
    ListView {
        id: rows; objectName: "shortcutRows"; x: 0; y: 81; width: 590; height: 420
        model: root.shortcuts.items; spacing: 6; clip: true; boundsBehavior: Flickable.StopAtBounds
        delegate: Item {
            id: row; required property var modelData; required property int index
            width: rows.contentHeight > rows.height ? 580 : 590; height: 56
            Shape {
                anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
                ShapePath { strokeWidth: -1
                    fillGradient: LinearGradient { x1: 0; y1: 0; x2: 0; y2: 56
                        GradientStop { position: 0; color: "#E2E3DE" }
                        GradientStop { position: 1; color: "#CFD0CB" }
                    }
                    PathSvg { path: "M0 0H" + (row.width - 9) + "L" + row.width + " 9V56H0Z" }
                }
            }
            Image {
                x: 16; y: 9; width: 40; height: 40; mipmap: true; sourceSize: Qt.size(120, 120)
                source: root.imageSource(row.modelData.id, row.modelData.iconPreset)
                transform: Translate { y: iconMouse.containsMouse ? -1 : 0 }
                Rectangle { anchors.fill: parent; color: "#12FFFFFF"; visible: iconMouse.containsMouse }
                MouseArea { id: iconMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.menuId === row.modelData.id) { root.menuId = ""; return }
                        root.menuId = row.modelData.id
                        root.menuIndex = root.presets.findIndex(p => p.id === row.modelData.iconPreset)
                        const top = 81 + row.y - rows.contentY + 9
                        root.menuY = top + 44 + 148 > 530 ? top - 152 : top + 44
                        root.forceActiveFocus()
                    }
                }
            }
            Rectangle { x: 70; y: 14; width: row.width - 156; height: 30
                color: nameInput.activeFocus ? "#F1F1EC" : "#0D000000"; border.color: nameInput.activeFocus ? "#343434" : "#33000000"
                Rectangle { width: 2; height: 30; color: "#1A1A1A"; visible: nameInput.activeFocus }
                TextInput {
                    id: nameInput; objectName: "shortcutName_" + row.index
                    anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 6
                    maximumLength: 24; selectByMouse: true; clip: true; verticalAlignment: TextInput.AlignVCenter
                    color: "#111111"; selectionColor: Appearance.accent("#FFFA00"); selectedTextColor: "#111111"
                    font.family: harmonyMedium.name; font.pixelSize: 13; renderType: Text.CurveRendering
                    Binding { target: nameInput; property: "text"; value: row.modelData.name; when: !nameInput.activeFocus; restoreMode: Binding.RestoreNone }
                    onActiveFocusChanged: root.editingName = activeFocus
                    onEditingFinished: { if (text.trim()) appShortcutsController.rename(row.modelData.id, text); else text = row.modelData.name }
                    Keys.onEscapePressed: event => { text = row.modelData.name; root.forceActiveFocus(); event.accepted = true }
                    Keys.onReturnPressed: root.forceActiveFocus()
                }
            }
            Rectangle {
                x: row.width - 78; y: 14; width: 30; height: 30; color: launchMouse.containsMouse ? "#ECEDE7" : "#21000000"; border.color: "#55000000"
                SourceGlyph { anchors.centerIn: parent; width: 14; height: 14; kind: "ext"; ink: "#222222" }
                MouseArea { id: launchMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; enabled: !root.shortcuts.launching; onClicked: appShortcutsController.requestOpen(row.modelData.id) }
            }
            CalendarButton { x: row.width - 40; y: 14; width: 30; height: 30; kind: "icon"; glyph: "×"; reduceMotion: root.reduceMotion; onClicked: { root.menuId = ""; root.removing = JSON.parse(JSON.stringify(row.modelData)) } }
        }
        ScrollBar.vertical: ScrollBar {
            id: bar; width: 4; policy: ScrollBar.AsNeeded; active: true
            contentItem: Rectangle { implicitWidth: 4; color: Appearance.accent("#FFFA00") }
            background: Rectangle { color: "#222222"; visible: bar.size < 1 }
        }
    }
    Text { x: 0; y: 185; width: 590; text: "NO SHORTCUTS"; visible: root.shortcuts.count === 0; color: Appearance.tone("#777777"); horizontalAlignment: Text.AlignHCenter; font.family: harmonyMedium.name; font.pixelSize: 12; font.letterSpacing: 1.2; renderType: Text.CurveRendering }
    DropArea {
        id: dropArea; x: 0; y: 81; width: 590; height: 420
        onEntered: drag => drag.accepted = drag.hasUrls && drag.urls.length === 1 && String(drag.urls[0]).toLowerCase().endsWith(".desktop")
        onDropped: drop => { if (drop.urls.length === 1) appShortcutsController.add(drop.urls[0]); drop.accepted = true }
        Rectangle { anchors.fill: parent; visible: dropArea.containsDrag; color: "transparent"; border.color: Appearance.accent("#FFFA00"); Rectangle { width: 3; height: parent.height; color: Appearance.accent("#FFFA00") } }
    }
    Text { x: 0; y: 506; width: 590; text: root.shortcuts.error; color: "#E6B56B"; font.family: harmonyRegular.name; font.pixelSize: 11; wrapMode: Text.Wrap; renderType: Text.CurveRendering }
    FileDialog {
        id: chooser; title: qsTr("选择应用"); nameFilters: [qsTr("应用快捷方式 (*.desktop)")]; currentFolder: "file:///usr/share/applications"
        onAccepted: appShortcutsController.add(selectedFile)
    }
    Shortcut { sequence: "Ctrl+V"; enabled: root.presented && !root.editingName && !root.menuId && !root.removing; onActivated: appShortcutsController.paste() }
    Shortcut { sequence: "Escape"; enabled: root.presented && (!!root.menuId || !!root.removing); onActivated: { root.menuId = ""; root.removing = null } }
    Shortcut { sequence: "Left"; enabled: root.presented && !!root.menuId; onActivated: root.moveIcon(-1) }
    Shortcut { sequence: "Right"; enabled: root.presented && !!root.menuId; onActivated: root.moveIcon(1) }
    Shortcut { sequence: "Up"; enabled: root.presented && !!root.menuId; onActivated: root.moveIcon(-5) }
    Shortcut { sequence: "Down"; enabled: root.presented && !!root.menuId; onActivated: root.moveIcon(5) }
    Shortcut { sequence: "Return"; enabled: root.presented && !!root.menuId; onActivated: root.chooseIcon(root.menuIndex) }
    MouseArea { anchors.fill: parent; visible: !!root.menuId; z: 90; onClicked: root.menuId = "" }
    Rectangle {
        id: iconMenu; objectName: "shortcutIconMenu"; x: 16; y: root.menuY; width: 240; height: 148; z: 91
        visible: !!root.menuId; color: "#1A1A1A"; border.color: "#555555"
        Rectangle { x: -1; y: -1; width: 10; height: 2; color: Appearance.accent("#FFFA00") }
        Rectangle { x: -1; y: -1; width: 2; height: 10; color: Appearance.accent("#FFFA00") }
        Repeater { model: root.presets
            Image {
                id: presetTile; required property var modelData; required property int index
                x: 8 + index % 5 * 46; y: 8 + Math.floor(index / 5) * 46; width: 40; height: 40; mipmap: true; sourceSize: Qt.size(120, 120)
                source: root.menuId ? root.imageSource(root.menuId, modelData.id) : ""
                readonly property bool selected: !!root.menuEntry && root.menuEntry.iconPreset === modelData.id
                Rectangle { x: -3; y: -3; width: 46; height: 46; color: "transparent"; border.width: 1.5; border.color: presetTile.selected ? Appearance.accent("#FFFA00") : "#BBBBBB"; visible: presetTile.selected || root.menuIndex === presetTile.index }
                ToolTip.visible: tileMouse.containsMouse
                ToolTip.text: modelData.title
                MouseArea { id: tileMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onEntered: root.menuIndex = presetTile.index; onClicked: root.chooseIcon(presetTile.index) }
            }
        }
    }
    Item { anchors.fill: parent; visible: !!root.removing; z: 100
        Rectangle { anchors.fill: parent; color: "#99000000" }
        MouseArea { anchors.fill: parent; onClicked: root.removing = null }
        ClipboardConfirmBar { x: -15; y: 199; title: qsTr("移除 %1？").arg(root.removing ? root.removing.name : ""); message: qsTr("应用本身不会被卸载。"); confirmTitle: qsTr("移除"); reduceMotion: root.reduceMotion
            onAccepted: { appShortcutsController.remove(root.removing.id); root.removing = null }
            onCancelled: root.removing = null
        }
    }
}
