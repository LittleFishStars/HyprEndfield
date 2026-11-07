import QtQuick
import QtMultimedia
import "Appearance.js" as Appearance

// NotesWorkspace.mediaSeekRect: play at x=5, rail at x=31, footer 25 px.
Item {
    id: preview
    property url source
    property string kind: "image"
    property bool presented: visible
    property bool available: true
    property bool projectionStyle: false
    property bool gifPlaying: true
    readonly property bool video: kind === "video"
    readonly property bool moving: video || kind === "gif"
    readonly property bool playing: video ? player.playbackState === MediaPlayer.PlayingState : gifPlaying
    readonly property int footerHeight: projectionStyle ? 28 : 25
    onPresentedChanged: if (!presented) player.pause()
    onSourceChanged: { player.stop(); gifPlaying = true; }
    Image { anchors.fill: parent; visible: !preview.moving; source: visible ? preview.source : ""; asynchronous: true; fillMode: Image.PreserveAspectFit; mipmap: true }
    AnimatedImage { id: animation; objectName: "mediaAnimation"; anchors.fill: parent; anchors.bottomMargin: preview.footerHeight; visible: preview.kind === "gif"; source: visible ? preview.source : ""; playing: preview.presented && preview.gifPlaying; cache: false; fillMode: Image.PreserveAspectFit }
    VideoOutput { id: output; anchors.fill: parent; anchors.bottomMargin: preview.footerHeight; visible: preview.video; fillMode: VideoOutput.PreserveAspectFit }
    AudioOutput { id: audio; objectName: "mediaAudio" }
    MediaPlayer { id: player; objectName: "mediaPlayer"; source: preview.video ? preview.source : ""; videoOutput: output; audioOutput: audio }
    Text {
        anchors.centerIn: parent; width: parent.width - 12; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
        visible: !preview.available || player.error !== MediaPlayer.NoError || animation.status === Image.Error
        text: player.error !== MediaPlayer.NoError ? player.errorString : qsTr("媒体不可用")
        color: Appearance.tone("#AAAAAA"); font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering
    }
    Rectangle {
        x: preview.projectionStyle ? 2 : 0; y: parent.height - (preview.projectionStyle ? 20 : 25); width: preview.projectionStyle ? 22 : 20; height: width; visible: preview.moving; color: Appearance.tone("#262626"); border.color: Appearance.tone("#3A3A3A")
        SourceGlyph { anchors.centerIn: parent; width: 12; height: 12; kind: preview.playing ? "pause" : "play"; ink: Appearance.tone("#DDDDDD") }
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { if (!preview.video) preview.gifPlaying = !preview.gifPlaying; else if (preview.playing) player.pause(); else if (preview.presented) player.play(); } }
    }
    Item {
        x: preview.projectionStyle ? 32 : 26; y: parent.height - (preview.projectionStyle ? 15 : 25); width: Math.max(1, parent.width - (preview.projectionStyle ? 98 : 45)); height: preview.projectionStyle ? 12 : 20; visible: preview.video
        Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: preview.projectionStyle ? 2 : 4; color: Appearance.tone("#252525") }
        Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width * (player.duration > 0 ? player.position / player.duration : 0); height: preview.projectionStyle ? 2 : 4; color: Appearance.accent("#FFFA00") }
        Rectangle { anchors.verticalCenter: parent.verticalCenter; x: (parent.width - width) * (player.duration > 0 ? player.position / player.duration : 0); width: preview.projectionStyle ? 5 : 4; height: preview.projectionStyle ? 8 : 12; color: Appearance.tone("#EEEEEE") }
        MouseArea { anchors.fill: parent; enabled: player.seekable; cursorShape: Qt.PointingHandCursor; onPressed: mouse => player.setPosition(Math.max(0, Math.min(1, mouse.x / width)) * player.duration); onPositionChanged: mouse => { if (pressed) player.setPosition(Math.max(0, Math.min(1, mouse.x / width)) * player.duration); } }
    }
    Text { x: parent.width - 57; y: parent.height - 13; width: 44; height: 10; visible: preview.video && preview.projectionStyle; text: Math.floor(player.position / 60000) + ":" + String(Math.floor(player.position / 1000) % 60).padStart(2, "0"); color: Appearance.tone("#CCCCCC"); font.family: harmonyRegular.name; font.pixelSize: 10; renderType: Text.CurveRendering }
}
