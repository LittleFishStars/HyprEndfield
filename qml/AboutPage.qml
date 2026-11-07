import QtQuick
import QtQuick.Shapes
import QtQuick.Controls
import "Appearance.js" as Appearance

// HUDSettingsCanvas.buildAbout / aboutHero / buildCredits (SYS-04).
Item {
    id: root
    property var settings: settingsController
    property bool credits: false
    property int checkStep: 0
    readonly property var state: settings.updateState
    readonly property var acknowledgements: [
        {name: "DDDuoDuo / EndfieldHUD", role: qsTr("原始界面、动画与资源"), link: "source"},
        {name: "Arknights: Endfield", role: qsTr("非官方同人项目"), link: ""},
        {name: "llynxxx", role: qsTr("灵感来源"), link: "inspiration"},
        {name: "QinAnze / 氰氨锗", role: qsTr("灵感来源"), link: "charge"},
        {name: "Ronit Singh / FineTune", role: qsTr("音频架构参考"), link: "audio"},
        {name: "Qt / LayerShellQt / libvterm", role: qsTr("Linux 图形界面与终端组件"), link: ""}
    ]
    width: 590; height: 510
    SettingsHeader { title: root.credits ? qsTr("致谢") : qsTr("关于"); english: root.credits ? "CREDITS" : "ABOUT"; crumbs: root.credits ? "ABOUT / CREDITS" : "ABOUT"; code: "SYS-04"; count: root.credits ? "06" : "v" + root.settings.version; back: root.credits; onBackRequested: root.credits = false }
    Item {
        visible: !root.credits; y: 81; width: 590; height: 429
        Shape {
            width: 590; height: 84; preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeWidth: -1
                fillGradient: LinearGradient { x1: 0; y1: 0; x2: 590; y2: 84; GradientStop { position: 0; color: "#E7E8E3" } GradientStop { position: 1; color: "#D1D2CD" } }
                PathSvg { path: "M0 0H576L590 14V84H0Z" }
            }
        }
        Column { y: 14; Repeater { model: ["#FF1AAC", "#00FFA2", "#FFFA00"]; Rectangle { required property string modelData; width: 4; height: 38 / 3; color: modelData } } }
        Rectangle { x: 20; y: 14; width: 118; height: 9; color: "#141414"; Text { x: 4; width: 114; height: 9; text: "ENDFIELD. INDUSTRIES"; color: "white"; font.family: harmonyBold.name; font.pixelSize: 6; verticalAlignment: Text.AlignVCenter; renderType: Text.CurveRendering } }
        Text { x: 20; y: 28; width: 320; height: 40; text: "HyprEndfield"; color: "#111111"; font.family: harmonyBold.name; font.pixelSize: 30; font.weight: Font.Black; renderType: Text.CurveRendering }
        Text { x: 350; y: 10; width: 220; height: 48; text: root.settings.version; color: "#111111"; font.family: harmonyRegular.name; font.pixelSize: 38; horizontalAlignment: Text.AlignRight; renderType: Text.CurveRendering }
        Text { x: 350; y: 58; width: 220; height: 14; text: "BUILD " + root.settings.buildId; color: "#555555"; font.family: harmonyBold.name; font.pixelSize: 10; horizontalAlignment: Text.AlignRight; renderType: Text.CurveRendering }
        Row {
            y: 101; spacing: 4
            Repeater { model: 4; Rectangle { required property int index; width: 108.5; height: 10; color: index < (root.state.phase === "checking" ? root.checkStep : root.state.phase === "current" || root.state.phase === "available" ? 4 : 0) ? Appearance.accent("#FFFA00") : Appearance.tone("#333333") } }
        }
        Text { x: 460; y: 96; width: 130; height: 20; text: root.state.phase === "idle" ? qsTr("尚未检查") : root.state.phase === "checking" ? qsTr("连接中") : root.state.phase === "current" ? qsTr("已是最新") : root.state.phase === "failed" ? qsTr("检查失败") : qsTr("发现不同提交"); horizontalAlignment: Text.AlignRight; color: Appearance.tone("#D0D0D0"); font.family: harmonyBold.name; font.pixelSize: 11; renderType: Text.CurveRendering }
        Flickable {
            y: 126; width: 590; height: 303; contentHeight: 8 * 56 - 6; clip: true; boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
            HudSettingsRow { objectName: "aboutCheckUpdates"; width: 580; title: qsTr("检查更新"); mode: "button"; buttonTitle: qsTr("检查"); enabled: root.state.phase !== "checking"; onActivated: { root.checkStep = 1; root.settings.checkUpdates() } }
            HudSettingsRow { y: 56; width: 580; title: qsTr("远端最新提交"); mode: "nav"; chipText: root.state.latest || "—"; onActivated: root.settings.openLink("latest") }
            HudSettingsRow { y: 112; width: 580; title: qsTr("自动安装更新"); sublabel: qsTr("源码构建，请手动更新并重新编译"); mode: "toggle"; switched: false; toggleEnabled: false }
            HudSettingsRow { y: 168; width: 580; title: qsTr("BUG报告及反馈"); mode: "nav"; onActivated: root.settings.openLink("feedback") }
            HudSettingsRow { y: 224; width: 580; title: qsTr("作者"); mode: "value"; value: "OctoberPrayRain / AmeoInoru" }
            HudSettingsRow { y: 280; width: 580; title: "GitHub"; mode: "nav"; onActivated: root.settings.openLink("repository") }
            HudSettingsRow { y: 336; width: 580; title: qsTr("许可证"); mode: "value"; value: "MIT" }
            HudSettingsRow { objectName: "aboutCredits"; y: 392; width: 580; title: qsTr("致谢"); mode: "nav"; onActivated: root.credits = true }
        }
    }
    Column {
        y: 81; spacing: 6; visible: root.credits
        Repeater { model: root.acknowledgements; HudSettingsRow { required property var modelData; title: modelData.name; sublabel: modelData.role; mode: modelData.link ? "nav" : "plain"; onActivated: root.settings.openLink(modelData.link) } }
    }
    Timer { interval: 650; running: root.state.phase === "checking"; repeat: true; onTriggered: root.checkStep = Math.min(3, root.checkStep + 1) }
    Connections { target: root.settings; function onUpdateStateChanged() { if (root.state.phase === "failed" || root.state.phase === "available") root.settings.setNotice(root.state.detail) } }
}
