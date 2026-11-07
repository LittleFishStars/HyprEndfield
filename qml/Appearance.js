// HUDDesignTokens.swift: accent colours and neutral theme surfaces.
function accent(value) {
    if (typeof settingsController === "undefined") return value;
    const index = settingsController.accentIndex;
    const custom = settingsController.customAccent;
    return settingsController.color(value);
}
function tone(value) {
    if (typeof settingsController === "undefined") return value;
    const light = settingsController.light;
    return settingsController.tone(value);
}
function recording() {
    return typeof settingsController !== "undefined" && settingsController.recordingMode;
}
