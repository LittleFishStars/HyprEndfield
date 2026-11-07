import QtQuick
import QtQuick.Shapes
import QtQuick.Controls as Controls
import QtQuick.Dialogs
import HyprEndfield.Media 1.0
import "Appearance.js" as Appearance

// MediaAssemblyCanvas: stage(-10,81,448,412), panel(448,81,152,412).
Item {
    id: page; width: 590; height: 510
    property bool presented: false
    property int tab: 0
    property string tool: ""
    property string confirmation: ""
    readonly property var state: editor.state
    readonly property var a: state.adjustments
    readonly property real duration: state.metadata.duration || 0
    readonly property bool editable: state.loaded && !state.busy && !state.exporting
    Connections { target: typeof settingsController === "undefined" ? null : settingsController; function onLanguageChanged() { editor.retranslate() } }
    function params() {
        if (tool === "curves") return [0,1,2,3,4].map(i => ({key: "curve" + i, title: i * 25 + "%", min: 0, max: 1, step: .01}))
        if (tool === "levels") return [{key: "black", title: qsTr("黑场"), min:0, max:1, step:.01}, {key:"gamma", title:qsTr("中间调"), min:.1,max:4,step:.01}, {key:"white",title:qsTr("白场"),min:0,max:1,step:.01}]
        if (tool === "trim") return [{key:"trimStart",title:qsTr("开始"),min:0,max:page.duration || 1,step:.05},{key:"trimEnd",title:qsTr("结束"),min:0,max:page.duration || 1,step:.05}]
        return [{key:"brightness",title:qsTr("亮度"),min:-1,max:1,step:.01},{key:"contrast",title:qsTr("对比度"),min:0,max:4,step:.01},{key:"saturation",title:qsTr("饱和度"),min:0,max:2,step:.01},{key:"temperature",title:qsTr("色温"),min:2000,max:12000,step:100},{key:"tint",title:qsTr("色调"),min:-200,max:200,step:1},{key:"highlights",title:qsTr("高光"),min:0,max:1,step:.01,unsupported:true},{key:"shadows",title:qsTr("阴影"),min:0,max:1,step:.01,unsupported:true},{key:"exposure",title:qsTr("曝光度"),min:-4,max:4,step:.01}]
    }
    function toolTitle() { return ({crop:qsTr("裁剪"),adjust:qsTr("调整"),curves:qsTr("曲线"),levels:qsTr("色阶"),trim:qsTr("剪辑")})[tool] || "" }
    function selectTool(name) { tool = name; editor.cropping = name === "crop" }
    function time(seconds) { return Math.floor(seconds / 60).toString().padStart(2, "0") + ":" + Math.floor(seconds % 60).toString().padStart(2, "0") }
    SettingsHeader { title: qsTr("影像加工"); english: "MEDIA ASSEMBLY"; code: "MOD-05"; count: page.state.exporting ? Math.floor(page.state.progress * 100) + "%" : page.state.loaded ? (page.state.video ? "VID 01" : "IMG 01") : "EMPTY" }
    Row { x: 400; y: 24; spacing: 6
        CalendarButton { width: 64; title: qsTr("打开"); kind: "secondary"; enabled: !page.state.exporting; onClicked: sources.visible = !sources.visible }
        CalendarButton { width: 30; kind: "icon"; glyph: "×"; enabled: page.editable; onClicked: page.confirmation = "close" }
        CalendarButton { width: 76; title: page.state.exporting ? qsTr("取消") : qsTr("导出"); enabled: page.state.loaded && !page.state.busy; onClicked: { if (page.state.exporting) editor.cancelExport(); else saveDialog.open(); } }
    }
    Item { x: -10; y: 81; width: 448; height: 412
        MediaEditor { id: editor; objectName: "mediaEditor"; anchors.fill: parent; presented: page.presented
            onEventOccurred: action => { if (typeof eventLogController !== "undefined") eventLogController.recordAction("mediaAssemblyAction", action, action === "edited") }
        }
        Rectangle { anchors.fill: parent; color: "transparent"; border.color: Appearance.tone("#444444") }
        Column { y: 165; width: 448; spacing: 10; visible: !page.state.loaded
            Text { width: 448; horizontalAlignment: Text.AlignHCenter; text: "[ NO SOURCE ]"; color: Appearance.tone("#CCCCCC"); font.family: harmonyBold.name; font.pixelSize: 20; renderType: Text.CurveRendering }
            Text { width: 448; horizontalAlignment: Text.AlignHCenter; text: page.state.busy ? qsTr("正在打开…") : qsTr("打开图片或视频开始编辑"); color: Appearance.tone("#8A8A8A"); font.family: harmonyRegular.name; font.pixelSize: Math.round(11.5); renderType: Text.CurveRendering }
            CalendarButton { anchors.horizontalCenter: parent.horizontalCenter; width: 70; title: qsTr("打开"); enabled: !page.state.busy; onClicked: sources.visible = !sources.visible }
        }
        Rectangle { x: 10; y: 364; width: 134; height: 38; visible: page.state.loaded; color: "#D10E0E0E"; border.color: "#444444"
            CalendarButton { x: 4; y: 4; width: 30; kind: "icon"; glyph: "−"; onClicked: editor.zoomBy(.8) }
            Text { x: 38; y: 4; width: 58; height: 30; text: Math.round(page.state.zoom * 100) + "%"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: "#EEEEEE"; font.family: harmonyBold.name; font.pixelSize: 11; renderType: Text.CurveRendering; MouseArea { anchors.fill: parent; onClicked: editor.resetZoom() } }
            CalendarButton { x: 100; y: 4; width: 30; kind: "icon"; glyph: "+"; onClicked: editor.zoomBy(1.25) }
        }
        Rectangle { x: 150; y: 364; width: 288; height: 38; visible: page.state.video; color: "#D10E0E0E"; border.color: "#444444"
            CalendarButton { x: 4; y: 4; width: 30; kind: "icon"; glyph: page.state.playing ? "Ⅱ" : "▶"; enabled: page.editable && !editor.cropping; onClicked: editor.togglePlayback() }
            Rectangle { x: 44; y: 18; width: 152; height: 3; color: "#444444"
                Rectangle { width: parent.width * page.state.position / Math.max(1, page.state.metadata.duration || 0); height: 3; color: Appearance.accent("#FFFA00") }
                MouseArea { anchors.fill: parent; anchors.topMargin: -12; anchors.bottomMargin: -12; enabled: page.editable; onPressed: mouse => editor.seek(Math.max(0, Math.min(1, mouse.x / width)) * page.state.metadata.duration); onPositionChanged: mouse => { if (pressed) editor.seek(Math.max(0, Math.min(1, mouse.x / width)) * page.state.metadata.duration); } }
            }
            Text { x: 202; y: 13.5; width: 76; horizontalAlignment: Text.AlignRight; text: page.time(page.state.position); color: "#EEEEEE"; font.family: harmonyBold.name; font.pixelSize: 11; renderType: Text.CurveRendering }
        }
        Text { x: 12; y: 12; width: 424; elide: Text.ElideRight; text: page.state.metadata.name || ""; visible: page.state.loaded; color: "#DDDDDD"; font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering }
        Rectangle { x: 12; y: 324; width: 424 * page.state.progress; height: 4; visible: page.state.exporting; color: Appearance.accent("#FFFA00") }
    }
    Item { x: 448; y: 81; width: 152; height: 412; opacity: page.editable ? 1 : .35; enabled: page.editable
        Row { spacing: 0
            Repeater { model: [qsTr("调整"), qsTr("滤镜"), qsTr("贴纸")]
                Rectangle { required property string modelData; required property int index; width: 152 / 3; height: 34; color: page.tab === index ? Appearance.accent("#FFFA00") : "#242424"
                    Text { anchors.centerIn: parent; text: modelData; color: page.tab === index ? "#111111" : "#DDDDDD"; font.family: harmonyMedium.name; font.pixelSize: 11; renderType: Text.CurveRendering }
                    MouseArea { anchors.fill: parent; onClicked: { page.tab = index; page.selectTool(""); } }
                }
            }
        }
        Grid { y: 42; columns: 2; spacing: 6; visible: page.tab === 0 && !page.tool
            Repeater { model: [{key:"crop",title:qsTr("裁剪"),icon:"grid"},{key:"rotate",title:qsTr("旋转"),icon:"rotate"},{key:"mirror",title:qsTr("镜像"),icon:"mirror"},{key:"adjust",title:qsTr("调整"),icon:"gear"},{key:"curves",title:qsTr("曲线"),icon:"lyric"},{key:"levels",title:qsTr("色阶"),icon:"apps"}]
                Item { id: tile; required property var modelData; width: 73; height: 62
                    Shape { anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
                        ShapePath { strokeWidth: -1; fillGradient: LinearGradient { x1:0; y1:0; x2:0; y2:62; GradientStop {position:0;color:tile.modelData.key === "mirror" && page.a.mirror ? Appearance.accent("#FFFC6B") : "#E4E5E0"} GradientStop {position:1;color:tile.modelData.key === "mirror" && page.a.mirror ? Appearance.accent("#D9D300") : "#C6C7C2"} } PathSvg {path:"M0 0H65L73 8V62H0Z"} }
                    }
                    SourceGlyph { x: 27.5; y: 12; width: 18; height: 18; kind: tile.modelData.icon; ink: "#161616" }
                    Text { x: 4; y: 37; width: 65; horizontalAlignment: Text.AlignHCenter; text: tile.modelData.title; color:"#161616"; font.family: harmonyMedium.name; font.pixelSize: 12; renderType: Text.CurveRendering }
                    MouseArea { anchors.fill: parent; onClicked: { const key = tile.modelData.key; if (key === "rotate") editor.setAdjustment("rotation", page.a.rotation + 1); else if (key === "mirror") editor.setAdjustment("mirror", !page.a.mirror); else page.selectTool(key); } }
                }
            }
        }
        CalendarButton { y: 246; width: 152; height: 36; visible: page.tab === 0 && !page.tool && page.state.video; title: qsTr("剪辑"); kind:"secondary"; onClicked: page.selectTool("trim") }
        CalendarButton { y: page.state.video ? 288 : 246; width: 152; height: 36; visible: page.tab === 0 && !page.tool; title: qsTr("全部重置"); kind:"secondary"; onClicked: page.confirmation = "reset" }
        Item { y: 42; width: 152; height: 30; visible: page.tab === 0 && !!page.tool
            CalendarButton { width:30; kind:"icon"; glyph:"‹"; onClicked:page.selectTool("") }
            Text { x:38; y:8; width:78; text:page.toolTitle(); color:"#EEEEEE"; font.family:harmonyBold.name; font.pixelSize:13; renderType:Text.CurveRendering }
            CalendarButton { x:122; width:30; kind:"icon"; glyph:"↻"; visible:page.tool !== "crop"; onClicked:editor.reset(page.tool) }
        }
        CalendarButton { y:78; width:152; height:32; visible:page.tool === "crop"; title:qsTr("重置裁剪"); kind:"secondary"; onClicked:editor.reset("crop") }
        CalendarButton { y:116; width:152; height:32; visible:page.tool === "crop"; title:qsTr("完成"); onClicked:page.selectTool("") }
        Flickable { y:78; width:152; height:330; visible:page.tab === 0 && !!page.tool && page.tool !== "crop"; clip:true; contentHeight:parameterRows.height; boundsBehavior:Flickable.StopAtBounds
            Controls.ScrollBar.vertical: Controls.ScrollBar { width:4 }
            Column { id:parameterRows; spacing:2
                Repeater { model:page.params()
                    Item { required property var modelData; width:146; height:44
                        Text { x:0; y:2; width:146; text:modelData.title + (modelData.unsupported ? " · " + qsTr("暂不支持") : ""); color:modelData.unsupported ? "#777777" : "#CCCCCC"; font.family:harmonyRegular.name; font.pixelSize:10; renderType:Text.CurveRendering }
                        HudSlider { y:17; width:146; height:27; minimum:modelData.min; maximum:modelData.max; stepSize:modelData.step; value:page.a[modelData.key] || 0; valueFormat:"decimal"; enabled:!modelData.unsupported; onValueModified:(value, final)=>editor.setAdjustment(modelData.key,value) }
                    }
                }
            }
        }
        Flickable { y:42; width:152; height:366; clip:true; visible:page.tab !== 0; contentHeight:presetGrid.height + 6; boundsBehavior:Flickable.StopAtBounds; opacity:page.tab === 2 && page.state.video ? .35 : 1
            Controls.ScrollBar.vertical: Controls.ScrollBar { width:4 }
            Grid { id:presetGrid; x:3; y:3; columns:page.tab === 1 ? 2 : 3; spacing:6
                Repeater { model:page.tab === 1 ? editor.filters : editor.stickers
                    Rectangle { required property var modelData; width:page.tab === 1 ? 69 : 44; height:page.tab === 1 ? 72 : 52; color:page.tab === 1 ? "#1E1E1E" : "#202020"; border.width:page.tab === 1 && page.a.filter === modelData.id ? 2 : 1; border.color:border.width === 2 ? Appearance.accent("#FFFA00") : "#363636"
                        Image { x:(parent.width - width)/2; y:page.tab === 1 ? 6 : 5; width:page.tab === 1 ? 44 : 42; height:width; source:modelData.source; fillMode:Image.PreserveAspectFit; mipmap:true }
                        Text { anchors.horizontalCenter:parent.horizontalCenter; y:15; visible:!modelData.source; text:"∅"; font.pixelSize:22; color:"#CCCCCC" }
                        Rectangle { y:parent.height - 18; width:parent.width; height:18; visible:page.tab === 1; color:page.a.filter === modelData.id ? Appearance.accent("#FFFA00") : "#80000000"
                            Text { x:4; y:4; width:parent.width - 8; text:modelData.title; elide:Text.ElideRight; horizontalAlignment:Text.AlignHCenter; color:page.a.filter === modelData.id ? "#111111" : "#DDDDDD"; font.family:harmonyMedium.name; font.pixelSize:10; renderType:Text.CurveRendering }
                        }
                        MouseArea { anchors.fill:parent; enabled:page.tab === 1 || !page.state.video; onClicked:{ if(page.tab === 1) editor.setFilter(modelData.id); else editor.addSticker(modelData.id); } }
                    }
                }
            }
        }
    }
    Text { x:-10; y:497; width:610; text:page.state.error || page.state.message; color:page.state.error ? "#FF6B6D" : "#AAAAAA"; elide:Text.ElideRight; font.family:harmonyRegular.name; font.pixelSize:11; renderType:Text.CurveRendering }
    MediaSourceMenu { id:sources; x:330; y:60; visible:false; onChooseFile:openDialog.open(); onChosen:source=>{ page.selectTool(""); editor.openSource(source); } }
    FileDialog { id:openDialog; title:qsTr("图片/视频"); nameFilters:[qsTr("图片与视频 (*.png *.jpg *.jpeg *.webp *.gif *.bmp *.tif *.tiff *.mp4 *.mkv *.webm *.mov *.avi *.m4v *.ogv)"),qsTr("所有文件 (*)")]; onAccepted:{ page.selectTool(""); editor.openSource(selectedFile); } }
    FileDialog { id:saveDialog; title:qsTr("导出"); fileMode:FileDialog.SaveFile; defaultSuffix:page.state.video ? "mp4" : "png"; nameFilters:page.state.video ? ["MP4 (*.mp4)"] : ["PNG (*.png)","JPEG (*.jpg *.jpeg)","TIFF (*.tif *.tiff)","WebP (*.webp)"]; onAccepted:editor.exportTo(selectedFile) }
    Rectangle { x:105; y:170; width:380; height:112; visible:!!page.confirmation || !!page.state.overwrite; color:"#242424"; border.color:"#777777"; z:100
        MouseArea { anchors.fill:parent }
        Text { x:16; y:16; width:348; text:page.state.overwrite ? qsTr("确认覆盖目标文件？") : page.confirmation === "reset" ? qsTr("重置全部调整？") : qsTr("关闭源文件并放弃调整？"); color:"#EEEEEE"; font.family:harmonyBold.name; font.pixelSize:14; renderType:Text.CurveRendering }
        CalendarButton { x:204; y:70; width:74; title:qsTr("取消"); kind:"secondary"; onClicked:{ page.confirmation = ""; editor.cancelExport(); } }
        CalendarButton { x:288; y:70; width:74; title:qsTr("确认"); onClicked:{ if(page.state.overwrite) editor.exportTo(page.state.overwrite,true); else if(page.confirmation === "reset") editor.reset(); else { editor.closeSource(); page.selectTool(""); } page.confirmation = ""; } }
    }
}
