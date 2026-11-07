import QtQuick
import QtQuick.Shapes

Item {
    id: card
    objectName: "noteCard-" + entry.id
    required property var entry
    required property var host
    property bool selected: false
    property bool reduceMotion: false
    property bool deleteArmed: false
    signal selectedRequested(string id)
    signal geometryCommitted(string id, real x, real y, real width, real height)
    signal deleteRequested(string id)
    signal deleteCancelled()
    signal deleteConfirmed(string id)

    x: entry.x; y: entry.y; width: entry.width; height: entry.height
    z: entry.z + (selected ? 100000 : 0)
    opacity: 1
    transform: Scale {
        origin.x: card.width / 2; origin.y: card.height / 2
        xScale: card.visible ? 1 : 0.94; yScale: xScale
    }
    Behavior on opacity { NumberAnimation { duration: card.reduceMotion ? 0 : 200 } }

    Shape {
        anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: card.selected ? "#FFFA00" : "#555555"; strokeWidth: 1
            fillColor: "#1C1C1C"
            PathSvg { path: "M.5 .5H" + (card.width - 8) + "L" + (card.width - .5) + " 8V" + (card.height - .5) + "H.5Z" }
        }
    }
    Shape {
        width: parent.width; height: 24; preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1; fillColor: "#262626"
            PathSvg { path: "M1 1H" + (card.width - 8) + "L" + (card.width - 1) + " 8V24H1Z" }
        }
    }
    Rectangle { x: 1; y: 1; width: 2; height: 22; color: "#FFFA00"; visible: card.selected }
    Grid {
        x: 7; y: 6; columns: 2; rowSpacing: 2; columnSpacing: 2
        Repeater { model: 6; Rectangle { width: 2; height: 2; radius: 1; color: "#818181" } }
    }
    Text {
        x: 20; y: 6; width: Math.max(10, card.width - 76); height: 12
        text: card.entry.kind === "text" ? "文字" : card.entry.kind === "todo" ? "待办" : card.entry.kind === "drawing" ? "画画" : "图片"
        color: "#8A8A8A"; font.family: harmonyBold.name; font.pointSize: 9.5 * 0.75; font.weight: Font.Bold; font.letterSpacing: 1.14
        elide: Text.ElideRight; renderType: Text.CurveRendering
    }
    MouseArea {
        id: moveArea; x: 0; y: 0; width: Math.max(20, card.width - 50); height: 24
        hoverEnabled: true; cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        property real startX; property real startY; property real originalX; property real originalY
        onPressed: mouse => {
            card.selectedRequested(card.entry.id)
            startX = mouse.x; startY = mouse.y; originalX = card.x; originalY = card.y
        }
        onPositionChanged: mouse => {
            if (!pressed) return
            card.x = Math.max(0, Math.min(card.host.width - card.width, originalX + mouse.x - startX))
            card.y = Math.max(0, Math.min(card.host.height - card.height, originalY + mouse.y - startY))
        }
        onReleased: card.geometryCommitted(card.entry.id, card.x, card.y, card.width, card.height)
    }
    Item {
        x: card.width - 44; y: 3; width: 18; height: 18
        Rectangle { anchors.fill: parent; color: pinMouse.containsMouse || card.entry.pinned ? "#FFFA00" : "transparent" }
        NotesGlyph { anchors.centerIn: parent; width: 10; height: 10; kind: "pin"; ink: card.entry.pinned || pinMouse.containsMouse ? "#151515" : "#A0A0A0" }
        MouseArea { id: pinMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { card.selectedRequested(card.entry.id); notesController.toggleNotePin(card.entry.id) } }
    }
    Item {
        x: card.width - 23; y: 3; width: 18; height: 18
        Rectangle { anchors.fill: parent; color: deleteMouse.containsMouse ? "#E5383B" : "transparent" }
        NotesGlyph { anchors.centerIn: parent; width: 9; height: 9; kind: "close"; ink: deleteMouse.containsMouse ? "#FFFFFF" : "#A0A0A0" }
        MouseArea { id: deleteMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: card.deleteRequested(card.entry.id) }
    }

    Item {
        id: textBody; x: 9; y: 29; width: card.width - 18; height: Math.max(1, card.height - 37)
        visible: card.entry.kind === "text"
        TextEdit {
            id: noteText; anchors.fill: parent; clip: true; wrapMode: TextEdit.Wrap; selectByMouse: true
            text: card.entry.text; color: "#EDEDED"; selectionColor: "#66FFFA00"; selectedTextColor: "#FFFFFF"
            font.family: harmonyRegular.name; font.pixelSize: 12; renderType: Text.CurveRendering
            onActiveFocusChanged: {
                if (activeFocus) card.selectedRequested(card.entry.id)
                else if (text !== card.entry.text) notesController.updateNoteText(card.entry.id, text)
            }
            Keys.onEscapePressed: event => { focus = false; event.accepted = true }
        }
        Text { visible: noteText.text === "" && !noteText.activeFocus; text: "开始书写"; color: "#777777"; font.family: harmonyRegular.name; font.pixelSize: 12; renderType: Text.CurveRendering }
    }

    Item {
        id: todoBody; x: 5; y: 27; width: card.width - 10; height: card.height - 31
        visible: card.entry.kind === "todo"
        ListView {
            id: noteTasks; width: parent.width; height: parent.height - 28; clip: true
            model: card.entry.tasks; spacing: 1; boundsBehavior: Flickable.StopAtBounds
            delegate: Item {
                id: taskRow; required property var modelData; required property int index
                width: noteTasks.width; height: 25
                Rectangle { y: 24; width: parent.width; height: 1; color: "#303030" }
                Rectangle {
                    x: 3; y: 5; width: 14; height: 14; color: modelData.checked ? "#FFFA00" : "transparent"
                    border.color: modelData.checked ? "#FFFA00" : "#777777"
                    NotesGlyph { anchors.centerIn: parent; width: 9; height: 9; kind: "check"; ink: "#181818"; visible: modelData.checked }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: notesController.toggleNoteTask(card.entry.id, modelData.id) }
                }
                TextInput {
                    id: taskText; x: 23; y: 2; width: Math.max(20, taskRow.width - 88); height: 21
                    text: modelData.text; color: modelData.checked || text === "" ? "#8A8A8A" : "#EDEDED"
                    font.family: harmonyRegular.name; font.pixelSize: 11; selectByMouse: true
                    font.strikeout: modelData.checked; renderType: Text.CurveRendering
                    onAccepted: { if (text.trim()) notesController.updateNoteTask(card.entry.id, modelData.id, text); focus = false }
                    onActiveFocusChanged: {
                        if (activeFocus) card.selectedRequested(card.entry.id)
                        else if (text.trim() && text.trim() !== modelData.text) notesController.updateNoteTask(card.entry.id, modelData.id, text)
                    }
                }
                Row {
                    x: taskRow.width - 61; y: 3; spacing: 2
                    Repeater {
                        model: [{kind: "up", offset: -1}, {kind: "down", offset: 1}, {kind: "close", offset: 0}]
                        delegate: Item {
                            required property var modelData; width: 18; height: 18
                            Rectangle { anchors.fill: parent; color: rowMouse.containsMouse ? "#343434" : "transparent" }
                            NotesGlyph { anchors.centerIn: parent; width: 9; height: 9; kind: modelData.kind; ink: "#A0A0A0" }
                            MouseArea { id: rowMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { if (modelData.offset === 0) notesController.removeNoteTask(card.entry.id, taskRow.modelData.id); else notesController.moveNoteTask(card.entry.id, taskRow.modelData.id, modelData.offset) } }
                        }
                    }
                }
            }
        }
        Item {
            y: parent.height - 25; width: parent.width; height: 22
            TextInput {
                id: addTask; x: 5; width: parent.width - 30; height: 22
                color: "#EDEDED"; font.family: harmonyRegular.name; font.pixelSize: 11; selectByMouse: true
                renderType: Text.CurveRendering
                onAccepted: { if (text.trim() && notesController.addNoteTask(card.entry.id, text)) text = "" }
            }
            Text { x: 5; height: 22; verticalAlignment: Text.AlignVCenter; visible: addTask.text === "" && !addTask.activeFocus; text: "+ 添加事项"; color: "#777777"; font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering }
            Item {
                x: parent.width - 22; width: 22; height: 22
                Rectangle { anchors.fill: parent; color: addMouse.containsMouse ? "#FFFA00" : "#2B2B2B" }
                Text { anchors.centerIn: parent; text: "+"; color: addMouse.containsMouse ? "#161616" : "#EDEDED"; font.family: harmonyBold.name; font.pixelSize: 14; font.weight: Font.Bold }
                MouseArea { id: addMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { if (addTask.text.trim() && notesController.addNoteTask(card.entry.id, addTask.text)) addTask.text = ""; else addTask.forceActiveFocus() } }
            }
        }
    }

    Item {
        id: imageBody; x: 5; y: 29; width: card.width - 10; height: card.height - 35
        visible: card.entry.kind === "image"; clip: true
        Image { anchors.fill: parent; source: card.entry.imageUrl; fillMode: Image.PreserveAspectFit; asynchronous: true; cache: true; smooth: true; mipmap: true }
        Text { anchors.centerIn: parent; visible: !card.entry.imageAvailable; text: "图片不可用"; color: "#8A8A8A"; font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering }
        MouseArea { anchors.fill: parent; onPressed: card.selectedRequested(card.entry.id) }
    }

    Item {
        id: drawingBody; x: 5; y: 29; width: card.width - 10; height: Math.max(1, card.height - 56)
        visible: card.entry.kind === "drawing"; clip: true
        Canvas {
            id: drawingCanvas; anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                const all = card.entry.strokes || []
                function drawStroke(stroke, points) {
                    if (!points || points.length === 0) return
                    ctx.beginPath(); ctx.lineCap = "round"; ctx.lineJoin = "round"
                    ctx.lineWidth = stroke.width; ctx.strokeStyle = stroke.color
                    ctx.moveTo(points[0].x * width, points[0].y * height)
                    if (points.length === 1) ctx.lineTo(points[0].x * width + 0.01, points[0].y * height)
                    for (let i = 1; i < points.length; ++i) ctx.lineTo(points[i].x * width, points[i].y * height)
                    ctx.stroke()
                }
                for (let i = 0; i < all.length; ++i) drawStroke(all[i], all[i].points)
                if (!card.host.drawingErasing) drawStroke({width: card.host.drawingWidth, color: card.host.drawingColor}, drawInput.points)
            }
        }
        MouseArea {
            id: drawInput; anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true; cursorShape: Qt.CrossCursor
            property var points: []
            function addPoint(x, y) {
                const next = {x: Math.max(0, Math.min(1, x / width)), y: Math.max(0, Math.min(1, y / height))}
                const copy = points.slice(); const last = copy.length ? copy[copy.length - 1] : null
                if (!last || Math.hypot((last.x - next.x) * width, (last.y - next.y) * height) >= 0.8) copy.push(next)
                points = copy; drawingCanvas.requestPaint()
            }
            onPressed: mouse => {
                card.selectedRequested(card.entry.id)
                if (mouse.button === Qt.RightButton) { card.host.drawingErasing = !card.host.drawingErasing; points = []; return }
                points = []; addPoint(mouse.x, mouse.y)
            }
            onPositionChanged: mouse => { if (pressed && (pressedButtons & Qt.LeftButton)) addPoint(mouse.x, mouse.y) }
            onReleased: mouse => {
                if (mouse.button !== Qt.LeftButton || points.length === 0) return
                if (card.host.drawingErasing)
                    notesController.eraseDrawingPath(card.entry.id, points, card.host.drawingWidth / Math.max(1, Math.min(width, height)) / 2)
                else notesController.appendStroke(card.entry.id, points, card.host.drawingWidth, card.host.drawingColor)
                points = []; drawingCanvas.requestPaint()
            }
            onWheel: wheel => {
                card.host.drawingWidth = Math.max(1, Math.min(80, card.host.drawingWidth + (wheel.angleDelta.y > 0 ? 1 : -1)))
                wheel.accepted = true
            }
        }
        Connections { target: notesController; function onStateChanged() { drawingCanvas.requestPaint() } }
    }
    Row {
        x: 5; y: card.height - 25; spacing: 5; visible: card.entry.kind === "drawing"
        Item {
            width: 20; height: 20
            Rectangle { anchors.fill: parent; color: card.host.drawingColor; border.color: "#777777" }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: card.host.cycleDrawingColor() }
        }
        Rectangle {
            width: 54; height: 20; color: "#282828"; border.color: "#4B4B4B"
            Text { anchors.centerIn: parent; text: Math.round(card.host.drawingWidth) + " px"; color: "#EDEDED"; font.family: harmonyRegular.name; font.pixelSize: 10; renderType: Text.CurveRendering }
            MouseArea { anchors.fill: parent; onWheel: wheel => { card.host.drawingWidth = Math.max(1, Math.min(80, card.host.drawingWidth + (wheel.angleDelta.y > 0 ? 1 : -1))); wheel.accepted = true } }
        }
        Item {
            width: 20; height: 20
            Rectangle { anchors.fill: parent; color: card.host.drawingErasing ? "#FFFA00" : eraserMouse.containsMouse ? "#373737" : "#282828"; border.color: "#4B4B4B" }
            NotesGlyph { anchors.centerIn: parent; width: 11; height: 11; kind: "eraser"; ink: card.host.drawingErasing ? "#151515" : "#EDEDED" }
            MouseArea { id: eraserMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: card.host.drawingErasing = !card.host.drawingErasing }
        }
        Item {
            width: 20; height: 20
            Rectangle { anchors.fill: parent; color: clearMouse.containsMouse ? "#E5383B" : "#282828"; border.color: "#4B4B4B" }
            NotesGlyph { anchors.centerIn: parent; width: 10; height: 10; kind: "close"; ink: "#EDEDED" }
            MouseArea { id: clearMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: notesController.clearDrawing(card.entry.id) }
        }
    }

    Canvas {
        anchors.fill: parent; z: 90
        onPaint: {
            const ctx = getContext("2d"); ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = card.selected ? "#FFFA00" : "#8A8A8A"; ctx.lineWidth = 1
            ctx.beginPath(); ctx.moveTo(width - 11, height - 3); ctx.lineTo(width - 3, height - 11)
            ctx.moveTo(width - 7, height - 3); ctx.lineTo(width - 3, height - 7); ctx.stroke()
        }
    }
    MouseArea {
        id: resizeArea; x: card.width - 16; y: card.height - 16; width: 16; height: 16; z: 100
        cursorShape: Qt.SizeFDiagCursor
        property real startX; property real startY; property real originalWidth; property real originalHeight
        onPressed: mouse => { card.selectedRequested(card.entry.id); startX = mouse.x; startY = mouse.y; originalWidth = card.width; originalHeight = card.height }
        onPositionChanged: mouse => {
            if (!pressed) return
            const minimumWidth = card.entry.kind === "todo" ? 180 : 110
            const minimumHeight = card.entry.kind === "todo" ? 105 : 70
            card.width = Math.max(minimumWidth, Math.min(card.host.width - card.x, originalWidth + mouse.x - startX))
            card.height = Math.max(minimumHeight, Math.min(card.host.height - card.y, originalHeight + mouse.y - startY))
            if (card.entry.kind === "drawing") drawingCanvas.requestPaint()
        }
        onReleased: card.geometryCommitted(card.entry.id, card.x, card.y, card.width, card.height)
    }

    Row {
        x: Math.min(card.host.width - card.x - 56, Math.max(0, card.width - 61)); y: 27; spacing: 6; z: 150
        visible: card.deleteArmed
        Item {
            width: 25; height: 25
            Rectangle { anchors.fill: parent; color: cancelMouse.containsMouse ? "#353535" : "#242424"; border.color: "#555555" }
            NotesGlyph { anchors.centerIn: parent; width: 11; height: 11; kind: "close"; ink: "#EEEEEE" }
            MouseArea { id: cancelMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: card.deleteCancelled() }
        }
        Item {
            width: 25; height: 25
            Rectangle { anchors.fill: parent; color: "#FFFA00" }
            NotesGlyph { anchors.centerIn: parent; width: 11; height: 11; kind: "check"; ink: "#151515" }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: card.deleteConfirmed(card.entry.id) }
        }
    }
}
