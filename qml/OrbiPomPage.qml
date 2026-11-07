import QtQuick
import QtQuick.Shapes
import HyprEndfield.OrbiPom 1.0
import "Appearance.js" as Appearance

// EndfieldHUD OrbiPomCanvas: 230 × 280 world, 1.2 px/unit, 32.5-unit drop zone.
Item {
    id: page
    width: 590; height: 510
    property bool presented: visible
    property var controller: orbipomController
    readonly property var game: controller.state
    property bool rules: false
    property bool confirmRestart: false
    property point keyboardAim: Qt.point(115, 120)
    readonly property bool lowPower: typeof settingsController !== "undefined" && settingsController.lowPowerVisualMode
    readonly property bool reduceMotion: typeof settingsController !== "undefined" && settingsController.reduceMotion
    readonly property var skills: [{key:"clear",title:qsTr("消除"),cost:1},{key:"wind",title:qsTr("风场"),cost:2},{key:"shake",title:qsTr("震动"),cost:3},{key:"swap",title:qsTr("交换"),cost:0}]
    Binding { target: page.controller; property: "active"; value: page.presented && Qt.application.state === Qt.ApplicationActive }
    Binding { target: page.controller; property: "held"; value: page.rules || page.confirmRestart }
    Binding { target: page.controller; property: "lowPower"; value: page.lowPower }
    Component.onDestruction: controller.active = false
    onPresentedChanged: { if (presented) forceActiveFocus(); else { rules = false; confirmRestart = false; } }
    function restart() { if (game.state === "playing") confirmRestart = true; else controller.start() }
    function aim(x, y) {
        const p = Qt.point(x / 1.2, y / 1.2 - 32.5)
        if (!game.skill) p.y = Math.max(0, Math.min(280, p.y))
        keyboardAim = p; controller.move(p.x, p.y)
    }
    SettingsHeader { title: qsTr("小游戏"); english: "MINIGAME"; code: "MOD-06"; count: "SCORE" }
    Rectangle { x: 560; y: 22; width: 28; height: 28; radius: 14; color: Appearance.tone("#242424"); border.color: Appearance.tone("#444444")
        Text { anchors.centerIn: parent; text: "?"; color: Appearance.tone("#EEEEEE"); font.family: harmonyBold.name; font.pixelSize: 15; renderType: Text.CurveRendering }
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.rules = !page.rules }
    }
    OrbiPomBoard {
        id: board; x: 157; y: 81; width: 276; height: 375; controller: page.controller; accent: Appearance.accent("#FFFA00")
        dangerInk: "#E5383B"
        SequentialAnimation on dangerInk {
            running: page.presented && !page.game.paused && page.game.dangerSeconds !== null && page.game.dangerSeconds !== undefined && !page.reduceMotion
            loops: Animation.Infinite
            ColorAnimation { from: "#E5383B"; to: "#661B1D"; duration: 333; easing.type: Easing.BezierSpline; easing.bezierCurve: [.42,0,.58,1,1,1] }
            ColorAnimation { from: "#661B1D"; to: "#E5383B"; duration: 333; easing.type: Easing.BezierSpline; easing.bezierCurve: [.42,0,.58,1,1,1] }
        }
        Text { x: 190; y: 22; width: 80; horizontalAlignment: Text.AlignRight; visible: page.game.dangerSeconds !== null && page.game.dangerSeconds !== undefined; text: Math.max(0, page.game.dangerSeconds || 0).toFixed(1); color: "#E5383B"; font.family: harmonyBold.name; font.pixelSize: 15; renderType: Text.CurveRendering }
        MouseArea { anchors.fill: parent; hoverEnabled: true; enabled: !page.rules && !page.confirmRestart; cursorShape: Qt.CrossCursor
            onPressed: mouse => { page.forceActiveFocus(); page.aim(mouse.x,mouse.y); }
            onPositionChanged: mouse => page.aim(mouse.x,mouse.y)
            onReleased: mouse => { if (containsMouse) { page.aim(mouse.x,mouse.y); page.controller.release(page.keyboardAim.x,page.keyboardAim.y); } }
        }
        Rectangle { anchors.fill: parent; color: "#99000000"; visible: !!page.game.error || page.game.state === "over" || page.game.manuallyPaused || page.confirmRestart
            Text { anchors.centerIn: parent; text: "<font color='" + Appearance.accent("#FFFA00") + "'>[ </font>" + (page.game.error ? "UNAVAILABLE" : page.game.state === "over" ? "GAME OVER" : "PAUSED") + "<font color='" + Appearance.accent("#FFFA00") + "'> ]</font>"; textFormat: Text.RichText; color: "#EEEEEE"; font.family: harmonyBold.name; font.pixelSize: 22; renderType: Text.CurveRendering }
        }
    }
    component StatPlate: Rectangle {
        property string title: ""
        width: 145; height: 67; border.color: Appearance.tone("#343434")
        gradient: Gradient { GradientStop { position:0; color:Appearance.tone("#232323") } GradientStop { position:1; color:Appearance.tone("#1B1B1B") } }
        Rectangle { width:8; height:2; color:Appearance.tone("#6A6A6A") }
        Rectangle { width:2; height:8; color:Appearance.tone("#6A6A6A") }
        Text { x:12; y:9; text:parent.title; color:Appearance.tone("#8A8A8A");font.family:harmonyRegular.name;font.pixelSize:10;renderType:Text.CurveRendering }
    }
    StatPlate { x:445;y:81;title:"SCORE"
        Text { x:12;y:25;width:121;text:Number(page.game.score).toLocaleString(Qt.locale(),"f",0);color:Appearance.tone("#F2F2F2");font.family:harmonyRegular.name;font.pixelSize:32;minimumPixelSize:22;fontSizeMode:Text.Fit;renderType:Text.CurveRendering }
        Text { x:52;y:9;width:81;horizontalAlignment:Text.AlignRight;text:"BEST "+page.game.best;color:Appearance.tone("#8A8A8A");font.family:harmonyRegular.name;font.pixelSize:10;renderType:Text.CurveRendering }
    }
    StatPlate { x:445;y:154;title:"NEXT"
        Image { x:(145-width)/2;y:22+(38-height)/2;width:Math.min(38,([24,32,40,56,64,72,84,96,128,160,192][Math.max(0,(page.game.nextLevel||1)-1)])*.96);height:width;source:"qrc:/assets/orbipom/level-"+(page.game.nextLevel||1)+".png";fillMode:Image.PreserveAspectFit;mipmap:true }
    }
    Item { x:445;y:227;width:145;height:26
        Text { text:"SP";color:Appearance.tone("#8A8A8A");font.family:harmonyRegular.name;font.pixelSize:10;renderType:Text.CurveRendering }
        Text { x:65;width:80;horizontalAlignment:Text.AlignRight;text:String(page.game.energy).padStart(2,"0")+"/03";color:"#22A6F2";font.family:harmonyRegular.name;font.pixelSize:10;renderType:Text.CurveRendering }
        Row { y:16;spacing:3;Repeater { model:12;Rectangle { required property int index;width:(145-33)/12;height:10;color:index<(page.game.energy>=3?12:Math.floor(page.game.energyProgress))?"#22A6F2":Appearance.tone("#2B2B2B") } } }
    }
    Repeater { model: page.skills
        Item { id:skill;required property var modelData;required property int index;x:445;y:261+index*40;width:145;height:34
            readonly property bool selected:page.game.skill===modelData.key
            enabled:page.game.state==="playing" && !page.game.paused && page.game.skillPhase!=="casting" && (selected || (modelData.key==="swap"?page.game.swapCharge>=6:page.game.energy>=modelData.cost))
            opacity:enabled?1:.4
            transform:[Translate { y:skillMouse.containsMouse&&skill.enabled?-2:0 },Scale {origin.x:72.5;origin.y:17;xScale:skillMouse.pressed?.965:1;yScale:xScale}]
            Shape { anchors.fill:parent;preferredRendererType:Shape.CurveRenderer
                ShapePath { strokeWidth:-1;fillGradient:LinearGradient {x1:0;y1:0;x2:145;y2:34;GradientStop {position:0;color:skill.selected?Appearance.accent("#FFFC6B"):"#ECEDE8"} GradientStop {position:1;color:skill.selected?Appearance.accent("#D9D300"):"#CDCEC9"} } PathSvg {path:"M0 0H137L145 8V34H0Z"} }
                ShapePath {strokeWidth:-1;fillColor:Qt.rgba(1,1,1,skillMouse.containsMouse?.07:0);PathSvg {path:"M0 0H137L145 8V34H0Z"}}
            }
            Image {x:8;y:6;width:22;height:22;source:"qrc:/assets/orbipom/skill-"+skill.modelData.key+".png";fillMode:Image.PreserveAspectFit;mipmap:true}
            Text {x:38;y:10.75;width:67;text:skill.modelData.title;color:"#161616";font.family:harmonyMedium.name;font.pixelSize:13;elide:Text.ElideRight;renderType:Text.CurveRendering}
            Text {x:105;y:12;width:30;horizontalAlignment:Text.AlignRight;text:skill.modelData.key==="swap"?Math.min(6,page.game.swapCharge)+"/6":skill.modelData.cost;color:"#666666";font.family:harmonyBold.name;font.pixelSize:10;renderType:Text.CurveRendering}
            MouseArea {id:skillMouse;anchors.fill:parent;hoverEnabled:true;cursorShape:Qt.PointingHandCursor;onClicked:page.controller.skill(skill.modelData.key)}
        }
    }
    CalendarButton {x:445;y:423;width:107;height:32;title:page.game.manuallyPaused?qsTr("继续"):qsTr("暂停");enabled:page.game.state==="playing"&&!page.confirmRestart;onClicked:page.controller.togglePause()}
    CalendarButton {x:558;y:423;width:32;height:32;kind:"icon";onClicked:page.restart();SourceGlyph {anchors.centerIn:parent;width:16;height:16;kind:"reset";ink:Appearance.tone("#EEEEEE")}}
    Text {y:468;width:590;text:page.game.error;wrapMode:Text.Wrap;color:Appearance.tone("#AAAAAA");font.family:harmonyRegular.name;font.pixelSize:12;renderType:Text.CurveRendering}
    ClipboardConfirmBar { x:-15;y:80;visible:page.confirmRestart;title:qsTr("重新开始游戏？");message:qsTr("当前得分将会丢失。");confirmTitle:qsTr("重新开始");confirmGlyph:"reset";accentColor:Appearance.accent("#FFFA00");onAccepted:{page.confirmRestart=false;page.controller.start()}onCancelled:page.confirmRestart=false }
    Rectangle { id:guide;x:12;y:81;width:566;height:Math.max(leftRules.height,rightRules.height)+54;visible:page.rules;color:Appearance.tone("#222222");border.color:Appearance.tone("#3D3D3D");z:10
        MouseArea {anchors.fill:parent}
        Text {x:12;y:10;text:qsTr("玩法说明").toUpperCase();color:Appearance.tone("#EEEEEE");font.family:harmonyBold.name;font.pixelSize:12;renderType:Text.CurveRendering}
        CalendarButton {x:530;y:5;width:26;height:26;kind:"icon";glyph:"×";onClicked:page.rules=false}
        Rectangle {x:282.5;y:44;width:1;height:guide.height-58;color:Appearance.tone("#333333")}
        component Paragraph: Text {width:261;wrapMode:Text.Wrap;color:Appearance.tone("#D9D9D9");font.family:harmonyRegular.name;font.pixelSize:12;lineHeight:1.45;renderType:Text.CurveRendering}
        component Heading: Text {width:261;color:Appearance.accent("#FFFA00");font.family:harmonyBold.name;font.pixelSize:10;renderType:Text.CurveRendering}
        Column {id:leftRules;x:12;y:42;width:261;spacing:7
            Heading {text:qsTr("目标")}
            Paragraph {text:qsTr("将山团团投入容器。两个相同的山团团接触后，会合成更大一级的山团团。两个最大的山团团（第 11 级）合成后会消失。")}
            Item {width:1;height:7}
            Heading {text:qsTr("操作")}
            Repeater {model:[{name:qsTr("瞄准"),detail:qsTr("在容器上移动指针，或使用方向键")},{name:qsTr("投放"),detail:qsTr("点击，或按 Return 键")},{name:qsTr("技能"),detail:qsTr("点击技能，或按 1–4")},{name:qsTr("取消技能"),detail:qsTr("按 Esc，或再次点击该技能")},{name:qsTr("暂停"),detail:qsTr("空格键或 P")}]
                Item {required property var modelData;width:261;height:Math.max(keyName.implicitHeight,keyValue.implicitHeight)
                    Paragraph {id:keyName;width:76;text:modelData.name;color:Appearance.tone("#8A8A8A")}
                    Paragraph {id:keyValue;x:82;width:179;text:modelData.detail}
                }
            }
            Item {width:1;height:7}
            Heading {text:qsTr("游戏结束")}
            Paragraph {text:qsTr("山团团停在红色虚线上方时，会开始 5 秒倒计时。倒计时结束前让它们回到线下，否则游戏结束。")}
        }
        Column {id:rightRules;x:293;y:42;width:261;spacing:7
            Heading {text:qsTr("计分")}
            Paragraph {text:qsTr("合成的山团团越大，得分越高：1、3、6、10……最高 66 分。新的山团团来自最小的五种；NEXT 显示当前之后的一个。BEST 记录最高分。")}
            Item {width:1;height:7}
            Heading {text:qsTr("能量与技能")}
            Paragraph {text:qsTr("每次合成填充一格能量，12 格积满获得 1 点 SP，最多 3 点。选择技能后，点击山团团或容器即可发动。")}
            Repeater {model:[qsTr("移除选中的山团团。"),qsTr("上升气流充满容器数秒，托起山团团。"),qsTr("左右摇晃容器。"),qsTr("选择两个山团团交换位置。在其他技能上消耗 SP 即可充能。")]
                Item {required property int index;required property string modelData;width:261;height:17+detail.implicitHeight
                    Image {width:22;height:22;source:"qrc:/assets/orbipom/skill-"+page.skills[index].key+".png";fillMode:Image.PreserveAspectFit;mipmap:true}
                    Paragraph {x:30;width:151;text:page.skills[index].title}
                    Text {x:181;y:2;width:80;horizontalAlignment:Text.AlignRight;text:index===3?qsTr("消耗 6 SP"):page.skills[index].cost+" SP";color:Appearance.accent("#FFFA00");font.family:harmonyBold.name;font.pixelSize:10;renderType:Text.CurveRendering}
                    Paragraph {id:detail;x:30;y:17;width:231;text:modelData;font.pixelSize:11;color:Appearance.tone("#A9A9A9");lineHeight:1.4}
                }
            }
        }
    }
    Keys.onPressed:event=>{
        if(event.modifiers&(Qt.ControlModifier|Qt.AltModifier|Qt.MetaModifier))return
        if(event.key===Qt.Key_Escape){if(rules)rules=false;else if(confirmRestart)confirmRestart=false;else controller.cancelSkill();event.accepted=true;return}
        if(rules||confirmRestart)return
        if(event.key===Qt.Key_Space||event.key===Qt.Key_P){if(!event.isAutoRepeat)controller.togglePause();event.accepted=true}
        else if(event.key===Qt.Key_Return||event.key===Qt.Key_Enter){if(game.skill)controller.release(keyboardAim.x,keyboardAim.y);else controller.drop();event.accepted=true}
        else if(event.key>=Qt.Key_1&&event.key<=Qt.Key_4){if(!event.isAutoRepeat)controller.skill(skills[event.key-Qt.Key_1].key);event.accepted=true}
        else if([Qt.Key_Left,Qt.Key_Right,Qt.Key_Up,Qt.Key_Down].includes(event.key)){let x=game.skill?keyboardAim.x:(game.previewX||115),y=keyboardAim.y;if(event.key===Qt.Key_Left)x-=6;if(event.key===Qt.Key_Right)x+=6;if(event.key===Qt.Key_Up)y-=6;if(event.key===Qt.Key_Down)y+=6;keyboardAim=Qt.point(Math.max(0,Math.min(230,x)),Math.max(-32.5,Math.min(280,y)));controller.move(keyboardAim.x,keyboardAim.y);event.accepted=true}
    }
}
