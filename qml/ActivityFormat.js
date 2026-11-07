.pragma library

// EndfieldHUD TelemetryFormat and ActivityGraph.paths, retaining decimal units,
// 44 samples, right-aligned history, and breaks where readings are unavailable.
function valid(value) { return value !== null && value !== undefined && Number.isFinite(Number(value)) && Number(value) >= 0; }
function bytes(value) {
    if (!valid(value)) return "—";
    const units = ["B", "KB", "MB", "GB", "TB", "PB"];
    let unit = 0, number = Number(value);
    while (number >= 1000 && unit + 1 < units.length) { number /= 1000; ++unit; }
    return number.toFixed(number >= 100 || unit === 0 ? 0 : 1) + " " + units[unit];
}
function rate(value) { const text = bytes(value); return text === "—" ? text : text + "/s"; }
function percent(value, clamp) { return valid(value) ? (clamp ? Math.min(100, Number(value)) : Number(value)).toFixed(1) + "%" : "—"; }
function memory(value) { return !valid(value) ? "—" : value >= 1e9 ? (value / 1e9).toFixed(1) + " GB" : (value / 1e6).toFixed(0) + " MB"; }
function paths(values) {
    const window = values.slice(-44), offset = 44 - window.length;
    let area = "", line = "", run = [];
    function flush() {
        if (run.length > 1) {
            line += "M" + run.map(point => point.join(" ")).join("L");
            area += "M" + run[0][0] + " 44L" + run.map(point => point.join(" ")).join("L") + "L" + run[run.length - 1][0] + " 44Z";
        }
        run = [];
    }
    window.forEach((value, index) => {
        if (value === null || value === undefined || !Number.isFinite(Number(value))) { flush(); return; }
        run.push([296 * (offset + index) / 43, 44 - Math.min(100, Math.max(0, value)) * 0.42]);
    });
    flush(); return { area: area, line: line };
}
function series(history, metric) {
    const recent = history.slice(-44);
    if (metric < 2) return [recent.map(sample => sample[metric === 0 ? "gpu" : "cpu"])];
    if (metric === 2) return [recent.map(sample => valid(sample.memoryUsed) && valid(sample.memoryTotal) && sample.memoryTotal > 0 ? sample.memoryUsed / sample.memoryTotal * 100 : null)];
    const keys = metric === 3 ? ["upload", "download"] : ["write", "read"];
    const values = keys.map(key => recent.map(sample => sample[key]));
    let peak = 0;
    values.forEach(series => series.forEach(value => { if (valid(value)) peak = Math.max(peak, value); }));
    const ceiling = Math.max(1000, peak * 1.12);
    return values.map(series => series.map(value => valid(value) ? value / ceiling * 100 : null));
}
function readout(latest, metric) {
    if (metric < 2) return [percent(latest[metric === 0 ? "gpu" : "cpu"], true), "0–100%"];
    if (metric === 2) return [bytes(latest.memoryUsed), "/ " + bytes(latest.memoryTotal)];
    if (metric === 3) return ["↑ " + rate(latest.upload), "↓ " + rate(latest.download)];
    return ["读 " + rate(latest.read), "写 " + rate(latest.write)];
}
