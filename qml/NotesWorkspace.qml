import QtQuick
import "Appearance.js" as Appearance

Item {
    id: notesWorkspace
    width: 1920; height: 1080
    property string selectedModule: ""
    property bool presented: false
    property bool reduceMotion: false
    property string selectedId: ""
    property string pendingDeleteId: ""
    property real drawingWidth: 8
    property string drawingColor: "#FAD40A"
    property bool drawingErasing: false
    readonly property var notes: notesController.state.notes || []
    readonly property var drawingColors: [Appearance.tone("#EEEEEE"), Appearance.accent("#FFFA00"), "#22A6F2", "#E5383B", "#8CE03C"]
    DropArea {
        anchors.fill: parent; enabled: notesWorkspace.presented && notesWorkspace.selectedModule === "notes"
        onEntered: drag => { drag.accepted = drag.hasUrls && drag.urls.length === 1 }
        onDropped: drop => { notesController.importMedia(drop.urls[0], drop.x, drop.y); drop.accepted = true }
    }

    function cycleDrawingColor() {
        const index = drawingColors.indexOf(drawingColor.toUpperCase())
        drawingColor = drawingColors[(index + 1) % drawingColors.length]
        drawingErasing = false
    }
    function select(id) { selectedId = id; pendingDeleteId = "" }
    // Keep the same editor/player instance when another note changes or saves.
    ListModel { id: cards; dynamicRoles: true }
    function syncCards() {
        const values = notesWorkspace.notes
        for (let i = cards.count - 1; i >= 0; --i)
            if (!values.some(note => note.id === cards.get(i).noteId)) cards.remove(i)
        for (const note of values) {
            let i = 0
            while (i < cards.count && cards.get(i).noteId !== note.id) ++i
            if (i === cards.count) cards.append({noteId: note.id, noteData: note})
            else cards.setProperty(i, "noteData", note)
        }
    }
    Component.onCompleted: syncCards()

    Connections {
        target: notesController
        function onStateChanged() {
            notesWorkspace.syncCards()
            if (notesWorkspace.selectedId && !notesWorkspace.notes.some(note => note.id === notesWorkspace.selectedId)) notesWorkspace.selectedId = ""
            if (notesWorkspace.pendingDeleteId && !notesWorkspace.notes.some(note => note.id === notesWorkspace.pendingDeleteId)) notesWorkspace.pendingDeleteId = ""
        }
    }
    Repeater {
        model: cards
        delegate: NoteCard {
            required property var noteData
            entry: noteData; host: notesWorkspace
            selected: notesWorkspace.selectedId === noteData.id
            deleteArmed: notesWorkspace.pendingDeleteId === noteData.id
            reduceMotion: notesWorkspace.reduceMotion
            visible: notesWorkspace.presented && (notesWorkspace.selectedModule === "notes" || noteData.pinned)
            enabled: visible
            opacity: visible ? 1 : 0
            onSelectedRequested: id => notesWorkspace.select(id)
            onGeometryCommitted: (id, x, y, width, height) => notesController.updateNoteGeometry(id, x, y, width, height)
            onDeleteRequested: id => { notesWorkspace.selectedId = id; notesWorkspace.pendingDeleteId = id }
            onDeleteCancelled: notesWorkspace.pendingDeleteId = ""
            onDeleteConfirmed: id => { notesWorkspace.pendingDeleteId = ""; notesController.removeNote(id) }
        }
    }
}
