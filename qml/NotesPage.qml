import QtQuick
import "Appearance.js" as Appearance
import QtQuick.Dialogs
import QtQuick.Shapes

// EndfieldHUD NotesCanvas, MOD-01. Video notes remain intentionally deferred.
Item {
    id: notesPage
    width: 590; height: 510; focus: presented
    property bool presented: false
    property bool reduceMotion: false
    readonly property bool redacted: Appearance.recording()
    onRedactedChanged: if (redacted) { flushText(); rootFocus.forceActiveFocus(); formatBar.closeMenus() }
    readonly property var notes: notesController.state
    readonly property var centre: notes.centre
    readonly property var tasks: centre.tasks || []
    readonly property real todoListHeight: tasks.length === 0 ? 126 : Math.min(250, tasks.length * 66 - 6)
    readonly property real addBarY: centre.folded ? 89 : centre.tab === "text" ? 384 : 125 + todoListHeight + 8 + 32 + 12
    property string textSnapshot: ""
    property string toastTitle: ""
    property string toastDetail: ""
    property bool toastOpen: false
    property real importX: 865
    property real importY: 420

    function showToast(title, detail) {
        toastTitle = title; toastDetail = detail || ""; toastOpen = true; toastTimer.restart()
    }
    function selectTab(tab) {
        if (tab === centre.tab) return
        flushText(); formatBar.closeMenus(); notesController.setCentreTab(tab)
    }
    function flushText() {
        if (textEditor.text !== textSnapshot) {
            notesController.setCentreText(textEditor.text)
            textSnapshot = textEditor.text
        }
    }
    function submitTask() {
        const value = taskField.text.trim()
        if (!value) { taskField.invalid = true; taskField.edit(); return }
        if (notesController.addCentreTask(value)) { taskField.text = ""; taskField.invalid = false }
    }
    function creationPoint() {
        const offset = (notes.total % 7) * 18
        return { x: 865 + offset, y: 420 + offset }
    }
    function create(kind) {
        const point = creationPoint()
        if (kind === "image") {
            importX = point.x; importY = point.y; imageDialog.open(); return
        }
        notesController.createNote(kind, point.x, point.y)
    }

    Component.onCompleted: { textEditor.text = centre.text || ""; textSnapshot = textEditor.text }
    onPresentedChanged: {
        if (presented) rootFocus.forceActiveFocus()
        else { flushText(); formatBar.closeMenus() }
    }
    Component.onDestruction: flushText()
    Connections {
        target: notesController
        function onStateChanged() {
            if (!textEditor.activeFocus && textEditor.text !== notesPage.centre.text) {
                textEditor.text = notesPage.centre.text || ""
                notesPage.textSnapshot = textEditor.text
            }
            if (notesPage.notes.error) notesPage.showToast(qsTr("便笺"), notesPage.notes.error)
        }
        function onActionFinished(action, success, message) { notesPage.showToast(success ? qsTr("便笺") : qsTr("提示"), message) }
    }
    Item { id: rootFocus; anchors.fill: parent; focus: true }
    Timer { id: textSave; interval: 450; onTriggered: notesPage.flushText() }
    Timer { id: toastTimer; interval: 1600; onTriggered: notesPage.toastOpen = false }
    Shortcut { sequence: "Q"; enabled: notesPage.presented && !textEditor.activeFocus && !taskField.fieldFocused; onActivated: notesPage.selectTab("text") }
    Shortcut { sequence: "E"; enabled: notesPage.presented && !textEditor.activeFocus && !taskField.fieldFocused; onActivated: notesPage.selectTab("todo") }
    FileDialog {
        id: imageDialog
        title: qsTr("选择图片")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("图片文件 (*.png *.jpg *.jpeg *.webp *.bmp *.gif *.tif *.tiff)"), qsTr("所有文件 (*)")]
        onAccepted: notesController.importImage(selectedFile, notesPage.importX, notesPage.importY)
    }

    Item {
        width: 590; height: 81
        Row {
            y: 1.75; spacing: 6
            Repeater {
                model: ["// HUD", "/", "NOTES"]
                Text {
                    required property string modelData; required property int index
                    text: modelData; color: index === 2 ? Appearance.tone("#E6E6E6") : Appearance.tone("#8A8A8A")
                    font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; font.letterSpacing: 0.63
                    renderType: Text.CurveRendering
                }
            }
        }
        Text {
            x: 549 - width; y: 1.75; text: "MOD-01"; color: Appearance.tone("#8A8A8A")
            font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; font.letterSpacing: 0.63
            renderType: Text.CurveRendering
        }
        Item {
            x: 556; y: 3.5; width: 34; height: 7; clip: true
            Shape {
                anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
                ShapePath { strokeWidth: -1; fillColor: Qt.rgba(1, 1, 1, 0.14); PathSvg { path: { let p = ""; for (let x = -7; x < 41; x += 3.5 * Math.SQRT2) p += "M" + x + " 7h1.414l7 -7h-1.414Z"; return p } } }
            }
        }
        Text {
            id: title; y: 20; height: 30; text: qsTr("便笺"); color: Appearance.tone("#F4F4F4")
            font.family: harmonyBold.name; font.pixelSize: 26; font.weight: Font.Bold; font.letterSpacing: 2.08
            renderType: Text.CurveRendering
        }
        Text {
            x: title.width + 12; y: title.y + title.baselineOffset - baselineOffset; text: "NOTES"; color: Appearance.tone("#8A8A8A")
            font.family: harmonyMedium.name; font.pixelSize: 13; font.letterSpacing: 2.6; font.weight: Font.Medium
            renderType: Text.CurveRendering
        }
        CalendarButton {
            x: 510; y: 22; width: 80; title: notesPage.centre.folded ? qsTr("显示") : qsTr("隐藏"); kind: "secondary"
            reduceMotion: notesPage.reduceMotion
            onClicked: { notesPage.flushText(); notesController.setCentreFolded(!notesPage.centre.folded) }
        }
        Item {
            y: 60; width: 590; height: 9
            Rectangle { y: 4; width: 590; height: 1; color: "#38FFFFFF" }
            Rectangle { y: 3; width: 56; height: 3; color: Appearance.accent("#FFFA00") }
            Repeater { model: 10; Rectangle { required property int index; x: 454 + index * 9; width: 1; height: 9; color: "#59FFFFFF" } }
            Rectangle { x: 590 - width; y: -2; width: counter.width + 7; height: 12; color: Appearance.tone("#151515") }
            Text {
                id: counter; x: 590 - width; y: -1.5
                text: notesPage.centre.tab === "todo"
                    ? String(notesPage.centre.done).padStart(2, "0") + "/" + String(notesPage.tasks.length).padStart(2, "0") : "01"
                color: Appearance.accent("#FFFA00"); font.family: harmonyBold.name; font.pixelSize: 11; font.weight: Font.Bold
                renderType: Text.CurveRendering
            }
        }
    }

    Item {
        id: body; x: 2; y: 85; width: 586; height: notesPage.centre.folded ? 6 : notesPage.addBarY - 85 - 10
        clip: true; opacity: notesPage.centre.folded ? 0 : 1
        transform: Translate { y: notesPage.centre.folded ? -8 : 0 }
        Behavior on height { NumberAnimation { duration: notesPage.reduceMotion ? 0 : 320; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: notesPage.reduceMotion ? 0 : 240 } }
        Rectangle { anchors.fill: parent; color: "transparent" }

        Item {
            id: tabs; width: 586; height: 34
            Repeater {
                model: [{key: "text", title: qsTr("文字")}, {key: "todo", title: qsTr("待办")}]
                delegate: Item {
                    id: tabButton; required property var modelData; required property int index
                    x: index * 293; width: 293; height: 34
                    readonly property bool selected: notesPage.centre.tab === modelData.key
                    Rectangle { anchors.fill: parent; color: tabMouse.containsMouse ? "#12FFFFFF" : "transparent" }
                    Rectangle { y: 32; width: parent.width; height: selected ? 2 : 1; color: selected ? Appearance.accent("#FFFA00") : Appearance.tone("#4A4A4A") }
                    Text { anchors.centerIn: parent; text: modelData.title; color: selected ? Appearance.tone("#EDEDED") : Appearance.tone("#8A8A8A"); font.family: selected ? harmonyBold.name : harmonyMedium.name; font.pixelSize: 13; font.weight: selected ? Font.Bold : Font.Medium; font.letterSpacing: 0.52; renderType: Text.CurveRendering }
                    Shape {
                        x: parent.width - 8; y: -3; width: 10; height: 10; visible: modelData.key === "todo" && notesPage.centre.hasDot
                        ShapePath { strokeWidth: -1; fillColor: "#F2A23A"; PathSvg { path: "M5 0L10 5L5 10L0 5Z" } }
                    }
                    MouseArea { id: tabMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: notesPage.selectTab(modelData.key) }
                }
            }
        }

        Item {
            id: textPanel; y: 44; width: 586; height: 243; visible: notesPage.centre.tab === "text"
            Rectangle { anchors.fill: parent; color: Appearance.tone("#1C1C1C"); border.color: Appearance.tone("#4B4B4B") }
            NotesFormatBar {
                id: formatBar; x: 8; y: 8; style: notesPage.centre.style; reduceMotion: notesPage.reduceMotion
                onChanged: (key, value) => notesController.setCentreStyle(key, value)
            }
            Rectangle { y: 42; width: 586; height: 1; color: Appearance.tone("#444444") }
            TextEdit {
                id: textEditor; objectName: "notesTextEditor"
                visible: !notesPage.redacted; readOnly: notesPage.redacted
                x: 12; y: 52; width: 562; height: 180; clip: true
                wrapMode: TextEdit.Wrap; selectByMouse: true; persistentSelection: true
                color: notesPage.centre.style.color
                selectionColor: "#66FFFA00"; selectedTextColor: Appearance.tone("#FFFFFF")
                font.family: notesPage.centre.style.family
                font.pixelSize: notesPage.centre.style.size
                font.bold: notesPage.centre.style.bold
                font.italic: notesPage.centre.style.italic
                font.underline: notesPage.centre.style.underline
                font.strikeout: notesPage.centre.style.strikeout
                renderType: Text.CurveRendering
                onTextChanged: if (activeFocus) textSave.restart()
                onActiveFocusChanged: { if (!activeFocus) notesPage.flushText(); else formatBar.closeMenus() }
                Keys.onEscapePressed: { focus = false; event.accepted = true }
            }
            Text { x: 12; y: 54; visible: !notesPage.redacted && textEditor.text === "" && !textEditor.activeFocus; text: qsTr("开始书写"); color: Appearance.tone("#737373"); font.family: harmonyRegular.name; font.pixelSize: 14; renderType: Text.CurveRendering }
            RedactionBlock { y: 43; width: 586; height: 200; visible: notesPage.redacted; content: notesPage.centre.text || "" }
        }

        Item {
            id: todoPanel; y: 44; width: 586; height: notesPage.todoListHeight + 40; visible: notesPage.centre.tab === "todo"
            ListView {
                id: taskList; objectName: "notesCentreTasks"; width: 586; height: notesPage.todoListHeight
                model: notesPage.tasks; spacing: 6; clip: true; boundsBehavior: Flickable.StopAtBounds
                delegate: NotesTaskCard {
                    required property var modelData; required property int index
                    task: modelData; reduceMotion: notesPage.reduceMotion
                    onToggled: notesController.toggleCentreTask(modelData.id)
                    onRemoved: notesController.removeCentreTask(modelData.id)
                    onFlagged: notesController.toggleCentreFlag(modelData.id)
                    onMoved: offset => notesController.moveCentreTask(modelData.id, offset)
                    onRenamed: text => notesController.updateCentreTask(modelData.id, text)
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter; y: 47; visible: notesPage.tasks.length === 0
                text: qsTr("NO TASKS\n待办为空"); color: Appearance.tone("#777777"); horizontalAlignment: Text.AlignHCenter
                font.family: harmonyMedium.name; font.pixelSize: 12; font.letterSpacing: 1.2; lineHeight: 1.8
                renderType: Text.CurveRendering
            }
            Item {
                y: notesPage.todoListHeight + 8; width: 586; height: 32
                CalendarTextField {
                    id: taskField; objectName: "notesTaskField"; width: 500; height: 32; maximumLength: 60
                    placeholder: qsTr("+ 添加事项"); reduceMotion: notesPage.reduceMotion; onAccepted: notesPage.submitTask()
                }
                CalendarButton { x: 508; width: 78; height: 32; title: qsTr("添加"); kind: "primary"; reduceMotion: notesPage.reduceMotion; onClicked: notesPage.submitTask() }
            }
        }
    }

    Row {
        id: addBar; objectName: "notesAddBar"; x: 0; y: notesPage.addBarY; spacing: 8
        Behavior on y { NumberAnimation { duration: notesPage.reduceMotion ? 0 : 320; easing.type: Easing.OutCubic } }
        Repeater {
            model: [{kind: "text", title: qsTr("文字")}, {kind: "todo", title: qsTr("待办")}, {kind: "image", title: qsTr("媒体")}, {kind: "pen", action: "drawing", title: qsTr("画画")}]
            delegate: NotesAddButton {
                required property var modelData
                width: 141.5; height: 40; title: modelData.title; kind: modelData.kind
                enabled: notesPage.notes.ready; reduceMotion: notesPage.reduceMotion
                onClicked: notesPage.create(modelData.action || modelData.kind)
            }
        }
    }

    Item {
        x: 320; y: 424; width: 270; height: 58; z: 300
        opacity: notesPage.toastOpen ? 1 : 0; visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: notesPage.reduceMotion ? 0 : 250 } }
        Rectangle { anchors.fill: parent; color: "#F0181818"; border.color: Appearance.tone("#505050") }
        Rectangle { width: 4; height: parent.height; color: Appearance.accent("#FFFA00") }
        Text { x: 18; y: 10; width: 240; text: notesPage.toastTitle; color: Appearance.tone("#F2F2F2"); elide: Text.ElideRight; font.family: harmonyBold.name; font.pixelSize: 14; font.weight: Font.Bold; renderType: Text.CurveRendering }
        Text { x: 18; y: 33; width: 240; text: notesPage.toastDetail; color: Appearance.tone("#A0A0A0"); elide: Text.ElideRight; font.family: harmonyRegular.name; font.pointSize: 11.5 * 0.75; renderType: Text.CurveRendering }
    }
}
