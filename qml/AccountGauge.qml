import QtQuick
import HyprEndfield.Account 1.0

// HUDAccountGauge: source MoneyCell artwork and 168 × 42 interaction rect.
Item {
    id:gauge
    property var controller:accountController
    readonly property var account:controller.state
    property bool presented:true
    property bool opened:false
    property bool reduceMotion:false
    width:168; height:42
    visible:account.gaugeVisible
    onPresentedChanged:if(!presented)opened=false
    onVisibleChanged:if(!visible)opened=false
    AccountGaugeArtwork { y:-6; value:gauge.account.gauge; highlighted:pointer.containsMouse || gauge.opened }
    MouseArea { id:pointer; anchors.fill:parent; hoverEnabled:true; cursorShape:Qt.PointingHandCursor; onClicked:gauge.opened=!gauge.opened }
    Rectangle {
        id:popover; x:168-width; y:40; height:65; width:Math.min(300,Math.max(224,10+labelMetrics.width+2+14+Math.max(nextMetrics.width,fullMetrics.width,templateMetrics.width)+2+10+23+6))
        visible:gauge.opened; radius:4; color:Qt.rgba(.065,.065,.065,.98)
        FontMetrics { id:fontMetrics; font.family:harmonyMedium.name; font.pointSize:14.4*.75 }
        TextMetrics { id:labelMetrics; font:fontMetrics.font; text:qsTr("全部回复") }
        TextMetrics { id:nextMetrics; font:fontMetrics.font; text:gauge.account.gaugeNext }
        TextMetrics { id:fullMetrics; font:fontMetrics.font; text:gauge.account.gaugeFull }
        TextMetrics { id:templateMetrics; font:fontMetrics.font; text:"000:00:00" }
        Repeater {
            model:[{label:qsTr("下次回复"),value:gauge.account.gaugeNext},{label:qsTr("全部回复"),value:gauge.account.gaugeFull}]
            Item { required property int index; required property var modelData; x:10; y:index ? 33 : 6; width:popover.width-49; height:25
                Text { width:parent.width-number.width-14; height:25; text:parent.modelData.label; color:"#999999"; elide:Text.ElideRight; font:fontMetrics.font; verticalAlignment:Text.AlignVCenter; renderType:Text.CurveRendering }
                Text { id:number; anchors.right:parent.right; height:25; text:parent.modelData.value; color:"#EEEEEE"; font:fontMetrics.font; verticalAlignment:Text.AlignVCenter; renderType:Text.CurveRendering }
            }
        }
        CalendarButton { x:parent.width-29; y:19; width:23; height:25; kind:"icon"; glyph:"↻"; enabled:!gauge.account.busy && !gauge.account.reconnect; reduceMotion:gauge.reduceMotion; onClicked:gauge.controller.action("refresh") }
    }
}
