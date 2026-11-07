import QtQuick
import QtQuick.VectorImage
import "MenuGeometry.js" as Geometry

Item {
    id: art
    required property string asset
    readonly property bool clipped: Geometry.data.rasterAssets.indexOf(asset) >= 0
    VectorImage {
        anchors.fill: parent
        visible: !art.clipped
        source: visible ? "qrc:/assets/menu/" + art.asset + ".svg" : ""
        preferredRendererType: VectorImage.CurveRenderer
        fillMode: VectorImage.Stretch
    }
    Image {
        anchors.fill: parent
        visible: art.clipped
        source: visible ? "qrc:/assets/menu/" + art.asset + ".png" : ""
        fillMode: Image.Stretch
        smooth: true
        mipmap: true
    }
}
