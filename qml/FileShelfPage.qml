import QtQuick
import "ModuleNumbering.js" as ModuleNumbering
import "Appearance.js" as Appearance
import QtQuick.Shapes
import QtQuick.Dialogs
import QtQuick.Controls as Controls

// EndfieldHUD FileShelfCanvas (MOD-02). Geometry, palette and motion come
// from FileShelfCanvas.swift and HUDArchiveTile, not from the menu stand-ins.
FocusScope {
    id: shelfPage
    width: 590; height: 510; focus: presented
    property bool presented: false
    property bool reduceMotion: false
    readonly property var shelf: fileShelfController.state
    readonly property var items: shelf.items || []
    property var selectedIds: []
    property string focusId: ""
    property string anchorId: ""
    readonly property var selectedEntry: selectedIds.length === 1 ? items.find(item => item.id === selectedIds[0]) : null
    readonly property int columns: selectedEntry ? 3 : 4
    readonly property real gridWidth: selectedEntry ? 392 : 590
    readonly property int rowCount: Math.ceil(items.length / columns)
    readonly property real gridContentHeight: 10 + rowCount * 124 + Math.max(0, rowCount - 1) * 12
    readonly property real gridHeight: items.length ? Math.min(420, gridContentHeight) : 250
    readonly property real tileWidth: Math.floor((gridWidth - 10 - (gridContentHeight > gridHeight ? 4 : 0) - 12 * (columns - 1)) / columns)
    property bool clearing: false
    property bool confirmOpen: false
    property string contextId: ""
    property point contextPoint: Qt.point(0, 0)
    readonly property var contextEntry: items.find(item => item.id === contextId)
    property bool toastOpen: false
    property string toastMessage: ""
    property string toastTitle: ""

    function notify(title, message) { toastTitle = title; toastMessage = message || ""; toastOpen = true; toastTimer.restart() }
    function select(ids) { selectedIds = ids; if (ids.length === 1) fileShelfController.readProperties(ids[0]) }
    function clickItem(id, modifiers, clickCount) {
        if (clearing) return
        forceActiveFocus()
        if (modifiers & (Qt.ControlModifier | Qt.MetaModifier | Qt.ShiftModifier)) {
            const ids = selectedIds.slice(); const index = ids.indexOf(id)
            if (index >= 0) ids.splice(index, 1); else ids.push(id)
            select(ids)
        } else select(clickCount >= 2 ? [id] : selectedIds.length === 1 && selectedIds[0] === id ? [] : [id])
        focusId = id; anchorId = id
    }
    function moveSelection(step, extending) {
        if (!items.length || clearing) return
        const previous = items.findIndex(item => item.id === focusId)
        const index = Math.max(0, Math.min(items.length - 1, previous < 0 ? (step < 0 ? items.length - 1 : 0) : previous + step))
        const anchor = items.findIndex(item => item.id === anchorId)
        focusId = items[index].id
        if (extending && anchor >= 0) select(items.slice(Math.min(anchor, index), Math.max(anchor, index) + 1).map(item => item.id))
        else { select([focusId]); anchorId = focusId }
        Qt.callLater(() => { grid.positionViewAtIndex(index, GridView.Contain); clampScroll() })
    }
    function clampScroll() { grid.contentY = Math.max(0, Math.min(Math.max(0, gridContentHeight - gridHeight), grid.contentY)) }
    function scrollToEnd() { grid.contentY = Math.max(0, gridContentHeight - gridHeight) }
    function toggleProperties() {
        if (selectedIds.length === 1) { select([]); return }
        const entry = items.find(item => item.id === focusId && (!selectedIds.length || selectedIds.includes(item.id)))
            || items.find(item => selectedIds.includes(item.id)) || items[0]
        if (entry) { focusId = entry.id; select([entry.id]) }
    }
    function remove(ids) { if (fileShelfController.removeItems(ids)) select([]) }
    function askClear() { if (items.length && !shelf.busy) { contextId = ""; confirmOpen = true } }
    function clearShelf() {
        confirmOpen = false; clearing = true
        if (reduceMotion) finishClear(); else clearTimer.restart()
    }
    function finishClear() { clearTimer.stop(); fileShelfController.clear(); clearing = false; select([]) }
    function drag(id) {
        if (clearing) return
        const selected = selectedIds.includes(id) ? selectedIds : [id]
        fileShelfController.beginDrag(items.filter(item => selected.includes(item.id) && item.available).map(item => item.id))
    }
    onPresentedChanged: {
        fileShelfController.active = presented
        if (presented) { forceActiveFocus(); if (shelf.error) notify(qsTr("提示"), shelf.error) }
        else { contextId = ""; confirmOpen = false; chooser.close(); if (clearing) finishClear() }
    }
    onItemsChanged: {
        const ids = selectedIds.filter(id => items.some(item => item.id === id))
        if (ids.length !== selectedIds.length) select(ids)
    }
    Component.onDestruction: fileShelfController.active = false
    Connections {
        target: fileShelfController
        function onActionFinished(action, success, message) { shelfPage.notify(success ? qsTr("文件暂存架") : qsTr("提示"), message) }
        function onItemsAdded(ids) { Qt.callLater(() => shelfPage.scrollToEnd()) }
    }
    Timer { id: toastTimer; interval: 1800; onTriggered: shelfPage.toastOpen = false }
    Timer { id: clearTimer; interval: 380; onTriggered: shelfPage.finishClear() }
    FileDialog {
        id: chooser; objectName: "shelfFileChooser"; title: qsTr("添加到文件暂存架"); acceptLabel: qsTr("添加引用"); fileMode: FileDialog.OpenFiles
        // Qt Quick's in-window chooser stays above the LayerShell HUD.
        options: FileDialog.DontUseNativeDialog | FileDialog.DontResolveSymlinks
        Component.onCompleted: { if ("popupType" in chooser) chooser.popupType = Controls.Popup.Item }
        onAccepted: { fileShelfController.addUrls(selectedFiles); shelfPage.forceActiveFocus() }
        onRejected: shelfPage.forceActiveFocus()
    }
    Keys.onPressed: event => {
        if (!presented || clearing || chooser.visible || shelf.busy) return
        if (event.key === Qt.Key_Escape && (confirmOpen || contextId !== "")) { confirmOpen = false; contextId = ""; event.accepted = true; return }
        if (confirmOpen || contextId !== "") return
        const shift = event.modifiers === Qt.ShiftModifier
        if (event.modifiers === Qt.NoModifier || shift) {
            if (event.key === Qt.Key_Left) moveSelection(-1, shift)
            else if (event.key === Qt.Key_Right) moveSelection(1, shift)
            else if (event.key === Qt.Key_Up) moveSelection(-columns, shift)
            else if (event.key === Qt.Key_Down) moveSelection(columns, shift)
            else if (!shift && event.key === Qt.Key_Space) toggleProperties()
            else if (!shift && (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace)) remove(selectedIds)
            else if (!shift && event.key === Qt.Key_Home) grid.positionViewAtBeginning()
            else if (!shift && event.key === Qt.Key_End) scrollToEnd()
            else if (!shift && (event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown))
                grid.contentY = Math.max(0, Math.min(grid.contentHeight - grid.height, grid.contentY + (event.key === Qt.Key_PageDown ? 1 : -1) * (grid.height - 40)))
            else return
            event.accepted = true
        } else if (event.modifiers === Qt.ControlModifier) {
            if (event.key === Qt.Key_V) fileShelfController.pasteFiles()
            else if (event.key === Qt.Key_O && selectedEntry) fileShelfController.openItem(selectedEntry.id)
            else if (event.key === Qt.Key_R && selectedEntry) fileShelfController.revealItem(selectedEntry.id)
            else return
            event.accepted = true
        }
    }

    Item {
        width: 590; height: 81
        Row {
            y: 1.75; spacing: 6
            Repeater {
                model: ["// HUD", "/", "FILE SHELF"]
                Text { required property string modelData; required property int index; text: modelData; color: index === 2 ? Appearance.tone("#E6E6E6") : Appearance.tone("#8A8A8A"); font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; font.letterSpacing: 0.63; renderType: Text.CurveRendering }
            }
        }
        Text { x: 549 - width; y: 1.75; text: ModuleNumbering.code("fileShelf"); color: Appearance.tone("#8A8A8A"); font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; font.letterSpacing: 0.63; renderType: Text.CurveRendering }
        Item {
            x: 556; y: 3.5; width: 34; height: 7; clip: true
            Shape { anchors.fill: parent; preferredRendererType: Shape.CurveRenderer; ShapePath { strokeWidth: -1; fillColor: "#24FFFFFF"; PathSvg { path: { let p = ""; for (let x = -7; x < 41; x += 3.5 * Math.SQRT2) p += "M" + x + " 7h1.414l7 -7h-1.414Z"; return p } } } }
        }
        Text { id: title; y: 20; height: 30; text: qsTr("文件暂存架"); color: Appearance.tone("#F4F4F4"); font.family: harmonyBold.name; font.pixelSize: 26; font.weight: Font.Bold; font.letterSpacing: 2.08; renderType: Text.CurveRendering }
        Text { x: title.width + 12; y: title.y + title.baselineOffset - baselineOffset; text: "FILE SHELF"; color: Appearance.tone("#8A8A8A"); font.family: harmonyMedium.name; font.pixelSize: 13; font.letterSpacing: 2.6; font.weight: Font.Medium; renderType: Text.CurveRendering }
        CalendarButton { objectName: "shelfAdd"; x: 438; y: 22; width: 70; title: qsTr("+ 添加"); enabled: shelfPage.shelf.ready && !shelfPage.shelf.busy && !shelfPage.clearing; reduceMotion: shelfPage.reduceMotion; onClicked: chooser.open() }
        CalendarButton { objectName: "shelfClear"; x: 516; y: 22; width: 74; title: qsTr("清空"); kind: "secondary"; enabled: shelfPage.items.length > 0 && !shelfPage.shelf.busy && !shelfPage.clearing; reduceMotion: shelfPage.reduceMotion; onClicked: shelfPage.askClear() }
        Item {
            y: 60; width: 590; height: 9
            Rectangle { y: 4; width: parent.width; height: 1; color: "#38FFFFFF" }
            Rectangle { y: 3; width: 56; height: 3; color: Appearance.accent("#FFFA00") }
            Repeater { model: 10; Rectangle { required property int index; x: 454 + index * 9; width: 1; height: 9; color: "#59FFFFFF" } }
            Rectangle { x: 590 - counter.width - 8; y: -1; width: counter.width + 8; height: 12; color: Appearance.tone("#151515") }
            Text { id: counter; x: 590 - width; y: -1.5; text: String(shelfPage.items.length).padStart(2, "0") + qsTr(" 个文件"); color: Appearance.accent("#FFFA00"); font.family: harmonyBold.name; font.pixelSize: 11; font.weight: Font.Bold; renderType: Text.CurveRendering }
        }
    }

    Item {
        id: gridBox; objectName: "shelfGridBox"; x: 0; y: 81; width: shelfPage.gridWidth; height: shelfPage.gridHeight; clip: true
        enabled: !shelfPage.clearing && !shelfPage.confirmOpen
        MouseArea { anchors.fill: parent; onClicked: { shelfPage.select([]); shelfPage.forceActiveFocus() } }
        GridView {
            id: grid; objectName: "shelfGrid"; x: 5; y: 5
            width: shelfPage.columns * cellWidth; height: gridBox.height - 10
            cellWidth: shelfPage.tileWidth + 12; cellHeight: 136
            contentHeight: Math.max(0, shelfPage.gridContentHeight - 10)
            model: shelfPage.items; boundsBehavior: Flickable.StopAtBounds; cacheBuffer: 136
            onContentYChanged: { shelfPage.clampScroll(); scrollbar.shown = true; scrollbarDelay.restart() }
            delegate: ShelfTile {
                required property var modelData; required property int index
                objectName: "shelfTile-" + modelData.id
                width: shelfPage.tileWidth; entry: modelData; number: index + 1
                selected: shelfPage.selectedIds.includes(modelData.id); reduceMotion: shelfPage.reduceMotion
                presented: shelfPage.presented; clearing: shelfPage.clearing
                animationIndex: Math.max(0, index - Math.floor(grid.contentY / 136) * shelfPage.columns)
                onClicked: (modifiers, count) => shelfPage.clickItem(modelData.id, modifiers, count)
                onDragRequested: shelfPage.drag(modelData.id)
                onContextRequested: (localX, localY) => {
                    shelfPage.forceActiveFocus(); shelfPage.contextId = modelData.id
                    const point = mapToItem(shelfPage, localX, localY)
                    shelfPage.contextPoint = Qt.point(Math.max(0, Math.min(420, point.x)), Math.max(81, Math.min(408, point.y)))
                }
            }
        }
        Rectangle {
            id: scrollbar
            property bool shown: false
            x: gridBox.width - 4; width: 4; height: gridBox.height
            visible: shelfPage.gridContentHeight > shelfPage.gridHeight; color: Appearance.tone("#222222")
            opacity: shown || scrollHover.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: shelfPage.reduceMotion ? 0 : 140 } }
            HoverHandler { id: scrollHover }
            Timer { id: scrollbarDelay; interval: 1000; onTriggered: scrollbar.shown = false }
            Rectangle {
                width: 4; height: Math.max(18, grid.height * grid.height / Math.max(1, grid.contentHeight)); color: Appearance.accent("#FFFA00")
                y: Math.max(0, grid.contentY) / Math.max(1, grid.contentHeight - grid.height) * (parent.height - height)
            }
        }
        Item {
            anchors.fill: parent; visible: shelfPage.items.length === 0
            Rectangle { anchors.fill: parent; color: "transparent"; border.color: Appearance.tone("#4B4B4B") }
            Shape {
                anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
                ShapePath { strokeWidth: -1; fillColor: "#0AFFFFFF"; PathSvg { path: { let p = ""; for (let x = -250; x < 590; x += 7) p += "M" + x + " 250h1.414l250 -250h-1.414Z"; return p } } }
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter; y: (parent.height - 41.5) / 2
                Text { text: "[ "; color: Appearance.accent("#FFFA00"); font.family: harmonyBold.name; font.pixelSize: 20; font.weight: Font.Bold; font.letterSpacing: 4.8; renderType: Text.CurveRendering }
                Text { text: "SHELF EMPTY"; color: Appearance.tone("#E9E9E9"); font.family: harmonyBold.name; font.pixelSize: 20; font.weight: Font.Bold; font.letterSpacing: 4.8; renderType: Text.CurveRendering }
                Text { text: " ]"; color: Appearance.accent("#FFFA00"); font.family: harmonyBold.name; font.pixelSize: 20; font.weight: Font.Bold; font.letterSpacing: 4.8; renderType: Text.CurveRendering }
            }
            Text { anchors.horizontalCenter: parent.horizontalCenter; y: (parent.height - 41.5) / 2 + 30; text: qsTr("拖入文件以暂存"); color: Appearance.tone("#8A8A8A"); font.family: harmonyRegular.name; font.pointSize: 11.5 * 0.75; font.letterSpacing: 1.38; renderType: Text.CurveRendering }
        }
        DropArea {
            id: incoming; anchors.fill: parent; enabled: shelfPage.presented && !shelfPage.shelf.busy && !shelfPage.clearing
            onEntered: drag => { drag.accepted = drag.hasUrls && drag.urls.every(url => String(url).startsWith("file:")) }
            onDropped: drop => { if (fileShelfController.addUrls(drop.urls)) { drop.action = Qt.CopyAction; drop.accepted = true } }
        }
        Rectangle {
            anchors.fill: parent; color: "#0DFFFA00"; border.color: Appearance.accent("#FFFA00"); opacity: incoming.containsDrag ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: shelfPage.reduceMotion ? 0 : 140; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1] } }
        }
    }
    Loader {
        x: 404; y: 81; active: !!shelfPage.selectedEntry; objectName: "shelfPropertiesLoader"
        sourceComponent: ShelfProperties {
            entry: shelfPage.selectedEntry || ({}); details: fileShelfController.properties; reduceMotion: shelfPage.reduceMotion
            onOpened: fileShelfController.openItem(entry.id)
            onRevealed: fileShelfController.revealItem(entry.id)
            onRemoved: shelfPage.remove([entry.id])
        }
    }
    Item {
        anchors.fill: parent; z: 80; visible: shelfPage.contextId !== ""
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton; onClicked: shelfPage.contextId = "" }
        Rectangle {
            x: shelfPage.contextPoint.x; y: shelfPage.contextPoint.y; width: 170; height: 98; color: "#F51B1B1B"; border.color: Appearance.tone("#555555")
            Column {
                Repeater {
                    model: [{text: qsTr("打开"), action: "open"}, {text: qsTr("在文件管理器中显示"), action: "reveal"}, {text: qsTr("从暂存架移除"), action: "remove"}]
                    Item {
                        required property var modelData; required property int index; width: 170; height: 32
                        enabled: index === 2 || !!(shelfPage.contextEntry && shelfPage.contextEntry.available)
                        opacity: enabled ? 1 : 0.35
                        Rectangle { anchors.fill: parent; color: contextMouse.containsMouse ? Appearance.tone("#333333") : "transparent" }
                        Rectangle { y: 0; width: 170; height: 1; visible: index === 2; color: Appearance.tone("#444444") }
                        Text { x: 14; height: 32; verticalAlignment: Text.AlignVCenter; text: modelData.text; color: index === 2 ? "#E35B5D" : Appearance.tone("#EEEEEE"); font.family: harmonyMedium.name; font.pixelSize: 12; renderType: Text.CurveRendering }
                        MouseArea {
                            id: contextMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                const id = shelfPage.contextId; shelfPage.contextId = ""
                                if (modelData.action === "open") fileShelfController.openItem(id)
                                else if (modelData.action === "reveal") fileShelfController.revealItem(id)
                                else shelfPage.remove([id])
                            }
                        }
                    }
                }
            }
        }
    }
    Item {
        objectName: "shelfConfirm"; anchors.fill: parent; z: 200; visible: shelfPage.confirmOpen
        Rectangle { anchors.fill: parent; color: "#99000000" }
        MouseArea { anchors.fill: parent; onClicked: shelfPage.confirmOpen = false }
        ClipboardConfirmBar {
            x: -15; y: 370; title: qsTr("从暂存架移除 ") + shelfPage.items.length + qsTr(" 个文件？"); message: qsTr("原文件保持不变。"); confirmTitle: qsTr("全部移除"); reduceMotion: shelfPage.reduceMotion
            onCancelled: shelfPage.confirmOpen = false
            onAccepted: shelfPage.clearShelf()
        }
    }
    Item {
        x: 320; y: 424; width: 270; height: 58; z: 300; opacity: shelfPage.toastOpen ? 1 : 0; visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: shelfPage.reduceMotion ? 0 : 250 } }
        Rectangle { anchors.fill: parent; color: "#F0181818"; border.color: Appearance.tone("#505050") }
        Rectangle { width: 4; height: parent.height; color: Appearance.accent("#FFFA00") }
        Text { x: 18; y: 10; width: 240; text: shelfPage.toastTitle; color: Appearance.tone("#F2F2F2"); elide: Text.ElideRight; font.family: harmonyBold.name; font.pixelSize: 14; font.weight: Font.Bold; renderType: Text.CurveRendering }
        Text { x: 18; y: 33; width: 240; text: shelfPage.toastMessage; color: Appearance.tone("#A0A0A0"); elide: Text.ElideRight; font.family: harmonyRegular.name; font.pointSize: 11.5 * 0.75; renderType: Text.CurveRendering }
    }
}
