// Only pages with an existing MOD header participate. Playback and focus keep
// their original headerless layouts; SYS labels use their separate numbering.
var headerModules = ["notes", "fileShelf", "clipboard", "archive", "volume", "reader",
    "calendar", "projection", "mediaAssembly", "minigame", "eventLog", "account"];

function code(key) {
    const settings = typeof settingsController === "undefined" ? null : settingsController;
    const order = settings ? settings.moduleOrder : headerModules;
    const hidden = settings ? settings.hiddenModules : [];
    const modules = order.filter(module => headerModules.includes(module));
    const visible = modules.filter(module => !hidden.includes(module));
    // Shortcuts follow the right-hand menu; the profile has its own entry.
    // Hidden pages go last, since the profile can still open a hidden account page.
    const numbered = visible.concat(["addApp", "profile"], modules.filter(module => hidden.includes(module)));
    const index = numbered.indexOf(key);
    return index < 0 ? "" : "MOD-" + String(index + 1).padStart(2, "0");
}
