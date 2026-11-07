import QtQuick
import HyprEndfield.Account 1.0
import "ProfileCardSource.js" as Source
import "Appearance.js" as Appearance

Item {
    id: badge
    required property var shell
    property var controller: gameProfileController
    readonly property var profile: controller.state
    readonly property color accent: profile.themeColor || Appearance.accent("#FFFA00")
    readonly property bool hovered: pointer.containsMouse
    property real highlight: pointer.pressed ? 0.019607843831181526 : hovered ? 1 : 0
    Behavior on highlight { NumberAnimation { duration: badge.shell.displaySettings && badge.shell.displaySettings.reduceMotion ? 0 : 100; easing.type: Easing.Linear } }
    signal clicked()
    width:1920; height:1080
    function caption(binding) {
        switch (binding) {
        case "managerName": return Appearance.recording() ? "" : profile.name;
        case "managerNumber": return Appearance.recording() ? "UID:" : "UID: " + profile.playerID;
        case "managerLevel": return String(profile.permissionLevel);
        case "managerLevelLabel": return qsTr("权限等级");
        case "progressTxt": return profile.permissionLevel === 60 ? qsTr("满级") : "";
        default: return "";
        }
    }
    Repeater {
        model:Source.data.surfaces
        SourceSurface {
            required property var modelData
            shell:badge.shell; spec:modelData; drawImage:false
            visible:modelData.dynamic !== "disabledGlow"
            opacity:modelData.dynamic === "hover" ? badge.highlight : 1
            GameProfileArtwork {
                anchors.fill:parent; visible:!parent.modelData.fontSize
                spec: {
                    const s = Object.assign({}, parent.modelData);
                    if (s.path.includes("/PlayerInfo/DecoNode/") && ["LeftLineImage", "RightLineImage", "LineImage", "LeftBottomImage"].includes(s.name) && s.color) {
                        s.color = s.color.slice(); s.color[3] += Math.max(0, .62 - s.color[3]) * badge.highlight;
                    }
                    return s;
                }
                controller:badge.controller; accent:badge.accent; recording:Appearance.recording()
            }
            Text {
                anchors.fill:parent; visible:!!parent.modelData.fontSize
                text:badge.caption(parent.modelData.binding)
                color:parent.modelData.binding === "progressTxt" ? badge.accent : "white"
                font.family:harmonyMedium.name; font.pointSize:(parent.modelData.fontSize || 16) * .75; font.weight:Font.Medium
                minimumPixelSize:10; fontSizeMode:Text.HorizontalFit; elide:Text.ElideRight
                horizontalAlignment:["managerLevelLabel","progressTxt"].includes(parent.modelData.binding) ? Text.AlignRight : Text.AlignLeft
                verticalAlignment:Text.AlignVCenter; renderType:Text.CurveRendering
            }
        }
    }
    SourceSurface {
        shell:badge.shell; spec:Source.data.root; drawImage:false
        MouseArea { id:pointer; anchors.fill:parent; hoverEnabled:true; cursorShape:Qt.PointingHandCursor; onClicked:badge.clicked() }
    }
}
