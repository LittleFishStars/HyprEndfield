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

    function cycleDrawingColor() {
        const index = drawingColors.indexOf(drawingColor.toUpperCase())
        drawingColor = drawingColors[(index + 1) % drawingColors.length]
        drawingErasing = false
    }
    function select(id) { selectedId = id; pendingDeleteId = "" }

    Connections {
        target: notesController
        function onStateChanged() {
            if (notesWorkspace.selectedId && !notesWorkspace.notes.some(note => note.id === notesWorkspace.selectedId)) notesWorkspace.selectedId = ""
            if (notesWorkspace.pendingDeleteId && !notesWorkspace.notes.some(note => note.id === notesWorkspace.pendingDeleteId)) notesWorkspace.pendingDeleteId = ""
        }
    }
    Repeater {
        model: notesWorkspace.notes
        delegate: NoteCard {
            required property var modelData
            entry: modelData; host: notesWorkspace
            selected: notesWorkspace.selectedId === modelData.id
            deleteArmed: notesWorkspace.pendingDeleteId === modelData.id
            reduceMotion: notesWorkspace.reduceMotion
            visible: notesWorkspace.presented && (notesWorkspace.selectedModule === "notes" || modelData.pinned)
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
