// HUDDesignTokens.swift: accent colours and neutral theme surfaces.
function accent(value) {
    if (typeof settingsController === "undefined") return value;
    const index = settingsController.accentIndex;
    const custom = settingsController.customAccent;
    // Existing colour pickers compare and cycle uppercase hex strings.
    return settingsController.color(value).toString().toUpperCase();
}
function tone(value) {
    if (typeof settingsController === "undefined") return value;
    const light = settingsController.light;
    return settingsController.tone(value).toString().toUpperCase();
}
function recording() {
    return typeof settingsController !== "undefined" && settingsController.recordingMode;
}
