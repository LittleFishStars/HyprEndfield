import QtQuick

Item {
    id: plane
    required property var scene
    property real elevation: 0
    property bool compensated: true
    property matrix4x4 localMatrix: Qt.matrix4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)
    width: 1920
    height: 1080
    z: elevation
    transform: Matrix4x4 {
        matrix: sourceWatchAnimation.projectPlane(plane.elevation, plane.compensated, plane.scene.sourceGyro, plane.scene.animationPhase, plane.scene.sourceAnimationTime, plane.localMatrix)
    }
}
