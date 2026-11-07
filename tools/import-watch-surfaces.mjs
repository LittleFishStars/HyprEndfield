// Extract EndfieldHUD sprites and its desktop icon layout without tracing.
import { readFileSync, writeFileSync, mkdirSync, existsSync, copyFileSync } from 'node:fs';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
if (!process.argv[2]) throw new Error('Usage: node tools/import-watch-surfaces.mjs /path/to/EndfieldHUD');
const reference = resolve(process.argv[2], 'Resources/WatchSource/Scene');
const scene = JSON.parse(readFileSync(resolve(reference, 'scene.json')));
const sprites = JSON.parse(readFileSync(resolve(reference, 'sprites.json')));
const out = resolve(root, 'assets/watch-surfaces');
mkdirSync(out, { recursive: true });
const base = 'WatchPanel_PC/Content/WatchNode/canvas_watch/';
const byPath = path => scene.nodes.find(n => n.path === base + path);
const image = node => node.components.find(c => c.script === 'UIImage')?.data;

function silhouette(sprite, width, height) {
    // HUDSourceWatchView.makeDesktopBottomSilhouettes: boundary of the exported
    // sprite triangles, converted once from bottom-origin UV to top-origin QML.
    const vertices = sprite.decoded_render_mesh.vertices.map(([x, y]) => [
        (x * sprite.raw_sprite.m_PixelsToUnits / sprite.raw_sprite.m_Rect.width + sprite.raw_sprite.m_Pivot.x) * width,
        (1 - y * sprite.raw_sprite.m_PixelsToUnits / sprite.raw_sprite.m_Rect.height - sprite.raw_sprite.m_Pivot.y) * height]);
    const counts = new Map(), directed = [];
    const key = (a, b) => `${Math.min(a,b)}:${Math.max(a,b)}`;
    for (const triangle of sprite.decoded_render_mesh.triangles.flat()) {
        for (let i = 0; i < 3; i++) {
            const edge = [triangle[i], triangle[(i + 1) % 3]];
            counts.set(key(...edge), (counts.get(key(...edge)) || 0) + 1);
            directed.push(edge);
        }
    }
    const edges = directed.filter(e => counts.get(key(...e)) === 1), paths = [];
    while (edges.length) {
        const [first, next] = edges.shift();
        const loop = [first]; let cursor = next;
        while (cursor !== first) {
            loop.push(cursor);
            const index = edges.findIndex(e => e[0] === cursor);
            if (index < 0) throw new Error('Open sprite silhouette: ' + sprite.name);
            cursor = edges.splice(index, 1)[0][1];
        }
        paths.push(loop.map((i, n) => `${n ? 'L' : 'M'}${vertices[i].join(' ')}`).join(' ') + 'Z');
    }
    return paths.join(' ');
}

function surface(node, options = {}) {
    if (!node) throw new Error('Missing source node');
    const raw = image(node), transform = node.transform.raw;
    const result = { id: node.id, path: node.path, width: transform.m_SizeDelta.x,
        height: transform.m_SizeDelta.y, ...options };
    if (!raw || options.glyph) return result;
    const sprite = sprites.sprites.find(s => raw.m_Sprite?.target_id === s.id
        || (raw.imgRefPath && raw.imgRefPath.toLowerCase().endsWith('/' + s.name.toLowerCase() + '.png')));
    if (!sprite) throw new Error('Missing original sprite: ' + node.path);
    const name = sprite.name + '.png', dest = resolve(out, name);
    {
        const exported = sprite.png && resolve(reference, sprite.png.file);
        if (exported && existsSync(exported)) copyFileSync(exported, dest);
        else {
            const texture = sprites.source_textures.find(t => t.id === sprite.texture.id);
            // These are unpacked source textures. Keep the full authored sprite
            // rectangle, including transparent padding (not the trimmed mesh UV).
            const rect = sprite.raw_sprite.m_Rect;
            if (sprite.effective_render_data.settingsRaw !== 64)
                throw new Error('This sprite requires packed-mesh extraction: ' + sprite.name);
            execFileSync('magick', [resolve(reference, texture.png.file), '-crop',
                `${rect.width}x${rect.height}+${rect.x}+${texture.height - rect.y - rect.height}`, '+repage', dest]);
        }
    }
    result.asset = name;
    result.color = [raw.m_Color.r, raw.m_Color.g, raw.m_Color.b, raw.m_Color.a];
    if (options.desktopIcon) {
        // HUDSourceWatchView.desktopReportArtwork + DesktopIconLayout.image:
        // 12px padding, 96px working canvas, alpha > 8, then optical 32px box.
        const padded = execFileSync('magick', [dest, '-bordercolor', 'none', '-border', '12',
            '-resize', '96x96', 'png:-']);
        const bounds = execFileSync('magick', ['png:-', '-alpha', 'extract', '-threshold',
            `${8 / 255 * 100}%`, '-format', '%@', 'info:'], { input: padded }).toString();
        const [, width, height] = bounds.match(/^(\d+)x(\d+)\+/) || [];
        if (!width || !height) throw new Error('Empty desktop icon: ' + sprite.name);
        execFileSync('magick', ['png:-', '-crop', bounds, '+repage', dest], { input: padded });
        const scale = Math.min(28 / Math.max(+width, +height), 23 / Math.sqrt(width * height));
        result.imageFrame = [(32 - width * scale) / 64, (32 - height * scale) / 64,
            width * scale / 32, height * scale / 32];
    }
    if (options.hit) result.outline = silhouette(sprite, result.width, result.height);
    return result;
}

const outer = scene.nodes.filter(n => n.path.startsWith(base + 'MiddleDecoNode/OutRingNode/')
    || /^WatchPanel_PC\/Content\/WatchNode\/canvas_watch\/MiddleDecoNode\/DecoOut\/(Left|Right)$/.test(n.path))
    .map(n => surface(n, { group: n.path.includes('/DecoOut/') ? 'secondary' : 'outer' }));
const bottom = ['TechtreeNode/TechtreeBtn', 'ReportNode/ReportBtn'].map((name, index) => {
    const prefix = 'HudBgShdow/' + name + '/Icon';
    const label = byPath(prefix + '/BtnNameShadow/BtnName');
    const iconNode = scene.nodes.find(n => n.path.startsWith(base + prefix + '/IconShadow/') && n.name.trim() === 'Icon');
    return {
        face: surface(byPath(prefix), { hit: true }),
        light: surface(byPath(prefix + '/Light'), { additive: true }),
        label: surface(label),
        icon: index === 0 ? surface(iconNode, { glyph: 'storage' }) : surface(iconNode, { desktopIcon: true }),
    };
});
const shadow = surface(byPath('HudBgShdow'));
mkdirSync(resolve(root, 'qml'), { recursive: true });
writeFileSync(resolve(root, 'qml/WatchSurfaces.js'), '// Imported from EndfieldHUD by tools/import-watch-surfaces.mjs.\n.pragma library\nvar data = ' + JSON.stringify({ outer, bottom, shadow }, null, 2) + ';\n');
console.log(`Imported ${outer.length} outer surfaces and both original bottom buttons.`);
