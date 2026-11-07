import QtQuick
import QtQuick.Shapes
import "Appearance.js" as Appearance

// EndfieldHUD AccountCanvas: MOD-16, 590 × 510, original plate geometry.
Item {
    id: page
    width: 590; height: 510
    property bool presented: visible
    property bool reduceMotion: false
    property var controller: accountController
    readonly property var account: controller.state
    property bool confirming: false
    property string notice: ""
    property string previousStatus: ""
    onPresentedChanged: if (!presented) { confirming = false; role.opened = false; headerMode.opened = false; notice = ""; }
    Connections {
        target: page.controller
        function onChanged() {
            if (page.account.busy) { role.opened = false; headerMode.opened = false; }
            if (page.account.status !== page.previousStatus) {
                page.previousStatus = page.account.status;
                if (page.presented && page.account.status) { page.notice = page.account.status; noticeTimer.restart(); }
            }
        }
    }
    SettingsHeader { title: qsTr("账户绑定"); english: "ACCOUNT"; code: "MOD-16"; count: page.account.linked ? "LINKED" : "OFFLINE" }
    CalendarButton { x: 386; y: 22; width: 96; visible: page.account.linked && page.account.reconnect; enabled: !page.account.busy; title: qsTr("重新连接"); onClicked: page.controller.action("connect") }
    CalendarButton { x: 490; y: 22; width: 100; visible: page.account.linked; enabled: !page.account.busy; title: qsTr("解除绑定"); kind: "secondary"; onClicked: page.confirming = true }
    Row {
        y: 81; spacing: 4
        Repeater {
            model: [{key: "china", title: qsTr("国服")}, {key: "global", title: qsTr("国际服")}]
            Rectangle {
                required property var modelData
                readonly property bool selected: page.account.region === modelData.key
                width: 293; height: 30; color: selected ? "#D6D7D2" : Appearance.tone("#202020")
                border.color: selected ? "transparent" : Appearance.tone("#3A3A3A")
                opacity: page.account.busy ? .35 : 1
                Rectangle { width: 3; height: 20; y: 5; visible: parent.selected; color: "#111111" }
                Text { anchors.centerIn: parent; text: modelData.title; color: parent.selected ? "#111111" : Appearance.tone("#D9D9D9"); font.family: harmonyMedium.name; font.pointSize: 12.5 * .75; renderType: Text.CurveRendering }
                MouseArea { anchors.fill: parent; enabled: !page.account.busy; cursorShape: Qt.PointingHandCursor; onClicked: { page.confirming = false; page.controller.action("region", parent.modelData.key); } }
            }
        }
    }
    Rectangle {
        y: 121; width: 590; height: 250; visible: !page.account.linked
        color: Appearance.tone("#121212"); border.color: Appearance.tone("#3A3A3A")
        Item { anchors.fill: parent; clip: true; opacity: .09
            Repeater { model: 75; Rectangle { required property int index; x: index * 12 - 250; y: -125; width: 1; height: 600; rotation: 45; color: "white" } }
        }
        Text { y: 76.25; width: 590; height: 20; text: "[ " + qsTr("未绑定") + " ]"; horizontalAlignment: Text.AlignHCenter; color: Appearance.accent("#FFFA00"); font.family: harmonyBold.name; font.pixelSize: 20; renderType: Text.CurveRendering }
        Text { y: 106.25; width: 590; height: 12; text: qsTr("鹰角网络通行证"); horizontalAlignment: Text.AlignHCenter; color: Appearance.tone("#777777"); font.family: harmonyRegular.name; font.pointSize: 11.5 * .75; renderType: Text.CurveRendering }
        TextMetrics { id:connectMetrics; text:qsTr("绑定账户"); font.family:harmonyBold.name; font.pixelSize:13; font.bold:true }
        CalendarButton { objectName: "accountConnect"; x: (590-width)/2; y: 133.75; width: Math.ceil(67+connectMetrics.width); height: 40; title: qsTr("绑定账户"); textSize:13; sourceIcon:"link2"; enabled: !page.account.busy; onClicked: page.controller.action("connect") }
    }
    Item {
        y: 121; width: 590; height: 320; visible: page.account.linked
        Repeater {
            model: [{x:0,w:322},{x:332,w:258}]
            Rectangle {
                required property var modelData
                x: modelData.x; width: modelData.w; height: 99.4
                gradient: Gradient { GradientStop { position:0; color: Appearance.tone("#232323") } GradientStop { position:1; color: Appearance.tone("#1B1B1B") } }
                border.color: Appearance.tone("#343434")
                Rectangle { width:8; height:2; color: Appearance.tone("#6A6A6A") }
                Rectangle { width:2; height:8; color: Appearance.tone("#6A6A6A") }
            }
        }
        Text { x:12; y:9; text:"SANITY"; color: Appearance.tone("#888888"); font.family: harmonyMedium.name; font.pixelSize:10; renderType: Text.CurveRendering }
        Text { id: current; x:12; y:23; height:50.4; text: page.account.current; color: Appearance.tone("#EEEEEE"); font.family: harmonyBold.name; font.pixelSize:56; verticalAlignment:Text.AlignVCenter; renderType: Text.CurveRendering }
        Text { x: current.x + current.implicitWidth + 10; y:55.4; text:"/ " + page.account.maximum; color: Appearance.tone("#888888"); font.family:harmonyMedium.name; font.pixelSize:10; renderType:Text.CurveRendering }
        Repeater { model:24; Rectangle { required property int index; x:12 + index * (298 + 3) / 24; y:81.4; width:(298 - 23 * 3) / 24; height:8
            color:Appearance.tone("#2A2A2A"); border.color:index < page.account.filled ? "transparent" : Appearance.tone("#3A3A3A")
            gradient:index < page.account.filled ? capacityGradient : null
            Gradient { id:capacityGradient; GradientStop { position:0; color:Appearance.accent("#FFFC6B") } GradientStop { position:1; color:Appearance.accent("#D9D300") } }
        } }
        MouseArea { width:322; height:99.4; cursorShape:Qt.PointingHandCursor; onClicked: { page.notice = qsTr("下次回复 %1").arg(page.account.next) + "    " + qsTr("全部回复 %1").arg(page.account.full); noticeTimer.restart(); } }
        Repeater {
            model: [{title:"NEXT POINT",value:page.account.next}, {title:"ALL RECOVERED",value:page.account.full}]
            Item { required property int index; required property var modelData; x:344; y:9 + index * 46; width:234; height:34
                Text { text:parent.modelData.title; color: Appearance.tone("#888888"); font.family:harmonyMedium.name; font.pixelSize:10; renderType:Text.CurveRendering }
                Text { y:16; text:parent.modelData.value; color: Appearance.tone("#EEEEEE"); font.family:harmonyBold.name; font.pixelSize:18; renderType:Text.CurveRendering }
            }
        }
        VolumeRow {
            y:107.4; title:qsTr("游戏角色"); reduceMotion:page.reduceMotion
            VolumeDropdown { id:role; objectName:"accountRole"; x:338; y:7; width:240; overlayHost:page; options:page.account.roles; selectedKey:page.account.selectedRole; text:options.find(r => r.key === selectedKey)?.title || qsTr("选择账户"); enabled: options.length > 0 && !page.account.busy; reduceMotion:page.reduceMotion; onSelected:key => page.controller.action("role", key) }
        }
        VolumeRow {
            y:157.4; title:qsTr("顶部显示"); reduceMotion:page.reduceMotion
            VolumeDropdown { id:headerMode; x:400; y:7; overlayHost:page; options:page.account.headers; selectedKey:page.account.header; text:options.find(r => r.key === selectedKey)?.label || ""; reduceMotion:page.reduceMotion; onSelected:key => page.controller.action("header", key) }
        }
        VolumeRow {
            id:syncRow; y:207.4; height:page.account.lastSync ? 50 : 44; title:qsTr("同步个人名片"); subtitle:page.account.lastSync; reduceMotion:page.reduceMotion
            CalendarButton { x:452; y:(parent.height-30)/2; width:30; kind:"icon"; glyph:"↻"; enabled:!page.account.busy && !page.account.reconnect; reduceMotion:page.reduceMotion; onClicked:page.controller.action("refresh") }
            Rectangle {
                x:490; y:(parent.height-26)/2; width:88; height:26; color:Appearance.tone("#202020"); border.color:Appearance.tone("#393939")
                Rectangle { x:page.account.sync ? 44 : 2; y:2; width:42; height:22; color:page.account.sync ? Appearance.accent("#FFFA00") : "#8E8F8A" }
                Repeater { model:2; Text { required property int index; x:index*44; width:44; height:26; text:index ? "ON" : "OFF"; color:page.account.sync === !!index ? "#111111" : "#777777"; horizontalAlignment:Text.AlignHCenter; verticalAlignment:Text.AlignVCenter; font.family:harmonyBold.name; font.pixelSize:10; renderType:Text.CurveRendering } }
                MouseArea { anchors.fill:parent; cursorShape:Qt.PointingHandCursor; onClicked:page.controller.action("sync", !page.account.sync) }
            }
        }
        Flow {
            y:syncRow.y + syncRow.height + 8; width:590; spacing:6
            Repeater {
                model:page.account.fields
                Rectangle {
                    required property var modelData
                    width:label.implicitWidth + (modelData.enabled ? 29 : 14); height:24
                    color:modelData.enabled ? Appearance.accent("#FFFA00") : "transparent"; border.color:modelData.enabled ? "transparent" : Appearance.tone("#5A5A5A")
                    Text { x:7; y:5; visible:parent.modelData.enabled; text:"✓"; font.pixelSize:10; color:"#111111"; renderType:Text.CurveRendering }
                    Text { id:label; x:parent.modelData.enabled ? 22 : 7; y:7.25; text:parent.modelData.title; color:parent.modelData.enabled ? "#111111" : Appearance.tone("#CFCFCF"); font.family:harmonyBold.name; font.pointSize:9.5 * .75; renderType:Text.CurveRendering }
                    MouseArea { anchors.fill:parent; cursorShape:Qt.PointingHandCursor; onClicked:page.controller.action(parent.modelData.key, !parent.modelData.enabled) }
                }
            }
        }
    }
    // HUDToast's original 600 × 150 geometry. No new page-transition motion.
    Item { x:-5; y:190; width:600; height:150; z:35; visible:!!page.notice && !page.confirming
        Shape { x:-125; y:-310; width:850; height:850; preferredRendererType:Shape.CurveRenderer
            ShapePath { strokeWidth:-1
                fillGradient:RadialGradient { centerX:425; centerY:425; centerRadius:425; focalX:425; focalY:425
                    GradientStop { position:0; color:Appearance.tone("#9E000000") } GradientStop { position:.821; color:Appearance.tone("#9E000000") } GradientStop { position:.99; color:"transparent" }
                }
                PathSvg { path:"M0 0H850V850H0Z" }
            }
        }
        Rectangle { y:44; width:600; height:1; gradient:Gradient { orientation:Gradient.Horizontal; GradientStop { position:0; color:"#00FFFFFF" } GradientStop { position:.3; color:"#A6FFFFFF" } GradientStop { position:.7; color:"#A6FFFFFF" } GradientStop { position:1; color:"#00FFFFFF" } } }
        Repeater { model:[120,480]; Rectangle { required property int modelData; x:modelData; y:41; width:1; height:7; color:"white" } }
        Rectangle { x:270; y:14; width:60; height:60; color:Appearance.tone("#161616"); border.color:Appearance.tone("#4A4A4A")
            SourceGlyph { x:17; y:17; width:26; height:26; kind:"warn"; ink:"white" }
            Shape { x:48; width:12; height:12; preferredRendererType:Shape.CurveRenderer; ShapePath { strokeWidth:-1; fillColor:Appearance.accent("#FFFA00"); PathSvg { path:"M0 0H12V12Z" } } }
        }
        Text { y:88; width:600; height:14; text:qsTr("提示"); horizontalAlignment:Text.AlignHCenter; color:"white"; font.family:harmonyBold.name; font.pixelSize:14; renderType:Text.CurveRendering }
        Text { y:110; width:600; text:page.notice; horizontalAlignment:Text.AlignHCenter; wrapMode:Text.Wrap; color:Appearance.tone("#AAAAAA"); font.family:harmonyRegular.name; font.pointSize:11.5*.75; renderType:Text.CurveRendering }
        Rectangle { x:289; y:130; width:22; height:2; color:Appearance.accent("#FFFA00") }
    }
    Timer { id:noticeTimer; interval:1500; onTriggered:page.notice = "" }
    ClipboardConfirmBar { x:-15; y:190; z:40; visible:page.confirming; title:qsTr("解除此账户绑定？"); message:page.account.region === "china" ? qsTr("国服") : qsTr("国际服"); confirmTitle:qsTr("解除绑定"); confirmGlyph:"x"; reduceMotion:page.reduceMotion; onCancelled:page.confirming=false; onAccepted:{ page.confirming=false; page.controller.action("disconnect"); } }
}
