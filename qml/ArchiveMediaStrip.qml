import QtQuick
import "Appearance.js" as Appearance
import QtMultimedia

// EndfieldHUD ArchiveMediaStrip: 16:10 preview, details and compact controls.
Rectangle {
    id: strip
    required property var media
    property int index: 0
    property bool reduceMotion: false
    property bool gifPlaying: true
    readonly property var current: media.length ? media[Math.max(0, Math.min(index, media.length - 1))] : null
    signal previous()
    signal next()
    signal removed()
    width: 590; height: 150; color: Appearance.tone("#1C1C1C"); border.color: Appearance.tone("#3A3A3A")

    Rectangle {
        x: 8; y: 8; width: 214; height: 134; color: Appearance.tone("#0E0E0E"); clip: true
        Image { anchors.fill: parent; source: strip.current && strip.current.kind === "image" ? strip.current.url : ""; fillMode: Image.PreserveAspectFit; asynchronous: true; visible: source.toString().length > 0 }
        AnimatedImage { anchors.fill: parent; source: strip.current && strip.current.kind === "gif" ? strip.current.url : ""; fillMode: Image.PreserveAspectFit; playing: strip.gifPlaying; visible: source.toString().length > 0 }
        VideoOutput { id: video; anchors.fill: parent; visible: strip.current && strip.current.kind === "video"; fillMode: VideoOutput.PreserveAspectFit }
        Text { anchors.centerIn: parent; visible: strip.current && !strip.current.available; text: qsTr("媒体不可用"); color: "#D63235"; font.family: harmonyMedium.name; font.pixelSize: 12; renderType: Text.CurveRendering }
    }
    AudioOutput { id: audio }
    MediaPlayer {
        id: player; source: strip.current && strip.current.kind === "video" ? strip.current.url : ""
        audioOutput: audio; videoOutput: video
    }
    onCurrentChanged: { player.stop(); if (current && current.kind === "gif") player.play() }
    Text { x: 234; y: 12; width: 344; elide: Text.ElideRight; text: strip.current ? strip.current.name : ""; color: Appearance.tone("#EEEEEE"); font.family: harmonyMedium.name; font.pixelSize: 13; font.weight: Font.Medium; renderType: Text.CurveRendering }
    Text { x: 234; y: 31; text: String(strip.index + 1).padStart(2, "0") + " / " + String(strip.media.length).padStart(2, "0"); color: Appearance.tone("#858585"); font.family: harmonyBold.name; font.pointSize: 9.5 * 0.75; font.weight: Font.Bold; font.letterSpacing: 0.57; renderType: Text.CurveRendering }
    Row {
        x: 234; y: 112; spacing: 6
        CalendarButton { visible: strip.media.length > 1; width: 30; height: 30; kind: "icon"; glyph: "‹"; reduceMotion: strip.reduceMotion; onClicked: strip.previous() }
        CalendarButton { visible: strip.media.length > 1; width: 30; height: 30; kind: "icon"; glyph: "›"; reduceMotion: strip.reduceMotion; onClicked: strip.next() }
        CalendarButton {
            visible: strip.current && strip.current.kind !== "image"; width: 30; height: 30; kind: "icon"
            glyph: strip.current && strip.current.kind === "gif" ? (strip.gifPlaying ? "Ⅱ" : "▶") : player.playbackState === MediaPlayer.PlayingState ? "Ⅱ" : "▶"
            reduceMotion: strip.reduceMotion
            onClicked: { if (strip.current.kind === "gif") strip.gifPlaying = !strip.gifPlaying; else if (player.playbackState === MediaPlayer.PlayingState) player.pause(); else player.play() }
        }
        CalendarButton { width: 30; height: 30; kind: "icon"; glyph: "×"; reduceMotion: strip.reduceMotion; onClicked: strip.removed() }
    }
    Rectangle {
        visible: strip.current && strip.current.kind === "video"; x: 384; y: 125; width: 194; height: 4; color: Appearance.tone("#252525")
        Rectangle { width: parent.width * (player.duration > 0 ? player.position / player.duration : 0); height: 4; color: Appearance.accent("#FFFA00") }
        MouseArea { anchors.fill: parent; onClicked: mouse => { if (player.duration > 0) player.position = mouse.x / width * player.duration } }
    }
}
