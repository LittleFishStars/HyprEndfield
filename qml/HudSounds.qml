import QtQuick
import QtMultimedia

Item {
    id: sounds

    property int volume: 50
    property bool hudOpen: false
    property bool interactive: false
    property bool initialized: false
    readonly property real gain: Math.max(0, Math.min(100, volume)) / 100

    // Keep all four samples loaded, including while the HUD is hidden.
    // Each channel restarts in place; rapid input never builds a playback queue.
    SoundEffect {
        id: opening
        objectName: "hudOpenSound"
        source: "qrc:/assets/audio/HyprField_Open.wav"
        volume: sounds.gain
    }
    SoundEffect {
        id: closing
        objectName: "hudCloseSound"
        source: "qrc:/assets/audio/HyprField_Close.wav"
        volume: sounds.gain
    }
    SoundEffect {
        id: hovering
        objectName: "hudHoverSound"
        source: "qrc:/assets/audio/HyprField_Select.wav"
        volume: sounds.gain
    }
    SoundEffect {
        id: clicking
        objectName: "hudClickSound"
        source: "qrc:/assets/audio/HyprField_Click.wav"
        volume: sounds.gain
    }

    function stopAll() {
        opening.stop()
        closing.stop()
        hovering.stop()
        clicking.stop()
    }

    Component.onCompleted: {
        initialized = true
        if (hudOpen) playTransition()
    }
    onVolumeChanged: if (initialized && volume <= 0) stopAll()
    onHudOpenChanged: if (initialized) playTransition()

    function playTransition() {
        stopAll()
        if (volume > 0)
            (hudOpen ? opening : closing).play()
    }

    function playHover() {
        if (!interactive || !hudOpen || volume <= 0) return
        hovering.stop()
        hovering.play()
    }

    function playClick() {
        if (!interactive || !hudOpen || volume <= 0) return
        hovering.stop()
        clicking.stop()
        clicking.play()
    }
}
