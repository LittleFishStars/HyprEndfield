import QtQuick

// The item rectangle is the original Unity rect. Both drawing and pointer
// mapping use its parent-chain projection from EndfieldHUD's camera model.
Item {
    id: surface
    required property var shell
    required property var spec
    property bool drawImage: true
    property real ambientTime: -1
    width: spec.width
    height: spec.height
    transform: Matrix4x4 {
        matrix: sourceWatchAnimation.projectSourceNode(surface.spec.id,
                    surface.shell.animationPhase, surface.shell.sourceAnimationTime,
                    surface.shell.sourceGyro, surface.ambientTime)
    }
    Image {
        id: artwork
        anchors.fill: parent
        source: surface.drawImage && surface.spec.asset ? "qrc:/assets/watch-surfaces/" + surface.spec.asset : ""
        visible: false
        smooth: true
        mipmap: true
        fillMode: Image.Stretch
    }
    ShaderEffect {
        readonly property var imageFrame: surface.spec.imageFrame || [0, 0, 1, 1]
        x: parent.width * imageFrame[0]
        y: parent.height * imageFrame[1]
        width: parent.width * imageFrame[2]
        height: parent.height * imageFrame[3]
        visible: surface.drawImage && Boolean(surface.spec.asset)
        property var source: artwork
        property vector4d tint: surface.spec.color ? Qt.vector4d(surface.spec.color[0], surface.spec.color[1], surface.spec.color[2], surface.spec.color[3]) : Qt.vector4d(1, 1, 1, 1)
        property real additive: surface.spec.additive ? 1 : 0
        property real alphaOnly: surface.spec.desktopIcon ? 1 : 0
        fragmentShader: "qrc:/shaders/source-surface.frag.qsb"
    }
}
