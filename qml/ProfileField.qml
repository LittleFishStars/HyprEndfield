import QtQuick
import "Appearance.js" as Appearance

Item {
    id: field
    required property string fieldName
    property var controller: profileController
    readonly property bool locked: (controller.state.lockedFields || []).includes(fieldName)
    onLockedChanged: if (locked && editing) { editing = false; field.forceActiveFocus(); }
    property color ink: "#F5F5F5"
    property color accent: Appearance.accent("#FFFA00")
    property real pixelSize: 13
    property int textLimit: 64
    property bool numeric: false
    property bool multiline: false
    property string placeholder: ""
    property bool editing: false
    function beginEditing() { if (locked) return; editing = true; editor.text = String(controller.state[fieldName]); editor.initial = editor.text; editor.forceActiveFocus(); editor.selectAll() }
    function finish() { if (editing) { if (!locked) controller.setField(fieldName, editor.text); editing = false; field.forceActiveFocus(); } }
    width: 100; height: 28
    onVisibleChanged: if (!visible) finish()
    Rectangle { anchors.fill: parent; color: editor.activeFocus ? "#212121" : mouse.containsMouse ? "#22FFFFFF" : "transparent"; border.width: editor.activeFocus ? 1 : 0; border.color: field.accent }
    Rectangle { visible: editor.activeFocus; width: 3; height: parent.height; color: field.accent }
    TextEdit {
        id: editor; objectName: "profileField_" + field.fieldName
        x: field.multiline ? 6 : 4; width: parent.width - x * 2; height: parent.height
        verticalAlignment: TextEdit.AlignVCenter; horizontalAlignment: field.numeric ? TextEdit.AlignRight : TextEdit.AlignLeft
        textFormat: TextEdit.PlainText; wrapMode: field.multiline ? TextEdit.Wrap : TextEdit.NoWrap; clip: true
        readOnly: !activeFocus; selectByMouse: true; color: field.ink; selectionColor: field.accent; selectedTextColor: "#111111"
        font.family: harmonyBold.name; font.pixelSize: field.pixelSize; font.bold: true; renderType: Text.CurveRendering
        property string initial: ""
        onActiveFocusChanged: if (!activeFocus && field.editing) field.finish()
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Escape) { field.editing = false; field.forceActiveFocus(); event.accepted = true; }
            else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && (!field.multiline || event.modifiers & Qt.ControlModifier)) { field.finish(); event.accepted = true; }
        }
        onTextChanged: if (activeFocus && text.length > field.textLimit) remove(field.textLimit, text.length)
        Binding { target: editor; property: "text"; value: String(field.controller.state[field.fieldName] ?? ""); when: !field.editing; restoreMode: Binding.RestoreNone }
    }
    Text { x: 6; anchors.verticalCenter: parent.verticalCenter; visible: !editor.text && !editor.activeFocus; text: field.placeholder; font.family: harmonyRegular.name; font.pixelSize: field.pixelSize; color: field.ink; renderType: Text.CurveRendering }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; enabled: !editor.activeFocus && !field.locked; cursorShape: Qt.IBeamCursor; onClicked: field.beginEditing() }
}
