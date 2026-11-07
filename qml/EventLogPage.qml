import QtQuick
import "ModuleNumbering.js" as ModuleNumbering
import QtQuick.Controls
import QtQuick.Shapes
import "Appearance.js" as Appearance

// EndfieldHUD EventLogCanvas: 590 × 510, 40px rows on a 46px pitch.
Item {
    id: root
    width: 590; height: 510
    property bool presented: false
    property bool reduceMotion: false
    property int selectedFilter: 0
    property int refreshToken: 0
    property var entries: []
    property var logState: eventLogController.state
    readonly property var filterTitles: [qsTr("全部"), qsTr("计时"), qsTr("暂存架"), qsTr("设备"), qsTr("其他")]
    function refresh(keepPosition) {
        const offset = Math.max(0, rows.contentY)
        const index = Math.floor(offset / 46)
        const anchor = keepPosition && offset > 0 && entries[index] ? entries[index].id : ""
        // Copy values out of Qt's live QVariant wrappers before retaining an anchor.
        entries = JSON.parse(JSON.stringify(eventLogController.state.entries)).filter(e => selectedFilter === 0 || e.filter === selectedFilter)
        const next = anchor ? entries.findIndex(e => e.id === anchor) : -1
        const targetOffset = next >= 0 ? next * 46 + offset % 46 : 0
        const token = ++refreshToken
        Qt.callLater(() => { if (token === root.refreshToken) rows.contentY = Math.min(Math.max(0, rows.contentHeight - rows.height), targetOffset) })
    }
    function selectFilter(index) { if (index === selectedFilter) return; selectedFilter = index; refresh(false) }
    function scroll(delta) { rows.contentY = Math.max(0, Math.min(rows.contentHeight - rows.height, rows.contentY + delta)) }
    onPresentedChanged: { eventLogController.active = presented; if (presented) refresh(true); else confirmLayer.visible = false }
    Component.onCompleted: refresh(false)
    Connections { target: eventLogController; function onStateChanged() { if (root.presented) root.refresh(true) } }
    SettingsHeader { title: qsTr("事件日志"); english: "EVENT LOG"; code: ModuleNumbering.code("eventLog"); count: String(root.logState.count).padStart(2, "0") }
    CalendarButton { x: 512; y: 22; width: 78; title: qsTr("清空"); kind: "secondary"; enabled: root.logState.count > 0; onClicked: confirmLayer.visible = true; reduceMotion: root.reduceMotion }
    Shortcut { sequence: "Q"; enabled: root.presented && !confirmLayer.visible; onActivated: root.selectFilter((root.selectedFilter + 4) % 5) }
    Shortcut { sequence: "E"; enabled: root.presented && !confirmLayer.visible; onActivated: root.selectFilter((root.selectedFilter + 1) % 5) }
    Shortcut { sequence: "Up"; enabled: root.presented && !confirmLayer.visible; onActivated: root.scroll(-46) }
    Shortcut { sequence: "Down"; enabled: root.presented && !confirmLayer.visible; onActivated: root.scroll(46) }
    Shortcut { sequence: "PgUp"; enabled: root.presented && !confirmLayer.visible; onActivated: root.scroll(-350) }
    Shortcut { sequence: "PgDown"; enabled: root.presented && !confirmLayer.visible; onActivated: root.scroll(350) }
    Shortcut { sequence: "Home"; enabled: root.presented && !confirmLayer.visible; onActivated: rows.contentY = 0 }
    Shortcut { sequence: "End"; enabled: root.presented && !confirmLayer.visible; onActivated: root.scroll(rows.contentHeight) }
    Shortcut { sequence: "Escape"; enabled: root.presented && confirmLayer.visible; onActivated: confirmLayer.visible = false }
    Item {
        id: tabs; y: 81; width: 590; height: 44
        Rectangle { width: 590; height: 34; color: Appearance.tone("#1E1E1E"); border.color: Appearance.tone("#343434") }
        Repeater {
            model: 4
            Rectangle {
                required property int index
                x: 28 + (index + 1) * 106.8; y: 10; width: 1; height: 14
                visible: index + 1 !== root.selectedFilter && index !== root.selectedFilter
                color: Appearance.tone("#4A4A4A")
            }
        }
        Repeater {
            model: ["Q", "E"]
            delegate: Rectangle {
                required property string modelData; required property int index
                x: index === 0 ? 6.5 : 568.5; y: 9.5; width: 15; height: 15; color: "transparent"; border.color: Appearance.tone("#6A6A6A")
                Text { anchors.centerIn: parent; text: modelData; color: Appearance.tone("#8A8A8A"); font.family: harmonyBold.name; font.pixelSize: 9; font.weight: Font.Bold; renderType: Text.CurveRendering }
            }
        }
        Repeater {
            model: root.filterTitles
            delegate: Item {
                id: tab
                required property string modelData; required property int index
                readonly property bool selected: index === root.selectedFilter
                x: 28 + index * 106.8; y: selected ? -3 : 0; width: 106.8; height: selected ? 37 : 34
                transform: Translate { y: tabHover.hovered && !tab.selected ? -1 : 0 }
                Rectangle { anchors.fill: parent; color: "white"; opacity: tabHover.hovered && !tab.selected ? 0.06 : 0 }
                Rectangle {
                    anchors.fill: parent; visible: tab.selected
                    gradient: Gradient { orientation: Gradient.Horizontal; GradientStop { position: 0; color: Appearance.accent("#FFFC6B") } GradientStop { position: 0.45; color: Appearance.accent("#FFFA00") } GradientStop { position: 1; color: Appearance.accent("#FFFA00") } }
                }
                Text {
                    anchors.centerIn: parent; text: tab.modelData; color: tab.selected ? Appearance.tone("#111111") : tabHover.hovered ? Appearance.tone("#F2F2F2") : Appearance.tone("#A0A0A0")
                    font.family: tab.selected ? harmonyBold.name : harmonyMedium.name; font.pixelSize: 13; font.weight: tab.selected ? Font.Bold : Font.Medium; font.letterSpacing: 0.52; renderType: Text.CurveRendering
                }
                Rectangle { x: width * 0.32; y: height + 3; width: width * 0.36; height: 3; visible: tab.selected; color: Appearance.accent("#FFFA00") }
                HoverHandler { id: tabHover }
                MouseArea { anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.selectFilter(tab.index) }
            }
        }
    }

    ListView {
        id: rows; objectName: "eventLogRows"
        x: 0; y: 125; width: 590; height: 350; clip: true
        model: root.entries; spacing: 6; reuseItems: true; boundsBehavior: Flickable.StopAtBounds
        delegate: Item {
            id: row
            required property var modelData
            width: rows.contentHeight > rows.height ? 580 : 590; height: 40
            Shape {
                anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeWidth: -1
                    fillGradient: LinearGradient { x1: 0; y1: 0; x2: 0; y2: 40
                        GradientStop { position: 0; color: "#E2E3DE" }
                        GradientStop { position: 1; color: "#CFD0CB" }
                    }
                    PathSvg { path: "M0 0H" + (row.width - 9) + "L" + row.width + " 9V40H0Z" }
                }
            }
            Text { x: 18; y: row.modelData.day ? 9 : 15.5; width: 52; height: 11; text: row.modelData.time; color: "#444444"; font.family: harmonyBold.name; font.pixelSize: 11; font.weight: Font.Bold; renderType: Text.CurveRendering }
            Text { x: 18; y: 23; width: 52; height: 9.5; text: row.modelData.day; color: "#777777"; font.family: harmonyRegular.name; font.pointSize: 9.5 * .75; renderType: Text.CurveRendering }
            Rectangle {
                x: 80; y: 12; width: chipText.implicitWidth + 14; height: 16
                color: row.modelData.filter === 1 ? Appearance.accent("#FFFA00") : row.modelData.filter === 3 ? "#67E8D9" : "#333333"
                Text { id: chipText; anchors.centerIn: parent; text: row.modelData.category; color: row.modelData.filter === 1 || row.modelData.filter === 3 ? "#111111" : "#EDEDED"; font.family: harmonyBold.name; font.pixelSize: 10; font.weight: Font.Bold; renderType: Text.CurveRendering }
            }
            Text { x: 166; y: 12.55; width: row.width - 178; height: 16.9; text: row.modelData.text; elide: Text.ElideRight; color: "#111111"; font.family: harmonyRegular.name; font.pixelSize: 13; renderType: Text.CurveRendering }
        }
        ScrollBar.vertical: ScrollBar {
            id: scrollBar; width: 4; policy: ScrollBar.AsNeeded; active: true
            contentItem: Rectangle { implicitWidth: 4; color: Appearance.accent("#FFFA00") }
            background: Rectangle { color: "#222222"; visible: scrollBar.size < 1 }
        }
    }
    Text { x: 0; y: 229; width: 590; visible: root.entries.length === 0; text: qsTr("NO EVENTS\n日志为空"); horizontalAlignment: Text.AlignHCenter; color: Appearance.tone("#777777"); font.family: harmonyMedium.name; font.pixelSize: 12; font.letterSpacing: 1.2; lineHeight: 1.8; renderType: Text.CurveRendering }
    Text { x: 0; y: 484; width: 590; text: root.logState.status; color: "#E6B56B"; font.family: harmonyRegular.name; font.pixelSize: 11; wrapMode: Text.Wrap; renderType: Text.CurveRendering }
    Item {
        id: confirmLayer; objectName: "eventLogConfirm"; anchors.fill: parent; z: 100; visible: false
        Rectangle { anchors.fill: parent; color: "#99000000" }
        MouseArea { anchors.fill: parent; onClicked: confirmLayer.visible = false }
        ClipboardConfirmBar {
            x: -15; y: 199; title: qsTr("清空事件日志？"); message: qsTr("%1 条记录").arg(root.logState.count); reduceMotion: root.reduceMotion
            onAccepted: { eventLogController.clear(); confirmLayer.visible = false }
            onCancelled: confirmLayer.visible = false
        }
    }
}
