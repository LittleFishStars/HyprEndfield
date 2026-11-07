import QtQuick
import "Appearance.js" as Appearance

Item {
    id: field
    property alias text: editor.text
    property string placeholder: ""
    property int maximumLength: 120
    property bool invalid: false
    property bool reduceMotion: false
    readonly property bool fieldFocused: editor.activeFocus
    signal accepted()
    width: 254; height: 32
    function edit() { editor.forceActiveFocus(); editor.selectAll(); }

    Rectangle {
        anchors.fill: parent; color: Appearance.tone("#1B1B1B")
        border.width: editor.activeFocus || field.invalid ? 2 : 1
        border.color: field.invalid ? "#E5383B" : editor.activeFocus ? Appearance.accent("#FFFA00") : Appearance.tone("#494949")
    }
    Rectangle { x: 0; y: 8; width: 3; height: 16; color: field.invalid ? "#E5383B" : Appearance.accent("#FFFA00"); visible: editor.activeFocus || field.invalid }
    TextInput {
        id: editor; x: 10; width: parent.width - 20; height: parent.height
        verticalAlignment: TextInput.AlignVCenter; clip: true; selectByMouse: true
        color: Appearance.tone("#E6E6E6"); selectionColor: "#59FFFA00"; selectedTextColor: Appearance.tone("#FFFFFF")
        font.family: harmonyRegular.name; font.pixelSize: 12; renderType: Text.CurveRendering
        onTextEdited: { if (text.length > field.maximumLength) text = text.slice(0, field.maximumLength); field.invalid = false }
        onAccepted: field.accepted()
    }
    Text {
        x: 10; anchors.verticalCenter: parent.verticalCenter; visible: editor.text === "" && !editor.activeFocus
        text: field.placeholder; color: Appearance.tone("#777777"); font.family: harmonyRegular.name; font.pixelSize: 12; renderType: Text.CurveRendering
    }
}
