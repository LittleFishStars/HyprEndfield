import QtQuick
import "ModuleNumbering.js" as ModuleNumbering
import "Appearance.js" as Appearance
import QtQuick.Shapes
import QtQuick.Dialogs
import QtQuick.Controls as Controls

// EndfieldHUD ArchiveCanvas (MOD-04).
FocusScope {
    id: archivePage
    width: 590; height: 510; focus: presented
    property bool presented: false
    property bool reduceMotion: false
    readonly property var archive: archiveController.state
    readonly property var categories: archive.categories || []
    readonly property var entries: archive.entries || []
    readonly property var currentDocument: archive.document || ({})
    property string visualPage: "list"
    property var lockedSnapshot: ({ passwordStatus: "checking", redactionWidths: [], titleWidth: 10 })
    property bool loadingEditor: false
    property int mediaIndex: 0
    property string pendingMode: ""
    property string pendingValue: ""
    property int pendingDx: 0
    property int pendingDy: 0
    property string renameId: ""
    property string renameText: ""
    property bool categoryMenuVisible: false
    property bool mediaMenuVisible: false
    property bool shelfMenuVisible: false
    property string dialogMode: ""
    property string confirmAction: ""
    property string confirmTitle: ""
    property string confirmMessage: ""
    property string recoveryStage: "owner"
    property string formError: ""
    property bool toastOpen: false
    property string toastTitle: ""
    property string toastMessage: ""
    property string lastError: ""
    property var formatStyle: ({ family: "HarmonyOS Sans", size: 14, color: Appearance.tone("#EEEEEE"), bold: false, italic: false, underline: false, strikeout: false })

    function categoryName(id) {
        const category = categories.find(value => value.id === id)
        return category ? category.name : qsTr("未分类")
    }
    function escapeHtml(value) {
        return String(value || "").replaceAll("&", "&amp;").replaceAll("<", "&lt;").replaceAll(">", "&gt;").replaceAll("\n", "<br>")
    }
    function notify(title, message) { toastTitle = title; toastMessage = message || ""; toastOpen = true; toastTimer.restart() }
    function closeMenus() { categoryMenuVisible = false; mediaMenuVisible = false; shelfMenuVisible = false; formatBar.closeMenus() }
    function setEditorDocument(document) {
        if (!document || !document.id) return
        loadingEditor = true
        titleField.text = document.title || ""
        body.text = document.richBody && document.richBody.length ? document.richBody : escapeHtml(document.body)
        mediaIndex = Math.max(0, Math.min(mediaIndex, (document.media || []).length - 1))
        loadingEditor = false
        updateFormatState()
    }
    function persistEditor() {
        if (loadingEditor || visualPage !== "editor" || !currentDocument.id) return
        archiveController.updateSelected(titleField.text, body.getText(0, body.length), body.text)
    }
    function updateFormatState() {
        if (!body.textDocument) return
        const value = archiveController.formatState(body.textDocument, Math.max(0, body.cursorPosition))
        if (value && value.family) formatStyle = value
    }
    function applyFormat(key, value) {
        closeMenus()
        if (archiveController.applyFormat(body.textDocument, body.selectionStart, body.selectionEnd, key, value)) {
            updateFormatState(); persistEditor(); body.forceActiveFocus()
        }
    }
    function navigate(mode, value, dx, dy) {
        if (subnav.running || archive.busy) return
        closeMenus(); renameId = ""; pendingMode = mode; pendingValue = value || ""; pendingDx = dx; pendingDy = dy
        if (reduceMotion) { performPending(); return }
        subnav.restart()
    }
    function performPending() {
        if (pendingMode === "category") archiveController.selectCategory(pendingValue)
        else if (pendingMode === "open") archiveController.selectDocument(pendingValue)
        else if (pendingMode === "new") archiveController.createDocument()
        else if (pendingMode === "back") archiveController.closeDocument()
        else if (pendingMode === "delete") archiveController.deleteSelected()
        const current = archiveController.state
        visualPage = current.page || "list"
        if (visualPage === "editor") setEditorDocument(current.document)
        else if (visualPage === "locked") lockedSnapshot = current.document
        pendingMode = ""; pendingValue = ""
    }
    function selectCategory(id) {
        if (id === archive.filter) return
        const oldIndex = categories.findIndex(value => value.id === archive.filter)
        const newIndex = categories.findIndex(value => value.id === id)
        navigate("category", id, 0, newIndex >= oldIndex ? 1 : -1)
    }
    function beginRename() {
        if (archive.filter === "all" || archive.filter === "uncategorized") return
        renameId = archive.filter; renameText = categoryName(archive.filter)
    }
    function askConfirm(action, title, message) { closeMenus(); confirmAction = action; confirmTitle = title; confirmMessage = message }
    function confirm() {
        const action = confirmAction; confirmAction = ""
        if (action === "delete") navigate("delete", "", -1, 0)
        else if (action === "declassify") archiveController.declassifySelected()
        else if (action === "removeCategory") {
            const id = archive.filter; archiveController.selectCategory("all"); archiveController.removeCategory(id); visualPage = "list"
        }
    }
    function openPasswordForm(mode) { closeMenus(); dialogMode = mode; formError = ""; newPassword.text = ""; confirmPassword.text = ""; Qt.callLater(() => newPassword.forceActiveFocus()) }
    function submitPasswordForm() {
        formError = ""
        if (dialogMode === "classify") archiveController.classifySelected(newPassword.text, confirmPassword.text)
        else archiveController.recoverSelected(newPassword.text, confirmPassword.text, false)
    }
    function openRecovery() { closeMenus(); dialogMode = "recover"; recoveryStage = "owner"; formError = ""; ownerPassword.text = ""; Qt.callLater(() => ownerPassword.forceActiveFocus()) }
    function playOpening() {
        const current = archiveController.state.document || ({})
        lockedPanel.revealedTitle = current.title || qsTr("未命名")
        lockedPanel.revealedBody = current.body || ""
        openingTimer.interval = lockedPanel.opening()
        openingTimer.restart()
    }

    onPresentedChanged: {
        archiveController.active = presented
        if (presented) {
            const current = archiveController.state
            visualPage = current.page || "list"
            if (visualPage === "editor") setEditorDocument(current.document)
            else if (visualPage === "locked") lockedSnapshot = current.document
            forceActiveFocus()
        } else { closeMenus(); archiveController.cancelRecovery(); dialogMode = ""; confirmAction = ""; mediaChooser.close() }
    }
    Component.onCompleted: {
        visualPage = archive.page || "list"
        if (visualPage === "editor") setEditorDocument(currentDocument)
        else if (visualPage === "locked") lockedSnapshot = currentDocument
    }
    Component.onDestruction: archiveController.active = false

    Connections {
        target: archiveController
        function onStateChanged() {
            const current = archiveController.state
            if (current.error && current.error !== archivePage.lastError) { archivePage.lastError = current.error; archivePage.notify(qsTr("档案库"), current.error) }
            if (archivePage.visualPage === "locked" && current.page === "locked") archivePage.lockedSnapshot = current.document
            if (archivePage.visualPage === "editor" && current.page === "editor" && (current.document.media || []).length <= archivePage.mediaIndex)
                archivePage.mediaIndex = Math.max(0, (current.document.media || []).length - 1)
        }
        function onActionFinished(action, success, message) {
            if (!success) { archivePage.formError = message; archivePage.notify(qsTr("档案库"), message); return }
            if (action === "classify") {
                archivePage.dialogMode = ""; archivePage.lockedSnapshot = archiveController.state.document
                archivePage.visualPage = "locked"; Qt.callLater(() => lockedPanel.sealing())
            } else archivePage.notify(qsTr("档案库"), message)
        }
        function onUnlockFinished(success, wrongPassword, message) {
            if (success) archivePage.playOpening()
            else if (wrongPassword) { lockedPanel.shake(); archivePage.notify(qsTr("无法解锁"), qsTr("密码不正确。")) }
            else if (message) archivePage.notify(qsTr("无法解锁"), message)
        }
        function onOwnerAuthenticationFinished(success, message) {
            if (success) { archivePage.recoveryStage = "password"; archivePage.formError = ""; Qt.callLater(() => newPassword.forceActiveFocus()) }
            else archivePage.formError = message
        }
        function onRecoveryFinished(success, message) {
            if (!success) { archivePage.formError = message; return }
            archivePage.dialogMode = ""; archivePage.notify(qsTr("档案库"), message); archivePage.playOpening()
        }
    }

    Timer { id: toastTimer; interval: 1800; onTriggered: archivePage.toastOpen = false }
    Timer { id: openingTimer; interval: 0; onTriggered: { archivePage.visualPage = "editor"; archivePage.setEditorDocument(archiveController.state.document) } }

    FileDialog {
        id: mediaChooser; objectName: "archiveMediaChooser"; title: qsTr("向文档添加媒体"); acceptLabel: qsTr("添加引用"); fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("图片与视频 (*.png *.jpg *.jpeg *.webp *.gif *.bmp *.svg *.mp4 *.mkv *.webm *.mov *.avi)"), qsTr("所有文件 (*)")]
        options: FileDialog.DontUseNativeDialog | FileDialog.DontResolveSymlinks
        Component.onCompleted: { if ("popupType" in mediaChooser) mediaChooser.popupType = Controls.Popup.Item }
        onAccepted: { archiveController.importMedia(selectedFiles); archivePage.forceActiveFocus() }
        onRejected: archivePage.forceActiveFocus()
    }

    SequentialAnimation {
        id: subnav
        ParallelAnimation {
            NumberAnimation { target: pageHost; property: "opacity"; to: 0; duration: 130; easing.type: Easing.InOutQuad }
            NumberAnimation { target: pageHost; property: "x"; to: -0.6 * archivePage.pendingDx * 24; duration: 130; easing.type: Easing.InOutQuad }
            NumberAnimation { target: pageHost; property: "y"; to: -0.6 * archivePage.pendingDy * 24; duration: 130; easing.type: Easing.InOutQuad }
        }
        ScriptAction { script: { archivePage.performPending(); pageHost.x = archivePage.pendingDx * 24; pageHost.y = archivePage.pendingDy * 24 } }
        ParallelAnimation {
            NumberAnimation { target: pageHost; property: "opacity"; from: 0; to: 1; duration: 476; easing.type: Easing.OutCubic }
            NumberAnimation { target: pageHost; property: "x"; to: 0; duration: 476; easing.type: Easing.OutCubic }
            NumberAnimation { target: pageHost; property: "y"; to: 0; duration: 476; easing.type: Easing.OutCubic }
        }
    }

    Item {
        id: pageHost; anchors.fill: parent

        // Shared original header.
        Item {
            id: header; width: 590; height: 81
            Row {
                x: archivePage.visualPage === "list" ? 0 : 42; y: 1.75; spacing: 6
                Repeater {
                    model: archivePage.visualPage === "list" ? ["// HUD", "/", "ARCHIVE"]
                        : archivePage.visualPage === "locked" ? ["// HUD", "/", "ARCHIVE", "/", archivePage.categoryName(archivePage.lockedSnapshot.categoryId), "/", "CLASSIFIED"]
                        : ["// HUD", "/", "ARCHIVE", "/", archivePage.categoryName(archivePage.currentDocument.categoryId)]
                    Text { required property string modelData; required property int index; text: modelData; color: index >= 2 ? Appearance.tone("#E6E6E6") : Appearance.tone("#8A8A8A"); font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; font.letterSpacing: 0.63; renderType: Text.CurveRendering }
                }
            }
            Text { x: 549 - width; y: 1.75; text: ModuleNumbering.code("archive"); color: Appearance.tone("#8A8A8A"); font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; font.letterSpacing: 0.63; renderType: Text.CurveRendering }
            Item { x: 556; y: 3.5; width: 34; height: 7; clip: true; Shape { anchors.fill: parent; preferredRendererType: Shape.CurveRenderer; ShapePath { strokeWidth: -1; fillColor: "#24FFFFFF"; PathSvg { path: { let p = ""; for (let x = -7; x < 41; x += 3.5 * Math.SQRT2) p += "M" + x + " 7h1.414l7 -7h-1.414Z"; return p } } } } }
            CalendarButton { visible: archivePage.visualPage !== "list"; x: 0; y: 20; width: 30; height: 30; kind: "icon"; glyph: "‹"; reduceMotion: archivePage.reduceMotion; onClicked: archivePage.navigate("back", "", -1, 0) }
            Text {
                id: heading; x: archivePage.visualPage === "list" ? 0 : 42; y: 20; height: 30
                text: archivePage.visualPage === "list" ? qsTr("档案库") : archivePage.visualPage === "locked" ? qsTr("机密文档") : qsTr("文档")
                color: Appearance.tone("#F4F4F4"); font.family: harmonyBold.name; font.pixelSize: 26; font.weight: Font.Bold; font.letterSpacing: 2.08; renderType: Text.CurveRendering
            }
            Text { x: heading.x + heading.width + 12; y: heading.y + heading.baselineOffset - baselineOffset; text: archivePage.visualPage === "list" ? "ARCHIVE" : archivePage.visualPage === "locked" ? "CLASSIFIED" : "DOCUMENT"; color: Appearance.tone("#8A8A8A"); font.family: harmonyMedium.name; font.pixelSize: 13; font.weight: Font.Medium; font.letterSpacing: 2.6; renderType: Text.CurveRendering }
            CalendarButton { visible: archivePage.visualPage === "list"; x: 526; y: 22; width: 64; title: qsTr("新建"); enabled: archive.ready && !archive.busy; reduceMotion: archivePage.reduceMotion; onClicked: archivePage.navigate("new", "", 1, 0) }
            CalendarButton { visible: archivePage.visualPage === "editor"; x: 412; y: 22; width: 178; title: archivePage.categoryName(archivePage.currentDocument.categoryId); kind: "secondary"; enabled: !archive.busy; reduceMotion: archivePage.reduceMotion; onClicked: archivePage.categoryMenuVisible = !archivePage.categoryMenuVisible }
            Rectangle {
                visible: archivePage.visualPage === "locked"; x: 503; y: 22; width: 87; height: 26; color: "#D63235"
                Text { anchors.centerIn: parent; text: qsTr("◆  机密"); color: Appearance.tone("#FFFFFF"); font.family: harmonyBold.name; font.pixelSize: 10; font.weight: Font.Bold; renderType: Text.CurveRendering }
            }
            Item {
                y: 60; width: 590; height: 9
                Rectangle { y: 4; width: 590; height: 1; color: "#38FFFFFF" }
                Rectangle { y: 3; width: 56; height: 3; color: Appearance.accent("#FFFA00") }
                Repeater { model: 10; Rectangle { required property int index; x: 454 + index * 9; width: 1; height: 9; color: "#59FFFFFF" } }
                Rectangle { x: 590 - counter.width - 8; y: -1; width: counter.width + 8; height: 12; color: Appearance.tone("#151515") }
                Text {
                    id: counter; x: 590 - width; y: -1.5
                    text: archivePage.visualPage === "list" ? String(archivePage.entries.length).padStart(2, "0") + " DOCS"
                        : archivePage.visualPage === "locked" ? "LOCKED" : archivePage.currentDocument.date || ""
                    color: Appearance.accent("#FFFA00"); font.family: harmonyBold.name; font.pixelSize: 11; font.weight: Font.Bold; renderType: Text.CurveRendering
                }
            }
        }

        Item {
            id: listPage; visible: archivePage.visualPage === "list"; x: 0; y: 81; width: 590; height: 429
            ListView {
                id: categoryList; objectName: "archiveCategories"; width: 154; height: 384; clip: true; spacing: 6; model: archivePage.categories
                delegate: Item {
                    required property var modelData; required property int index; width: 150; height: 48
                    ArchiveCategoryPlate { anchors.fill: parent; entry: modelData; selected: modelData.id === archivePage.archive.filter; reduceMotion: archivePage.reduceMotion; onClicked: archivePage.selectCategory(modelData.id); onRenameRequested: { if (modelData.id !== "all" && modelData.id !== "uncategorized") { archivePage.renameId = modelData.id; archivePage.renameText = modelData.name } } }
                    CalendarTextField {
                        id: renameField; objectName: "archiveRenameField"; x: 16; y: 5; width: 118; height: 24; maximumLength: 24; visible: archivePage.renameId === modelData.id
                        onVisibleChanged: if (visible) { text = archivePage.renameText; edit() }
                        onAccepted: { if (archiveController.renameCategory(modelData.id, text)) archivePage.renameId = ""; else edit() }
                    }
                }
                Controls.ScrollBar.vertical: Controls.ScrollBar { policy: categoryList.contentHeight > categoryList.height ? Controls.ScrollBar.AlwaysOn : Controls.ScrollBar.AlwaysOff; width: 4 }
            }
            Row {
                y: 390; spacing: 6
                CalendarButton { width: 30; height: 30; kind: "icon"; glyph: "+"; enabled: !archive.busy; reduceMotion: archivePage.reduceMotion; onClicked: archiveController.addCategory() }
                CalendarButton { width: 30; height: 30; kind: "icon"; glyph: "✎"; enabled: archive.filter !== "all" && archive.filter !== "uncategorized" && !archive.busy; reduceMotion: archivePage.reduceMotion; onClicked: archivePage.beginRename() }
                CalendarButton { width: 30; height: 30; kind: "icon"; glyph: "−"; enabled: archive.filter !== "all" && archive.filter !== "uncategorized" && !archive.busy; reduceMotion: archivePage.reduceMotion; onClicked: archivePage.askConfirm("removeCategory", qsTr("删除") + archivePage.categoryName(archive.filter) + "？", qsTr("其中的文档将移至未分类。")) }
            }
            Item {
                x: 162; width: 428; height: 420; clip: true
                GridView {
                    id: documentGrid; objectName: "archiveDocuments"; x: 4; y: 4; width: 424; height: 412; model: archivePage.entries
                    cellWidth: 144; cellHeight: 132; clip: true; boundsBehavior: Flickable.StopAtBounds
                    delegate: ArchiveTile { required property var modelData; required property int index; width: 132; entry: modelData; number: modelData.number || index + 1; reduceMotion: archivePage.reduceMotion; onClicked: archivePage.navigate("open", modelData.id, 1, 0) }
                    Controls.ScrollBar.vertical: Controls.ScrollBar { policy: documentGrid.contentHeight > documentGrid.height ? Controls.ScrollBar.AlwaysOn : Controls.ScrollBar.AlwaysOff; width: 4 }
                }
                Item {
                    anchors.fill: parent; visible: archivePage.entries.length === 0
                    Row { anchors.horizontalCenter: parent.horizontalCenter; y: 150; spacing: 4
                        Text { text: "[ "; color: Appearance.accent("#FFFA00"); font.family: harmonyBold.name; font.pixelSize: 20; font.weight: Font.Bold; font.letterSpacing: 4.8; renderType: Text.CurveRendering }
                        Text { text: "EMPTY"; color: Appearance.tone("#E9E9E9"); font.family: harmonyBold.name; font.pixelSize: 20; font.weight: Font.Bold; font.letterSpacing: 4.8; renderType: Text.CurveRendering }
                        Text { text: " ]"; color: Appearance.accent("#FFFA00"); font.family: harmonyBold.name; font.pixelSize: 20; font.weight: Font.Bold; font.letterSpacing: 4.8; renderType: Text.CurveRendering }
                    }
                }
            }
        }

        Item {
            id: editorPage; visible: archivePage.visualPage === "editor"; x: 0; y: 81; width: 590; height: 429
            TextInput {
                id: titleField; objectName: "archiveTitle"; width: 590; height: 36; leftPadding: 12; rightPadding: 12; maximumLength: 200
                color: Appearance.tone("#F0F0F0"); selectionColor: "#66FFFA00"; selectedTextColor: Appearance.tone("#FFFFFF"); verticalAlignment: TextInput.AlignVCenter; selectByMouse: true
                font.family: harmonyBold.name; font.pixelSize: 18; font.weight: Font.Bold
                Rectangle { anchors.fill: parent; z: -1; color: Appearance.tone("#1C1C1C"); border.color: titleField.activeFocus ? Appearance.accent("#FFFA00") : Appearance.tone("#3A3A3A") }
                Text { x: 12; anchors.verticalCenter: parent.verticalCenter; visible: !titleField.text.length && !titleField.activeFocus; text: qsTr("标题"); color: Appearance.tone("#777777"); font.family: harmonyBold.name; font.pixelSize: 18 }
                onTextEdited: archivePage.persistEditor()
            }
            NotesFormatBar { id: formatBar; x: 0; y: 42; style: archivePage.formatStyle; reduceMotion: archivePage.reduceMotion; onChanged: (key, value) => archivePage.applyFormat(key, value) }
            CalendarButton { x: 526; y: 42; width: 64; height: 26; title: qsTr("媒体"); kind: "secondary"; enabled: (archivePage.currentDocument.media || []).length < 16 && !archive.busy; reduceMotion: archivePage.reduceMotion; onClicked: { formatBar.closeMenus(); archivePage.mediaMenuVisible = !archivePage.mediaMenuVisible } }
            Rectangle {
                x: 0; y: 74; width: 590; height: (archivePage.currentDocument.media || []).length ? 171 : 321; color: Appearance.tone("#1C1C1C"); border.color: body.activeFocus ? Appearance.accent("#FFFA00") : Appearance.tone("#3A3A3A")
                TextEdit {
                    id: body; objectName: "archiveBody"; anchors.fill: parent; anchors.margins: 1; leftPadding: 13; rightPadding: 13; topPadding: 11; bottomPadding: 11
                    color: Appearance.tone("#EEEEEE"); selectionColor: "#66FFFA00"; selectedTextColor: Appearance.tone("#FFFFFF"); textFormat: TextEdit.RichText; wrapMode: TextEdit.Wrap; selectByMouse: true
                    font.family: harmonyRegular.name; font.pixelSize: 14; renderType: Text.CurveRendering; persistentSelection: true
                    onTextChanged: archivePage.persistEditor()
                    onCursorPositionChanged: archivePage.updateFormatState()
                    onSelectionStartChanged: archivePage.updateFormatState()
                    Text { x: 13; y: 11; visible: body.length === 0 && !body.activeFocus; text: qsTr("开始书写"); color: Appearance.tone("#777777"); font.family: harmonyRegular.name; font.pixelSize: 14; renderType: Text.CurveRendering }
                }
            }
            ArchiveMediaStrip {
                id: mediaStrip; visible: (archivePage.currentDocument.media || []).length > 0; x: 0; y: 253
                media: archivePage.currentDocument.media || []; index: archivePage.mediaIndex; reduceMotion: archivePage.reduceMotion
                onPrevious: archivePage.mediaIndex = (archivePage.mediaIndex - 1 + media.length) % media.length
                onNext: archivePage.mediaIndex = (archivePage.mediaIndex + 1) % media.length
                onRemoved: archiveController.removeMedia(archivePage.mediaIndex)
            }
            Row {
                anchors.right: parent.right; y: 403; spacing: 8
                CalendarButton { width: archivePage.currentDocument.classified ? 92 : 82; title: archivePage.currentDocument.classified ? qsTr("取消机密") : qsTr("设为机密"); kind: "secondary"; enabled: !archive.busy; reduceMotion: archivePage.reduceMotion; onClicked: { if (archivePage.currentDocument.classified) archivePage.askConfirm("declassify", qsTr("取消机密？"), qsTr("密码将被删除，文本保持可读。")); else archivePage.openPasswordForm("classify") } }
                CalendarButton { width: 68; title: qsTr("删除"); kind: "secondary"; enabled: !archive.busy; reduceMotion: archivePage.reduceMotion; onClicked: archivePage.askConfirm("delete", qsTr("删除此文档？"), titleField.text || qsTr("未命名")) }
            }
        }

        ArchiveLockedPanel {
            id: lockedPanel; objectName: "archiveLockedPanel"; visible: archivePage.visualPage === "locked"; x: 0; y: 81
            document: archivePage.lockedSnapshot; reduceMotion: archivePage.reduceMotion
            onUnlock: password => archiveController.unlockSelected(password)
            onRecover: archivePage.openRecovery()
        }
    }

    // Menus are kept above the editor while the body retains its selection.
    Item {
        anchors.fill: parent; z: 100; visible: archivePage.categoryMenuVisible || archivePage.mediaMenuVisible || archivePage.shelfMenuVisible
        MouseArea { anchors.fill: parent; onClicked: archivePage.closeMenus() }
        Rectangle {
            visible: archivePage.categoryMenuVisible; x: 412; y: 52; width: 178; height: Math.min(220, (archivePage.categories.length - 1) * 30); color: "#F51B1B1B"; border.color: Appearance.tone("#555555")
            ListView {
                anchors.fill: parent; model: archivePage.categories.filter(value => value.id !== "all"); clip: true
                delegate: Rectangle {
                    required property var modelData; width: 178; height: 30; color: rowPointer.containsMouse ? Appearance.tone("#343434") : "transparent"
                    Rectangle { width: 3; height: 16; y: 7; color: Appearance.accent("#FFFA00"); visible: archivePage.currentDocument.categoryId === modelData.id || (!archivePage.currentDocument.categoryId && modelData.id === "uncategorized") }
                    Text { x: 12; width: 158; height: 30; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; text: modelData.name; color: Appearance.tone("#EEEEEE"); font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering }
                    MouseArea { id: rowPointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { archiveController.moveSelected(modelData.id); archivePage.categoryMenuVisible = false } }
                }
            }
        }
        Rectangle {
            visible: archivePage.mediaMenuVisible; x: 414; y: 151; width: 176; height: fileShelfController.mediaChoices().length ? 60 : 30; color: "#F51B1B1B"; border.color: Appearance.tone("#555555")
            Column {
                width: parent.width
                Rectangle { width: 176; height: 30; color: finderPointer.containsMouse ? Appearance.tone("#343434") : "transparent"; Text { x: 12; height: 30; verticalAlignment: Text.AlignVCenter; text: qsTr("从文件管理器选择"); color: Appearance.tone("#EEEEEE"); font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering } MouseArea { id: finderPointer; anchors.fill: parent; hoverEnabled: true; onClicked: { archivePage.mediaMenuVisible = false; mediaChooser.open() } } }
                Rectangle { visible: fileShelfController.mediaChoices().length > 0; width: 176; height: 30; color: shelfPointer.containsMouse ? Appearance.tone("#343434") : "transparent"; Text { x: 12; height: 30; verticalAlignment: Text.AlignVCenter; text: qsTr("从暂存架选择"); color: Appearance.tone("#EEEEEE"); font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering } MouseArea { id: shelfPointer; anchors.fill: parent; hoverEnabled: true; onClicked: { archivePage.mediaMenuVisible = false; archivePage.shelfMenuVisible = true } } }
            }
        }
        Rectangle {
            visible: archivePage.shelfMenuVisible; x: 350; y: 151; width: 240; height: Math.min(240, fileShelfController.mediaChoices().length * 32); color: "#F51B1B1B"; border.color: Appearance.tone("#555555")
            ListView {
                anchors.fill: parent; model: fileShelfController.mediaChoices(); clip: true
                delegate: Rectangle { required property var modelData; width: 240; height: 32; color: shelfItem.containsMouse ? Appearance.tone("#343434") : "transparent"; Text { x: 12; width: 216; height: 32; verticalAlignment: Text.AlignVCenter; elide: Text.ElideMiddle; text: modelData.name; color: Appearance.tone("#EEEEEE"); font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering } MouseArea { id: shelfItem; anchors.fill: parent; hoverEnabled: true; onClicked: { const url = fileShelfController.mediaUrl(modelData.id); if (url) archiveController.importMedia([url]); archivePage.shelfMenuVisible = false } } }
            }
        }
    }

    Item {
        anchors.fill: parent; z: 200; visible: archivePage.confirmAction !== ""
        Rectangle { anchors.fill: parent; color: "#99000000" }
        MouseArea { anchors.fill: parent; onClicked: archivePage.confirmAction = "" }
        Rectangle {
            x: 110; y: 190; width: 370; height: 132; color: "#F21A1A1A"; border.color: Appearance.tone("#555555")
            Rectangle { width: 4; height: parent.height; color: archivePage.confirmAction === "delete" || archivePage.confirmAction === "removeCategory" ? "#D63235" : Appearance.accent("#FFFA00") }
            Text { x: 20; y: 18; width: 330; text: archivePage.confirmTitle; color: Appearance.tone("#F2F2F2"); font.family: harmonyBold.name; font.pixelSize: 17; font.weight: Font.Bold; renderType: Text.CurveRendering }
            Text { x: 20; y: 49; width: 330; text: archivePage.confirmMessage; color: Appearance.tone("#A0A0A0"); font.family: harmonyRegular.name; font.pixelSize: 12; wrapMode: Text.Wrap; renderType: Text.CurveRendering }
            Row { anchors.right: parent.right; anchors.rightMargin: 14; y: 92; spacing: 8
                CalendarButton { width: 64; title: qsTr("取消"); kind: "secondary"; reduceMotion: archivePage.reduceMotion; onClicked: archivePage.confirmAction = "" }
                CalendarButton { width: 74; title: qsTr("确认"); reduceMotion: archivePage.reduceMotion; onClicked: archivePage.confirm() }
            }
        }
    }

    Item {
        anchors.fill: parent; z: 210; visible: archivePage.dialogMode !== ""
        Rectangle { anchors.fill: parent; color: "#99000000" }
        MouseArea { anchors.fill: parent; onClicked: {} }
        Rectangle {
            x: 130; y: archivePage.dialogMode === "recover" && archivePage.recoveryStage === "owner" ? 180 : 150
            width: 330; height: archivePage.dialogMode === "recover" && archivePage.recoveryStage === "owner" ? 152 : 205
            color: "#F51B1B1B"; border.color: Appearance.tone("#555555")
            Rectangle { width: 4; height: parent.height; color: Appearance.accent("#FFFA00") }
            Text { x: 18; y: 14; text: archivePage.dialogMode === "classify" ? qsTr("设为机密") : qsTr("恢复访问"); color: Appearance.tone("#F2F2F2"); font.family: harmonyBold.name; font.pixelSize: 17; font.weight: Font.Bold; renderType: Text.CurveRendering }
            Item {
                visible: archivePage.dialogMode === "recover" && archivePage.recoveryStage === "owner"; anchors.fill: parent
                Text { x: 18; y: 43; width: 294; text: qsTr("输入当前 Linux 用户的登录密码"); color: Appearance.tone("#A0A0A0"); font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering }
                TextInput {
                    id: ownerPassword; objectName: "archiveOwnerPassword"; x: 18; y: 66; width: 294; height: 32; leftPadding: 11; rightPadding: 11; echoMode: TextInput.Password
                    color: Appearance.tone("#EEEEEE"); selectionColor: "#66FFFA00"; verticalAlignment: TextInput.AlignVCenter; font.family: harmonyRegular.name; font.pixelSize: 12
                    Rectangle { anchors.fill: parent; z: -1; color: Appearance.tone("#202020"); border.color: parent.activeFocus ? Appearance.accent("#FFFA00") : Appearance.tone("#505050") }
                    onAccepted: { archiveController.authenticateOwner(text); text = "" }
                }
                Row { anchors.right: parent.right; anchors.rightMargin: 18; y: 110; spacing: 8
                    CalendarButton { width: 64; title: qsTr("取消"); kind: "secondary"; reduceMotion: archivePage.reduceMotion; onClicked: { archiveController.cancelRecovery(); archivePage.dialogMode = "" } }
                    CalendarButton { width: 74; title: qsTr("验证"); enabled: ownerPassword.text.length > 0 && !archive.busy; reduceMotion: archivePage.reduceMotion; onClicked: { archiveController.authenticateOwner(ownerPassword.text); ownerPassword.text = "" } }
                }
            }
            Item {
                visible: archivePage.dialogMode === "classify" || archivePage.recoveryStage === "password"; anchors.fill: parent
                TextInput {
                    id: newPassword; objectName: "archiveNewPassword"; x: 18; y: 50; width: 294; height: 32; leftPadding: 11; rightPadding: 11; echoMode: TextInput.Password
                    color: Appearance.tone("#EEEEEE"); selectionColor: "#66FFFA00"; verticalAlignment: TextInput.AlignVCenter; font.family: harmonyRegular.name; font.pixelSize: 12
                    Rectangle { anchors.fill: parent; z: -1; color: Appearance.tone("#202020"); border.width: archivePage.formError ? 2 : 1; border.color: archivePage.formError ? "#D63235" : parent.activeFocus ? Appearance.accent("#FFFA00") : Appearance.tone("#505050") }
                    Text { x: 11; anchors.verticalCenter: parent.verticalCenter; visible: !newPassword.text.length && !newPassword.activeFocus; text: qsTr("新密码"); color: Appearance.tone("#777777"); font.family: harmonyRegular.name; font.pixelSize: 12 }
                    onAccepted: confirmPassword.forceActiveFocus()
                }
                TextInput {
                    id: confirmPassword; objectName: "archiveConfirmPassword"; x: 18; y: 90; width: 294; height: 32; leftPadding: 11; rightPadding: 11; echoMode: TextInput.Password
                    color: Appearance.tone("#EEEEEE"); selectionColor: "#66FFFA00"; verticalAlignment: TextInput.AlignVCenter; font.family: harmonyRegular.name; font.pixelSize: 12
                    Rectangle { anchors.fill: parent; z: -1; color: Appearance.tone("#202020"); border.width: archivePage.formError ? 2 : 1; border.color: archivePage.formError ? "#D63235" : parent.activeFocus ? Appearance.accent("#FFFA00") : Appearance.tone("#505050") }
                    Text { x: 11; anchors.verticalCenter: parent.verticalCenter; visible: !confirmPassword.text.length && !confirmPassword.activeFocus; text: qsTr("确认密码"); color: Appearance.tone("#777777"); font.family: harmonyRegular.name; font.pixelSize: 12 }
                    onAccepted: archivePage.submitPasswordForm()
                }
                Text { x: 18; y: 126; width: 294; visible: archivePage.formError.length > 0; text: archivePage.formError; color: "#D63235"; elide: Text.ElideRight; font.family: harmonyRegular.name; font.pixelSize: 10; renderType: Text.CurveRendering }
                Row { anchors.right: parent.right; anchors.rightMargin: 18; y: 161; spacing: 8
                    CalendarButton { visible: archivePage.dialogMode === "recover"; width: 82; title: qsTr("取消机密"); kind: "secondary"; enabled: !archive.busy; reduceMotion: archivePage.reduceMotion; onClicked: archiveController.recoverSelected("", "", true) }
                    CalendarButton { width: 64; title: qsTr("取消"); kind: "secondary"; reduceMotion: archivePage.reduceMotion; onClicked: { archiveController.cancelRecovery(); archivePage.dialogMode = "" } }
                    CalendarButton { width: 74; title: archivePage.dialogMode === "classify" ? qsTr("设为机密") : qsTr("保存"); enabled: !archive.busy; reduceMotion: archivePage.reduceMotion; onClicked: archivePage.submitPasswordForm() }
                }
            }
            Text { x: 18; y: parent.height - 18; width: 294; visible: archivePage.dialogMode === "recover" && archivePage.recoveryStage === "owner" && archivePage.formError.length > 0; text: archivePage.formError; color: "#D63235"; elide: Text.ElideRight; font.family: harmonyRegular.name; font.pixelSize: 10; renderType: Text.CurveRendering }
        }
    }

    Item {
        x: 320; y: 424; width: 270; height: 58; z: 300; opacity: archivePage.toastOpen ? 1 : 0; visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: archivePage.reduceMotion ? 0 : 250 } }
        Rectangle { anchors.fill: parent; color: "#F0181818"; border.color: Appearance.tone("#505050") }
        Rectangle { width: 4; height: parent.height; color: Appearance.accent("#FFFA00") }
        Text { x: 18; y: 10; width: 240; text: archivePage.toastTitle; color: Appearance.tone("#F2F2F2"); elide: Text.ElideRight; font.family: harmonyBold.name; font.pixelSize: 14; font.weight: Font.Bold; renderType: Text.CurveRendering }
        Text { x: 18; y: 33; width: 240; text: archivePage.toastMessage; color: Appearance.tone("#A0A0A0"); elide: Text.ElideRight; font.family: harmonyRegular.name; font.pointSize: 11.5 * 0.75; renderType: Text.CurveRendering }
    }
}
