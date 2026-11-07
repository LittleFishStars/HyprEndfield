import QtQuick
import "ModuleNumbering.js" as ModuleNumbering
import QtQuick.Dialogs
import HyprEndfield.Profile 1.0
import "Appearance.js" as Appearance

// EndfieldHUD PersonalProfileCanvas: original 590 × 422 card and inline editors.
Item {
    id: page
    width: 590; height: 510
    property bool presented: visible
    readonly property bool gameMode: typeof accountController !== "undefined" && accountController.gameCard
    readonly property var controller: gameMode ? gameProfileController : profileController
    readonly property var profile: controller.state
    signal accountRequested()
    function switchMode(key) {
        forceActiveFocus(); popup = ""; textHidden = false;
        accountController.gameCard = key === "game";
    }
    readonly property color accent: profile.themeColor || Appearance.accent("#FFFA00")
    property string popup: ""
    property bool textHidden: false
    property string imageKind: "avatar"
    property string lastTheme: "#C9A2FF"
    function chooseImage(kind) { imageKind = kind; imageDialog.open() }
    onPresentedChanged: if (!presented) { forceActiveFocus(); popup = ""; }
    SettingsHeader { title: qsTr("个人名片"); english: "PERSONAL ID"; code: ModuleNumbering.code("profile"); count: "#" + page.profile.tag }
    VolumeDropdown {
        objectName: "profileMode"; x: 412; y: 22; width: 178; overlayHost: page
        visible: typeof accountController !== "undefined"
        options: [{key:"custom",label:qsTr("自定义名片")},{key:"game",label:qsTr("游戏名片")}]
        selectedKey: page.gameMode ? "game" : "custom"
        text: page.gameMode ? qsTr("游戏名片") : qsTr("自定义名片")
        onSelected: key => page.switchMode(key)
    }
    CalendarButton { x: 302; y: 22; width: 100; title: qsTr("账户绑定"); kind: "secondary"; visible: typeof accountController !== "undefined"; onClicked: { page.forceActiveFocus(); page.accountRequested(); } }
    Item {
        id: card; y: 81; width: 590; height: 422
        ProfileArtwork { x: -180; y: -207; width: 950; height: 860; kind: "background"; controller: page.controller; visible: !Appearance.recording() }
        Canvas {
            anchors.fill: parent; visible: !page.textHidden
            property bool hasBackground: !!page.profile.background
            onHasBackgroundChanged: requestPaint()
            onPaint: {
                const c = getContext("2d"); c.clearRect(0,0,width,height); c.beginPath(); c.moveTo(0,0); c.lineTo(width-18,0); c.lineTo(width,18); c.lineTo(width,height); c.lineTo(0,height); c.closePath(); c.fillStyle = "rgba(12,12,12,.38)"; c.fill()
                if(hasBackground) { const g=c.createLinearGradient(0,0,width,0);g.addColorStop(0,"rgba(0,0,0,.74)");g.addColorStop(.52,"rgba(0,0,0,.52)");g.addColorStop(1,"rgba(0,0,0,.08)");c.fillStyle=g;c.fill() }
                c.strokeStyle = "#2E2E2E"; c.lineWidth = 1;c.beginPath();c.moveTo(width-18,.5);c.lineTo(.5,.5);c.lineTo(.5,height-.5);c.lineTo(width-.5,height-.5);c.lineTo(width-.5,18);c.stroke()
            }
        }
        Item {
            anchors.fill: parent; visible: !page.textHidden
            MouseArea { anchors.fill: parent; onClicked: { page.forceActiveFocus(); page.popup = ""; } }
            Text { x: 22; y: 16; text: qsTr("个人名片") + "   PERSONAL PROFILE"; color: "#BBBBBB"; font.family: harmonyBold.name; font.pixelSize: 12; renderType: Text.CurveRendering }
            Rectangle { x: 30; y: 52; width: 82; height: 82; color: "#D9383838"; clip: true
                ProfileArtwork { anchors.fill: parent; controller: page.controller; visible: !Appearance.recording() }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.chooseImage("avatar") }
            }
            ProfileArtwork { x: 12.5; y: 34.2; width: 117.6; height: 117.6; controller: page.controller; kind: "frame"; accent: page.accent }
            ProfileField { controller: page.controller; id: name; x: 132; y: 50; width: Math.min(238, Math.max(80, nameMetrics.advanceWidth + 8)); height: 36; fieldName: "name"; pixelSize: 30; textLimit: 20; accent: page.accent; visible: !Appearance.recording() }
            TextMetrics { id: nameMetrics; text: page.profile.name; font.family: harmonyBold.name; font.pixelSize: 30 }
            Text { x: name.x + name.width; y: 55; text: "#"; color: "#AAAAAA"; font.pixelSize: 23; font.family: harmonyRegular.name; renderType: Text.CurveRendering }
            ProfileField { controller: page.controller; id: tag; x: name.x + name.width + 14; y: 50; width: 100; height: 36; fieldName: "tag"; pixelSize: 23; textLimit: 10; ink: "#AAAAAA"; accent: page.accent }
            Redaction { x: 138; y: 54; width: 150; height: 26; visible: Appearance.recording() }
            Canvas { x: 138; y: 84; width: 64; height: 16; onPaint: { const c=getContext("2d"); c.strokeStyle="rgba(245,245,245,.45)"; c.lineWidth=1; c.beginPath(); c.moveTo(1,1); c.lineTo(1,10); c.lineTo(9,10); c.moveTo(48,1); c.lineTo(57,1); c.lineTo(57,10); c.moveTo(14,1); c.lineTo(25,10); c.moveTo(25,1); c.lineTo(14,10); c.stroke(); c.fillStyle="rgba(245,245,245,.75)"; c.fillRect(0,12,26,3.5); } }
            Rectangle { id: dateKind; x: 138; y: 98; width: dateLabel.implicitWidth + 32; height: 20; color: "#BA000000"
                Text { x: 6; y: 3; text: "»"; color: page.accent; font.pixelSize: 15 }
                Text { id: dateLabel; x: 24; y: 5; text: page.profile.showsBirthday ? qsTr("生日") : qsTr("苏醒日"); color: "#F5F5F5"; font.family: harmonyBold.name; font.pixelSize: 10; renderType: Text.CurveRendering }
                MouseArea { anchors.fill: parent; onClicked: page.controller.setField("showsBirthday", !page.profile.showsBirthday) }
            }
            Rectangle { x: dateKind.x + dateKind.width; y: 98; width: 108; height: 20; color: "#EBFFFFFF"
                ProfileField { controller: page.controller; anchors.fill: parent; fieldName: page.profile.showsBirthday ? "birthday" : "awakeningDate"; pixelSize: 12; textLimit: page.profile.showsBirthday ? 5 : 10; ink: "#171717"; accent: page.accent }
            }
            Text { x: 138; y: 126; text: "UID"; color: "#CCCCCC"; font.family: harmonyRegular.name; font.pixelSize: 13; renderType: Text.CurveRendering }
            ProfileField { controller: page.controller; x: 164; y: 122; width: 200; height: 21; fieldName: "playerID"; accent: page.accent; visible: !Appearance.recording() }
            Redaction { x: 168; y: 126; width: 120; height: 13; visible: Appearance.recording() }
            Rectangle { x: 22; y: 143; width: 28; height: 28; radius: 14; color: "#66000000"; border.color: "#777777"
                Text { anchors.centerIn: parent; text: "···"; color: "white"; font.pixelSize: 17 }
                MouseArea { anchors.fill: parent; onClicked: page.popup = page.popup === "more" ? "" : "more" }
            }
            Repeater { model: [166, 262, 334]; Rectangle { required property int modelData; x: 8; y: modelData; width: 8; height: 3; color: page.accent } }
            Repeater { model: [qsTr("权限等级"), qsTr("探索等级")]
                Rectangle { required property int index; required property string modelData; x: 22; y: 180 + index * 40; width: 300; height: 34; color: "#8F000000"
                    SourceGlyph { x: 8; y: 5; width: 26; height: 24; kind: index === 0 ? "authority" : "exploration"; ink: "white" }
                    Text { x: 44; y: 10; text: modelData; color: "#EEEEEE"; font.family: harmonyRegular.name; font.pixelSize: 14; renderType: Text.CurveRendering }
                    ProfileField { controller: page.controller; x: 240; y: 2.5; width: 48; height: 29; fieldName: index === 0 ? "permissionLevel" : "explorationLevel"; numeric: true; pixelSize: 25; textLimit: 2; accent: page.accent }
                }
            }
            Repeater { model: [{field:"operatorsCount",title:qsTr("干员")},{field:"weaponsCount",title:qsTr("武器")},{field:"archivesCount",title:qsTr("档案") }]
                Rectangle { required property int index; required property var modelData; x: 22 + index * 102; y: 268; width: 96; height: 58; color: "#52030303"
                    ProfileField { controller: page.controller; x: 5; y: 6; width: 86; height: 29; fieldName: modelData.field; pixelSize: 27; textLimit: 5; ink: page.accent; accent: page.accent }
                    Rectangle { x: 9; y: 41; width: 3; height: 3; color: page.accent }
                    Text { x: 17; y: 37; text: modelData.title; color: page.accent; font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering }
                }
            }
            Text { x: 22; y: 336; text: qsTr("地区建设概况"); color: page.accent; font.family: harmonyRegular.name; font.pixelSize: Math.round(11.5); renderType: Text.CurveRendering }
            Rectangle { x: 22; y: 352; width: 300; height: 28; color: "#8F000000"
                Text { x: 10; anchors.verticalCenter: parent.verticalCenter; text: qsTr("专注模式"); color: "#BBBBBB"; font.family: harmonyRegular.name; font.pixelSize: 12; renderType: Text.CurveRendering }
                Text { x: 130; width: 158; anchors.verticalCenter: parent.verticalCenter; horizontalAlignment: Text.AlignRight; text: Number((typeof focusModeController === "undefined" ? 0 : focusModeController.state.trackedWorkSeconds) / 3600).toFixed(1) + " " + qsTr("小时"); color: "white"; font.family: harmonyBold.name; font.pixelSize: 14; renderType: Text.CurveRendering }
            }
            Canvas { anchors.fill: parent; property color accent: page.accent; onAccentChanged: requestPaint()
                onPaint: { const c=getContext("2d"); c.clearRect(0,0,width,height); const points=[[1,0],[5.5,0],[5.5,6],[3.2,10],[1.6,10],[2.9,6],[1,6]]; for(let q=0;q<2;q++){const x=q?539.1:376,y=q?305:166; c.fillStyle="#F0F0F0"; for(const shift of [0,7]){c.beginPath(); points.forEach((p,i)=>{const px=x+(q?p[0]+shift:13.5-p[0]-shift)*1.4,py=y+(q?p[1]:10-p[1])*1.4; if(i)c.lineTo(px,py);else c.moveTo(px,py)});c.closePath();c.fill()} c.fillStyle=accent;c.fillRect(x+(q?7.5:0)*1.4,y+18.2,8.4,2.1)} c.strokeStyle="#F5F5F5";c.lineWidth=1.3;c.beginPath();c.moveTo(544,182);c.lineTo(556,170);c.lineTo(560,174);c.lineTo(548,186);c.lineTo(543,187);c.closePath();c.moveTo(541,190);c.lineTo(561,190);c.stroke(); }
            }
            ProfileField { controller: page.controller; x: 376; y: 196; width: 182; height: 112; fieldName: "introduction"; multiline: true; textLimit: 150; pixelSize: 13; placeholder: "…"; accent: page.accent; visible: !Appearance.recording() }
        }
        CalendarButton { x: 394; y: 375; width: 122; height: 30; title: qsTr("更换名片主题"); kind: "secondary"; onClicked: page.popup = page.popup === "theme" ? "" : "theme" }
        Rectangle { x: 526.7; y: 381; width: 1.3; height: 18; color: "#BDF5F5F5" }
        Rectangle { x: 538; y: 374; width: 32; height: 32; radius: 16; color: "#77000000"; border.color: "#777777"
            SourceGlyph { anchors.centerIn: parent; width: 18; height: 18; kind: page.textHidden ? "eye" : "eyeoff"; ink: "white" }
            MouseArea { anchors.fill: parent; onClicked: { page.forceActiveFocus(); page.textHidden = !page.textHidden; page.popup = ""; } }
        }
    }
    Rectangle { x: 22; y: 258; width: 220; height: 160; visible: page.popup === "more"; color: "#202020"; border.color: "#555555"; z: 20
        Column { anchors.fill: parent; anchors.margins: 5
            Repeater { model: [qsTr("修改名称"),qsTr("修改 #"),qsTr("更换头像"),qsTr("调整头像"),qsTr("恢复默认头像")]
                Rectangle { required property int index; required property string modelData; width: 210; height: 30; color: moreMouse.containsMouse ? "#404040" : "transparent"
                    Text { x: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData; color: "white"; font.family: harmonyRegular.name; font.pixelSize: 12; renderType: Text.CurveRendering }
                    MouseArea { id: moreMouse; anchors.fill: parent; hoverEnabled: true; onClicked: { page.popup = ""; if(index===0)name.beginEditing();else if(index===1)tag.beginEditing();else if(index===2)page.chooseImage("avatar");else if(index===3)page.popup="portrait";else page.controller.resetImage("avatar"); } }
                }
            }
        }
    }
    Rectangle {
        id: popover; x: page.popup === "portrait" ? 22 : 220; y: page.popup === "portrait" ? 260 : 86
        width: page.popup === "portrait" ? 330 : 350; height: page.popup === "portrait" ? 144 : 365
        visible: page.popup === "portrait" || page.popup === "theme"; z: 21; color: "#202020"; border.color: "#555555"
        MouseArea { anchors.fill: parent; onClicked: page.forceActiveFocus() }
        Text { x: 12; y: 10; text: page.popup === "portrait" ? qsTr("调整头像") + "   PORTRAIT" : qsTr("名片主题") + "   CARD THEME"; color: "#DDDDDD"; font.family: harmonyBold.name; font.pixelSize: 12; renderType: Text.CurveRendering }
        CalendarButton { x: parent.width-30; y: 4; width: 24; kind: "icon"; glyph: "×"; onClicked: page.popup = "" }
        Item { x: 12; y: 36; visible: page.popup === "theme"; width: parent.width - 24; height: 68
            Row { spacing: 6; Repeater { model: ["#FFFA00","#22A6F2","#E5383B","#8CE03C","#C9A2FF", ""]
                Rectangle { required property string modelData; width: 26; height: 26; color: modelData || page.accent; border.color: "#888888"; border.width: page.profile.themeColor === modelData ? 2 : 1
                    Text { anchors.centerIn: parent; text: modelData ? "" : "+"; font.pixelSize: 18; color: "white" }
                    MouseArea { anchors.fill: parent; onClicked: { if(modelData) { page.lastTheme=modelData;page.controller.setField("themeColor",modelData) } else themeColor.open(); } }
                }
            } }
            CalendarButton { x: parent.width - 120; width: 120; title: qsTr("跟随浮层"); kind: page.profile.themeColor ? "secondary" : "primary"; onClicked: page.controller.setField("themeColor", page.profile.themeColor ? "" : page.lastTheme) }
            CalendarButton { y: 36; width: 160; title: qsTr("背景"); kind: "secondary"; onClicked: page.chooseImage("background") }
            CalendarButton { x: 166; y: 36; width: 160; title: qsTr("默认"); kind: "secondary"; onClicked: page.controller.resetImage("background") }
        }
        Repeater {
            model: page.popup === "portrait" ? [{key:"avatarZoom",label:qsTr("缩放"),min:1,max:5}, {key:"avatarOffsetX",label:qsTr("水平位置"),min:-1,max:1}, {key:"avatarOffsetY",label:qsTr("垂直位置"),min:-1,max:1}]
                : [{key:"backgroundWidth",label:qsTr("背景宽度"),min:400,max:900}, {key:"backgroundZoom",label:qsTr("背景缩放"),min:1,max:20}, {key:"backgroundOffsetX",label:qsTr("背景水平位置"),min:-400,max:400}, {key:"backgroundOffsetY",label:qsTr("背景垂直位置"),min:-250,max:250}, {key:"thumbnailZoom",label:qsTr("缩略图缩放"),min:1,max:5}, {key:"thumbnailOffsetX",label:qsTr("缩略图水平位置"),min:-1,max:1}, {key:"thumbnailOffsetY",label:qsTr("缩略图垂直位置"),min:-1,max:1}]
            Item { required property int index; required property var modelData; x: 12; y: (page.popup === "portrait" ? 36 : 104) + index * 34 + (page.popup === "theme" && index>=4 ? 17 : 0); width: popover.width-24; height: 34
                Rectangle { y: -9; width: parent.width; height: 1; color: "#555555"; visible: page.popup === "theme" && index === 4 }
                Text { y: 11; width: 118; text: modelData.label; elide: Text.ElideRight; color: "#CCCCCC"; font.family: harmonyRegular.name; font.pixelSize: 12; renderType: Text.CurveRendering }
                HudSlider { x: 126; y: 2; width: parent.width-126; minimum: modelData.min; maximum: modelData.max; stepSize: modelData.max <= 20 ? .01 : 1; valueFormat: modelData.key.endsWith("Zoom") ? "scale" : "decimal"; value: page.profile[modelData.key]; onValueModified: (value, final) => page.controller.setField(modelData.key, value, final) }
            }
        }
    }
    Text { y: 505; width: 590; text: page.profile.error; color: "#E79D87"; font.family: harmonyRegular.name; font.pixelSize: 12; wrapMode: Text.Wrap; renderType: Text.CurveRendering }
    FileDialog { id: imageDialog; title: qsTr("选择图片"); nameFilters: [qsTr("图片 (*.png *.jpg *.jpeg *.webp *.bmp *.gif)"),qsTr("所有文件 (*)")]; onAccepted: page.controller.importImage(page.imageKind, selectedFile) }
    ColorDialog { id: themeColor; title: qsTr("自定名片颜色"); selectedColor: page.accent; onAccepted: page.controller.setField("themeColor", String(selectedColor).substring(0,7)) }
}
