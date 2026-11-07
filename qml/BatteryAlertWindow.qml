import QtQuick
import QtQuick.Window
import org.kde.layershell 1.0 as LayerShell

Window {
    id: popup
    objectName: "batteryAlertWindow"
    width: 300 * settingsController.batteryScale; height: 84 * settingsController.batteryScale
    color: "transparent"; flags: Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus
    visible: settingsController.batteryAlertVisible && !appController.hudOpen && !settingsController.editingBatteryPosition
    LayerShell.Window.layer: LayerShell.Window.LayerOverlay
    LayerShell.Window.anchors: LayerShell.Window.AnchorTop
    LayerShell.Window.exclusionZone: -1
    LayerShell.Window.scope: "hyprendfield-battery"
    LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityNone
    function metric() {
        const kind = settingsController.batteryMetric, sample = activityController.state.latest;
        if (kind === 0) return batteryController.state;
        let result = {available: true, estimated: false, status: "", current: "—", full: "", unit: "", percentage: -1, progress: 0, trailing: "—"};
        if (kind === 1 && sample.memoryTotal > 0 && sample.memoryUsed >= 0) {
            result.current = (sample.memoryUsed / 1073741824).toFixed(1); result.full = (sample.memoryTotal / 1073741824).toFixed(1); result.unit = "GB";
            result.percentage = Math.round(sample.memoryUsed / sample.memoryTotal * 100);
        } else if (kind === 2 || kind === 5) {
            result.current = kind === 2 ? "CPU" : "GPU";
            const usage = kind === 2 ? sample.cpu : sample.gpu;
            if (usage !== null && usage !== undefined && usage >= 0) result.percentage = Math.round(usage);
        } else if (kind === 3 || kind === 4) {
            const first = kind === 3 ? sample.upload : sample.read, second = kind === 3 ? sample.download : sample.write;
            if (first !== null && second !== null && first >= 0 && second >= 0) {
                const unit = Math.max(first, second) >= 1048576 ? 1048576 : 1024;
                result.current = (first / unit).toFixed(1); result.full = (second / unit).toFixed(1); result.unit = unit === 1024 ? "KB/s" : "MB/s";
            }
            result.trailing = kind === 3 ? "↑/↓" : "R/W";
        }
        if (result.percentage >= 0) { result.progress = result.percentage / 100; result.trailing = result.percentage + "%"; }
        return result;
    }
    BatteryBadge {
        scale: settingsController.batteryScale; transformOrigin: Item.TopLeft
        alwaysExpanded: true; manageMonitoring: false; presented: popup.visible; reduceMotion: settingsController.reduceMotion
        reading: popup.metric()
    }
}
