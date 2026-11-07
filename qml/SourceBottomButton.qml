import QtQuick
import QtQuick.Shapes

Item {
    id: button
    required property var shell
    required property var spec
    required property string label
    signal triggered
    width: 1920
    height: 1080
    SourceSurface {
        shell: button.shell
        spec: button.spec.face
        Shape {
            id: silhouette
            visible: false
            containsMode: Shape.FillContains
            ShapePath {
                strokeWidth: -1
                PathSvg { path: button.spec.face.outline }
            }
        }
        MouseArea {
            anchors.fill: parent
            containmentMask: silhouette
            cursorShape: Qt.PointingHandCursor
            onClicked: button.triggered()
        }
    }
    SourceSurface {
        shell: button.shell
        spec: button.spec.light
    }
    SourceSurface {
        shell: button.shell
        spec: button.spec.label
        drawImage: false
        Text {
            anchors.fill: parent
            text: button.label
            color: "white"
            font.family: button.shell.selectedModule === (button.spec.icon.glyph ? "storage" : "activity") ? harmonyBold.name : harmonyMedium.name
            font.pixelSize: 20
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            renderType: Text.CurveRendering
        }
    }
    SourceSurface {
        shell: button.shell
        spec: button.spec.icon
        HudGlyph {
            visible: Boolean(button.spec.icon.glyph)
            anchors.centerIn: parent
            // HUDSourceDesktopIconLayout fits a 28x28 factory glyph to a
            // 23x23 visible area inside its 32-point box.
            width: parent.width * 23 / 28
            height: parent.height * 23 / 28
            kind: button.spec.icon.glyph || ""
            ink: "white"
        }
    }
}
