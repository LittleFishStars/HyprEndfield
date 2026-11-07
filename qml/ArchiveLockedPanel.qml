import QtQuick
import "Appearance.js" as Appearance

// EndfieldHUD .clsdoc: hatched redactions, rotated stamp and 52 px password bar.
Rectangle {
    id: panel
    required property var document
    property bool reduceMotion: false
    property real shakeOffset: 0
    property int openEpoch: 0
    property int sealEpoch: 0
    property string revealedTitle: ""
    property string revealedBody: ""
    signal unlock(string password)
    signal recover()
    width: 590; height: 429; color: Appearance.tone("#141414"); border.color: Appearance.tone("#333333"); clip: true

    function shake() { if (!reduceMotion) shakeAnimation.restart() }
    function opening() { openEpoch += 1; openingAnimation.restart(); return reduceMotion ? 0 : Math.min(16, document.redactionWidths.length + 1) * 70 + 520 }
    function sealing() { sealEpoch += 1; sealingAnimation.restart() }

    Text { x: 18; y: 16; width: 554; text: panel.revealedTitle; color: Appearance.tone("#F0F0F0"); font.family: harmonyBold.name; font.pixelSize: 18; font.weight: Font.Bold; renderType: Text.CurveRendering }
    Text { x: 18; y: 55; width: 554; text: panel.revealedBody; color: Appearance.tone("#DDDDDD"); wrapMode: Text.Wrap; font.family: harmonyRegular.name; font.pixelSize: 13; lineHeight: 1.45; renderType: Text.CurveRendering }

    Item {
        id: bars; anchors.fill: parent
        property var widths: [panel.document.titleWidth || 10].concat(panel.document.redactionWidths || [])
        Repeater {
            model: bars.widths
            delegate: Item {
                id: redaction
                required property int index; required property var modelData
                property real wipeScale: 1
                property int observedOpenEpoch: panel.openEpoch
                property int observedSealEpoch: panel.sealEpoch
                x: 15; y: index === 0 ? 15 : 55 + (index - 1) * 27
                width: Math.min(554, Math.max(34, modelData * (index === 0 ? 10.2 : 7.1) + 6)); height: index === 0 ? 25.4 : 20.85
                clip: true
                transform: Scale { origin.x: redaction.width; origin.y: redaction.height / 2; xScale: redaction.wipeScale; yScale: 1 }
                Rectangle { anchors.fill: parent; color: Appearance.tone("#0C0C0C") }
                Repeater { model: Math.ceil(parent.width / 9) + 4; Rectangle { required property int index; x: index * 9 - 25; y: -20; width: 3; height: 65; color: Appearance.tone("#262626"); rotation: 45 } }
                onObservedOpenEpochChanged: openWipe.restart()
                onObservedSealEpochChanged: sealWipe.restart()
                SequentialAnimation {
                    id: openWipe
                    PauseAnimation { duration: panel.reduceMotion ? 0 : index * 70 }
                    NumberAnimation { target: redaction; property: "wipeScale"; to: 0.001; duration: panel.reduceMotion ? 0 : 450; easing.type: Easing.OutCubic }
                }
                SequentialAnimation {
                    id: sealWipe
                    PropertyAction { target: redaction; property: "wipeScale"; value: 0.001 }
                    PauseAnimation { duration: panel.reduceMotion ? 0 : index * 50 }
                    NumberAnimation { target: redaction; property: "wipeScale"; to: 1; duration: panel.reduceMotion ? 0 : 400; easing.type: Easing.InOutCubic }
                }
            }
        }
    }
    Item {
        id: stamp; x: 452; y: 22; width: 110; height: 31; rotation: -8; opacity: 0.9
        Rectangle { anchors.fill: parent; color: "transparent"; border.width: 3; border.color: "#D63235" }
        Text { anchors.centerIn: parent; text: qsTr("机密"); color: "#D63235"; font.family: harmonyBold.name; font.pixelSize: 15; font.weight: Font.Black; font.letterSpacing: 3.6; renderType: Text.CurveRendering }
    }
    Rectangle {
        id: passwordBar; x: 18 + panel.shakeOffset; y: 361; width: 554; height: 52; color: "#F51A1A1A"; border.color: Appearance.tone("#3A3A3A")
        Rectangle { width: 3; height: 52; color: "#D63235" }
        Text { x: 14; y: 15; text: "◆"; color: "#D63235"; font.pixelSize: 14 }
        Item {
            visible: panel.document.passwordStatus === "protected"; anchors.fill: parent
            TextInput {
                id: password; objectName: "archiveUnlockPassword"; x: 44; y: 10; width: 410; height: 32
                color: Appearance.tone("#EEEEEE"); selectionColor: "#66FFFA00"; selectedTextColor: Appearance.tone("#EEEEEE"); echoMode: TextInput.Password
                verticalAlignment: TextInput.AlignVCenter; font.family: harmonyRegular.name; font.pixelSize: 12
                Rectangle { anchors.fill: parent; z: -1; color: Appearance.tone("#202020"); border.color: password.activeFocus ? Appearance.accent("#FFFA00") : Appearance.tone("#505050") }
                Text { x: 12; anchors.verticalCenter: parent.verticalCenter; visible: !password.text.length && !password.activeFocus; text: qsTr("密码"); color: Appearance.tone("#777777"); font.family: harmonyRegular.name; font.pixelSize: 12 }
                onAccepted: { panel.unlock(text); text = "" }
            }
            CalendarButton { x: 466; y: 10; width: 78; height: 32; title: qsTr("解锁"); enabled: !archiveController.state.busy; reduceMotion: panel.reduceMotion; onClicked: { panel.unlock(password.text); password.text = "" } }
        }
        Item {
            visible: panel.document.passwordStatus !== "protected"; anchors.fill: parent
            Text {
                x: 44; y: 10; width: 345; height: 32; verticalAlignment: Text.AlignVCenter; wrapMode: Text.Wrap
                text: panel.document.passwordStatus === "checking" ? qsTr("正在检查密码记录…") : panel.document.passwordStatus === "missing" ? qsTr("密码记录已经丢失，需要验证 Linux 用户。") : qsTr("系统密钥服务暂时无法读取，需要恢复访问。")
                color: Appearance.tone("#DDDDDD"); font.family: harmonyRegular.name; font.pointSize: 11.5 * 0.75; renderType: Text.CurveRendering
            }
            CalendarButton { x: 425; y: 10; width: 119; height: 32; title: qsTr("恢复访问"); enabled: panel.document.passwordStatus !== "checking" && !archiveController.state.busy; reduceMotion: panel.reduceMotion; onClicked: panel.recover() }
        }
    }
    SequentialAnimation {
        id: shakeAnimation
        NumberAnimation { target: panel; property: "shakeOffset"; to: -6; duration: 70 }
        NumberAnimation { target: panel; property: "shakeOffset"; to: 6; duration: 70 }
        NumberAnimation { target: panel; property: "shakeOffset"; to: -6; duration: 70 }
        NumberAnimation { target: panel; property: "shakeOffset"; to: 6; duration: 70 }
        NumberAnimation { target: panel; property: "shakeOffset"; to: 0; duration: 70 }
    }
    ParallelAnimation {
        id: openingAnimation
        NumberAnimation { target: stamp; property: "opacity"; to: 0; duration: panel.reduceMotion ? 0 : 300 }
        NumberAnimation { target: stamp; property: "scale"; to: 1.3; duration: panel.reduceMotion ? 0 : 400 }
        NumberAnimation { target: passwordBar; property: "opacity"; to: 0; duration: panel.reduceMotion ? 0 : 300 }
        NumberAnimation { target: passwordBar; property: "y"; to: 371; duration: panel.reduceMotion ? 0 : 300 }
    }
    ParallelAnimation {
        id: sealingAnimation
        NumberAnimation { target: stamp; property: "opacity"; from: 0; to: 0.9; duration: panel.reduceMotion ? 0 : 300 }
    }
}
