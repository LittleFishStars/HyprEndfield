import QtQuick
import QtQuick.Shapes
import QtQuick.Effects

// HUDDropdown + HUDMenu: source dimensions, placement, wipe and hover motion.
Item {
    id: field
    required property Item overlayHost
    property var options: []
    property string selectedKey: ""
    property string text: "—"
    property bool opened: false
    property bool reduceMotion: false
    property int highlighted: -1
    property point menuPoint: Qt.point(0, 0)
    property real reveal: 0
    readonly property string optionKeys: options.map(item => item.key + "|" + item.label).join("\n")
    property bool scrollActive: false
    signal selected(string key)
    width: 178; height: 30
    opacity: enabled ? 1 : 0.35
    onEnabledChanged: if (!enabled) opened = false
    onVisibleChanged: if (!visible) opened = false
    onOptionKeysChanged: if (opened) opened = false
    onOpenedChanged: {
        if (opened) {
            menuPoint = mapToItem(overlayHost, 0, 0);
            highlighted = options.findIndex(item => item.key === selectedKey);
            menuScroll.contentY = Math.max(0, Math.min(menuScroll.contentHeight - menuScroll.height, highlighted * 30 - 60));
            wipe.restart();
        }
    }
    function move(delta) {
        if (!options.length) return;
        highlighted = (highlighted + delta + options.length) % options.length;
        menuScroll.contentY = Math.max(0, Math.min(menuScroll.contentHeight - menuScroll.height,
            highlighted * 30 < menuScroll.contentY ? highlighted * 30 : Math.max(menuScroll.contentY, (highlighted + 1) * 30 - menuScroll.height)));
    }
    function choose() {
        if (highlighted >= 0 && highlighted < options.length) selected(options[highlighted].key);
        opened = false;
    }
    Rectangle { anchors.fill: parent; color: "#212121"; border.width: 1; border.color: pointer.containsMouse || field.opened ? "#666666" : "#3A3A3A" }
    Rectangle {
        width: 2; height: field.opened || pointer.containsMouse ? 30 : 0; anchors.verticalCenter: parent.verticalCenter; color: "#FFFA00"
        Behavior on height { NumberAnimation { duration: field.reduceMotion ? 0 : 140; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] } }
    }
    Text {
        x: 12; width: 133; anchors.verticalCenter: parent.verticalCenter
        text: field.text; color: "#EEEEEE"; elide: Text.ElideRight
        font.family: harmonyMedium.name; font.pointSize: 12.5 * 0.75; font.weight: Font.Medium; renderType: Text.CurveRendering
    }
    Item {
        x: field.width - 19; y: 12 + (pointer.containsMouse ? 2 : 0); width: 9; height: 6; rotation: field.opened ? 180 : 0
        Behavior on rotation { NumberAnimation { duration: field.reduceMotion ? 0 : 160; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] } }
        Behavior on y { NumberAnimation { duration: field.reduceMotion ? 0 : 160; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] } }
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath { strokeWidth: -1; fillColor: "#FFFA00"; PathSvg { path: "M0 0H9L4.5 6Z" } }
        }
    }
    MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: field.opened = !field.opened }
    FontMetrics { id: metrics; font.family: harmonyRegular.name; font.pointSize: 12.5 * 0.75 }
    NumberAnimation { id: wipe; target: field; property: "reveal"; from: 0; to: 1; duration: field.reduceMotion ? 0 : 180; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0.8, 0.2, 1, 1, 1] }
    Timer { id: scrollLinger; interval: 1000; onTriggered: field.scrollActive = false }
    Item {
        parent: field.overlayHost; anchors.fill: parent; z: 30; visible: field.opened
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton; onClicked: field.opened = false }
        Item {
            id: panel
            width: Math.ceil(Math.max(178, ...field.options.map(item => metrics.advanceWidth(item.label) + 50 + (field.options.length > 7 ? 12 : 0))))
            height: Math.min(210, field.options.length * 30) + 8
            x: Math.max(0, Math.min(field.menuPoint.x, field.overlayHost.width - width))
            y: field.menuPoint.y + 34 + height > field.overlayHost.height + 20 ? field.menuPoint.y - height - 4 : field.menuPoint.y + 34
            opacity: 0.4 + 0.6 * field.reveal
            Rectangle {
                width: panel.width; height: panel.height * field.reveal; color: "#1A1A1A"
                layer.enabled: field.opened
                layer.effect: MultiEffect { shadowEnabled: true; shadowColor: Qt.rgba(0, 0, 0, 0.6); shadowVerticalOffset: 18; shadowBlur: 1; blurMax: 20 }
            }
            Item {
                width: panel.width; height: panel.height * field.reveal; clip: true
                Rectangle {
                    width: panel.width; height: panel.height; color: "#1A1A1A"; border.width: 1; border.color: "#3D3D3D"
                }
                Flickable {
                    id: menuScroll
                    y: 4; width: panel.width - (field.options.length > 7 ? 12 : 0); height: panel.height - 8
                    contentHeight: field.options.length * 30; clip: true; boundsBehavior: Flickable.StopAtBounds
                    onContentYChanged: { field.scrollActive = true; scrollLinger.restart(); }
                    Repeater {
                        model: field.options
                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            y: index * 30; width: menuScroll.width; height: 30
                            color: field.highlighted === index ? "#D6D7D2" : "transparent"
                            Rectangle { visible: field.highlighted === parent.index; x: 0; y: 5; width: 3; height: 20; color: "#111111" }
                            Text {
                                x: 14; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 50
                                text: modelData.label; color: field.highlighted === parent.index ? "#111111" : "#D9D9D9"
                                font.family: harmonyRegular.name; font.pointSize: 12.5 * 0.75; renderType: Text.CurveRendering
                            }
                            Rectangle {
                                visible: modelData.key === field.selectedKey
                                x: parent.width - 24; y: 9; width: 12; height: 12; color: "#1A1A1A"; border.width: 1; border.color: "#FFFA00"
                                Rectangle { x: 3; y: 3; width: 6; height: 6; color: "#FFFA00" }
                            }
                            MouseArea {
                                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onEntered: field.highlighted = parent.index
                                onClicked: { field.highlighted = parent.index; field.choose(); }
                            }
                        }
                    }
                }
                Rectangle {
                    visible: field.options.length > 7
                    x: panel.width - 9; y: 8; width: 5; height: panel.height - 16; color: "#2C2C2C"
                    opacity: field.scrollActive || trackPointer.containsMouse || trackPointer.pressed ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: field.reduceMotion ? 0 : 220 } }
                    Rectangle { width: 5; height: Math.max(18, parent.height * menuScroll.height / menuScroll.contentHeight); y: menuScroll.contentY / Math.max(1, menuScroll.contentHeight - menuScroll.height) * (parent.height - height); color: "#FFFA00" }
                    MouseArea { id: trackPointer; anchors.fill: parent; hoverEnabled: true; onPressed: mouse => move(mouse.y); onPositionChanged: mouse => { if (pressed) move(mouse.y); }; function move(y) { menuScroll.contentY = Math.max(0, Math.min(1, y / height)) * (menuScroll.contentHeight - menuScroll.height); } }
                }
            }
            Shape {
                x: -1; y: -1; width: 10; height: 10
                preferredRendererType: Shape.CurveRenderer
                ShapePath { strokeWidth: -1; fillColor: "#FFFA00"; PathSvg { path: "M0 0H10V2H2V10H0Z" } }
            }
        }
    }
}
