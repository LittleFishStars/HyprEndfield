// Import the measured geometry from endfield_menu, without modifying that repository.
// Game artwork remains the property of Hypergryph / its respective rights holders.
// Requires Node.js and rsvg-convert. Usage: node tools/import-menu-art.mjs /path/to/endfield_menu
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import vm from 'node:vm';
import { execFileSync } from 'node:child_process';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
if (!process.argv[2]) throw new Error('Usage: node tools/import-menu-art.mjs /path/to/endfield_menu');
const reference = resolve(process.argv[2]);
const out = resolve(root, 'assets/menu');
const rasterAssets = [];
mkdirSync(out, { recursive: true });
const EF = { addStyle() {} };
const context = vm.createContext({ EF, window: { EF }, document: { body: { insertAdjacentHTML() {} } } });
function source(name, exports = '') {
    let code = readFileSync(resolve(reference, 'components', name + '.js'), 'utf8');
    if (exports) code = code.replace(/\}\)\(\);\s*$/, `EF.port = {${exports}}; })();`);
    vm.runInContext(code, context, { filename: name });
    return EF.port;
}
source('shared-geometry');
source('card-contours');
const cards = source('left-cards', 'CARDS, CARD_SHAPES, ANGLES, R_CARD, cardPathD, platePathD, contoursMarkup, linksMarkup, cardCentre');
const rings = source('dial-rings', 'RINGS, FLAG_ANGLES, FLAG_R');
const backdrop = source('backdrop', 'FLOOR, ARC_Z, MARKS, RING');
const bottom = source('disc-buttons', 'BUTTONS, DCBTN, buttonsSVG, backPlate, buttonFace');
const art = source('button-art', 'ART');

// Qt SVG expects separate alpha attributes; rgba() is CSS in the upstream source.
function xmlColors(xml) {
    return xml.replace(/(fill|stroke|stop-color)="rgba\(([^)]+)\)"/g, (_, attr, value) => {
        const [r, g, b, a] = value.split(',').map(Number);
        return `${attr}="rgb(${r},${g},${b})" ${attr === 'stop-color' ? 'stop' : attr}-opacity="${a}"`;
    });
}
function svg(name, box, body) {
    const [x, y, w, h] = box;
    const file = resolve(out, name + '.svg');
    writeFileSync(file, xmlColors(`<!-- Geometry imported from endfield_menu/components. -->\n<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="${x} ${y} ${w} ${h}">${body}</svg>\n`));
    // Qt SVG Tiny silently ignores clipPath. librsvg implements the authored
    // clips, even-odd fills and gradients. Rasterize ONLY those surfaces at 4x;
    // labels, glyphs, outlines and the other scene planes remain native vectors.
    if (body.includes('<clipPath')) {
        execFileSync('rsvg-convert', ['--zoom', '4', file, '--output', resolve(out, name + '.png')]);
        rasterAssets.push(name);
    }
}
const metadata = { cards: [], rings: [], flags: rings.FLAG_ANGLES, flagRadius: rings.FLAG_R,
    floorHeight: backdrop.ARC_Z, marks: backdrop.MARKS, decorRing: backdrop.RING, bottom: bottom.BUTTONS };
const links = cards.linksMarkup();
const linksDefs = links.match(/<defs>[\s\S]*?<\/defs>/)[0];
const linkGroups = [...links.matchAll(/<g class="ef-links__b"[\s\S]*?<\/g>/g)].map(m => m[0]);
for (let i = 0; i < 4; i++) {
    const face = cards.cardPathD(i, 0), ring = cards.cardPathD(i, 4);
    const defs = `<defs><clipPath id="clip"><path d="${face}"/></clipPath><linearGradient id="face" gradientUnits="userSpaceOnUse" x1="67" y1="239" x2="373" y2="-67"><stop offset="0" stop-color="rgba(222,220,215,.86)"/><stop offset=".45" stop-color="rgba(189,187,182,.82)"/><stop offset="1" stop-color="rgba(144,142,137,.78)"/></linearGradient></defs>`;
    svg(`card-${i}-face`, [0, 0, 440, 172], defs + `<path d="${face}" fill="url(#face)"/><g clip-path="url(#clip)"><path d="${cards.platePathD(i)}" fill="rgba(255,253,247,.31)" fill-rule="evenodd"/><g fill="none" stroke="#23231f" stroke-linecap="round" stroke-linejoin="round">${cards.contoursMarkup(i)}</g></g><path d="${ring}" fill="none" stroke="rgba(242,242,236,.72)" stroke-width="2"/>`);
    svg(`card-${i}-back`, [0, 0, 440, 172], `<path d="${ring}" fill="none" stroke="rgba(255,255,250,.33)" stroke-width="2.5"/><path d="${face}" fill="rgba(255,255,250,.27)"/>`);
    svg(`card-${i}-hover`, [0, 0, 440, 172], `<path d="${face}" fill="#f4f4f2" fill-opacity=".82"/>`);
    svg(`card-${i}-links`, [0, 0, 1920, 1080], linksDefs + linkGroups[i]);
    const a = cards.ANGLES[i] * Math.PI / 180;
    metadata.cards.push({ x: +(960 - cards.R_CARD * Math.cos(a) - 350).toFixed(1),
        y: +(540 - cards.R_CARD * Math.sin(a) - 86).toFixed(1),
        shadow: cards.CARD_SHAPES[i].shadow, centre: cards.cardCentre(i), path: face,
        text: cards.CARDS[i].text });
}
for (const ring of rings.RINGS) {
    // Padding keeps the large, recessed annulus outside the SVG viewport unclipped.
    svg(ring.id, [-180, -180, 1360, 1360], ring.draw());
    metadata.rings.push({ id: ring.id, height: ring.h });
}
svg('floor', [0, 0, 1920, 1080], '<g fill="none" stroke="#fff">' + backdrop.FLOOR.map(l => `<path d="${l.d}" stroke-width="${l.w}" stroke-opacity="${l.a}"/>`).join('') + '</g>');
svg('flag', [-32, -64, 64, 64], '<path d="M0 0 L22 -48 L-22 -48 Z" fill="none" stroke="#FFDE00" stroke-width="2.55" stroke-linejoin="round"/>');

// Expand the SVG pattern to the same strokes, so Qt's vector renderer keeps it vector.
let grid = '';
for (let x = 640.25; x < 1300; x += 2.48) grid += `M${x.toFixed(2)} 840V990`;
for (let y = 840.25; y < 990; y += 2.48) grid += `M640 ${y.toFixed(2)}H1300`;
let buttonDefs = bottom.buttonsSVG().match(/<defs>[\s\S]*?<\/defs>/)[0].replace(/<pattern[\s\S]*?<\/pattern>/, '');
function platePosition(body) {
    return body.replace(/style="--ef-shx0: ([\d.-]+); --ef-shy0: ([\d.-]+)"/g, 'transform="translate($1 $2)"');
}
for (let i = 0; i < 2; i++) {
    const b = bottom.BUTTONS[i];
    let face = bottom.buttonFace(b, i).replace(/<rect[^>]*fill="url\(#dcGrid\)"[^>]*><\/rect>/,
        `<path d="${grid}" stroke="#1a1707" stroke-width=".5" opacity=".035" fill="none"/>`);
    const turn = `rotate(${bottom.DCBTN.rot} ${bottom.DCBTN.cx} ${bottom.DCBTN.cy})`;
    svg(`bottom-${i}-face`, [640, 830, 660, 160], buttonDefs + `<g transform="${turn}">${platePosition(face)}</g>`);
    svg(`bottom-${i}-decal`, [640, 830, 660, 160], `<g fill="#322c07">${art.ART[b.decal.replace('art-', '')]}</g>`);
    svg(`bottom-${i}-icon`, [640, 830, 660, 160], `<g fill="#fff">${art.ART[b.art.replace('art-', '')]}</g>`);
    svg(`bottom-${i}-back`, [640, 830, 660, 160], `<g transform="${turn}">${platePosition(bottom.backPlate(b))}</g>`);
}
// The map well's artwork follows disc.js; the desktop settings remain native items above it.
let gridLines = '', dots = '', scans = '';
for (let p = -10; p <= 894; p += 116) gridLines += `M${p} 0V894M0 ${p}H894`;
for (let y = 1.5; y < 894; y += 56) for (let x = 1.5; x < 894; x += 56) dots += `<circle cx="${x}" cy="${y}" r="1.2"/>`;
for (let y = 0; y < 894; y += 3) scans += `M0 ${y}H894`;
svg('well', [0, 0, 894, 894], `<defs><clipPath id="well"><circle cx="447" cy="447" r="447"/></clipPath><radialGradient id="vignette"><stop offset=".82" stop-color="#0f1113" stop-opacity="0"/><stop offset="1" stop-color="#0f1113" stop-opacity=".6"/></radialGradient></defs><circle cx="447" cy="447" r="447" fill="#090a0c"/><g clip-path="url(#well)"><path d="${gridLines}" stroke="#96d2ff" stroke-opacity=".05" fill="none"/><g fill="#fff" fill-opacity=".035">${dots}</g><path d="${scans}" stroke="#fff" stroke-opacity=".025" fill="none"/><circle cx="447" cy="447" r="447" fill="url(#vignette)"/></g>`);
// Right-tile surface, including the measured 3.3 px scanline spacing.
let scan = '';
for (let y = 104; y >= 0; y -= 3.3) scan += `<rect x="0" y="${(y - 1).toFixed(2)}" width="107" height="1"/>`;
svg('tile-face', [0, 0, 107, 104], `<defs><clipPath id="tile"><rect width="107" height="104" rx="8"/></clipPath><linearGradient id="face" x1="0" y1="1" x2="1" y2="0"><stop offset="0" stop-color="rgba(255,254,252,.94)"/><stop offset=".45" stop-color="rgba(251,250,247,.84)"/><stop offset="1" stop-color="rgba(238,237,233,.74)"/></linearGradient></defs><rect width="107" height="104" rx="8" fill="url(#face)"/><g clip-path="url(#tile)" fill="#000" fill-opacity=".026">${scan}</g>`);

// player-badge.js: keep the authored plate, texture and avatar brackets.
let badgeTexture = '', avatarTexture = '', avatarScan = '';
for (let x = -120; x < 520; x += 9 / Math.sin(56*Math.PI/180))
    badgeTexture += `M${x.toFixed(3)} 0l${(-116/Math.tan(56*Math.PI/180)).toFixed(3)} 116`;
for (let y = 0; y <= 116; y += 14) badgeTexture += `M0 ${y}H389`;
for (let x = -84; x <= 168; x += 14*Math.SQRT2) avatarTexture += `M${x} 0l84 84`;
for (let y = 14; y < 70; y += 11) avatarScan += `<path d="M6 ${y}h8" stroke="#8cebc3" stroke-opacity=".39" stroke-width="2"/><path d="M6 ${y+5}h8" stroke="#8cebc3" stroke-opacity=".2275"/>`;
const skyline = '0,78 6,78 6,44 12,44 12,66 20,66 20,22 24,22 24,10 27,10 27,52 35,52 35,34 42,34 42,70 50,70 50,18 55,18 55,48 63,48 63,30 70,30 70,60 78,60 78,8 82,8 82,40 90,40 90,56 100,56 100,100 0,100';
svg('badge', [0,0,389,116], `<defs>
<clipPath id="plate"><rect width="389" height="116" rx="20"/></clipPath>
<clipPath id="avatar"><rect width="84" height="84"/></clipPath>
<linearGradient id="body" gradientUnits="userSpaceOnUse" x1="68.25" y1="-68.25" x2="320.75" y2="184.25"><stop stop-color="rgba(56,60,66,.96)"/><stop offset=".48" stop-color="rgba(28,31,36,.96)"/><stop offset="1" stop-color="rgba(13,15,18,.98)"/></linearGradient>
<linearGradient id="rim"><stop offset=".4" stop-color="#d5b983" stop-opacity="0"/><stop offset=".7" stop-color="#d5b983" stop-opacity=".5"/><stop offset=".96" stop-color="#d5b983"/></linearGradient>
<linearGradient id="texture"><stop offset=".26" stop-color="#fff" stop-opacity="0"/><stop offset=".58" stop-color="#fff" stop-opacity=".024"/></linearGradient>
<linearGradient id="sky"><stop offset=".3" stop-color="#fff" stop-opacity="0"/><stop offset=".6" stop-color="#fff" stop-opacity=".045"/></linearGradient>
<linearGradient id="streak"><stop stop-color="#d5b983" stop-opacity="0"/><stop offset=".55" stop-color="#d5b983" stop-opacity=".08"/><stop offset="1" stop-color="#d5b983" stop-opacity=".16"/></linearGradient>
</defs><g clip-path="url(#plate)"><rect width="389" height="116" rx="20" fill="url(#body)"/><path d="${badgeTexture}" stroke="url(#texture)" stroke-width="2" fill="none"/><g transform="translate(0 44.08) scale(3.89 .7192)"><polygon points="${skyline}" fill="url(#sky)"/></g><rect x="269" width="120" height="116" fill="url(#streak)"/><rect x=".5" y=".5" width="388" height="115" rx="20" stroke="#fff" stroke-opacity=".05" fill="none"/></g><rect x="1.25" y="1.25" width="386.5" height="113.5" rx="18.75" fill="none" stroke="url(#rim)" stroke-width="2.5"/>
<g transform="translate(16 16)"><g clip-path="url(#avatar)"><rect width="84" height="84" fill="#fff" fill-opacity=".05"/><path d="${avatarTexture}" stroke="#fff" stroke-opacity=".08" stroke-width="7" fill="none"/></g><rect width="84" height="84" fill="none" stroke="#fff" stroke-opacity=".25" stroke-dasharray="3 3"/><path d="M-5 21V-5H21M73 89H89V73" fill="none" stroke="#8cebc3" stroke-opacity=".85" stroke-width="2"/><rect x="74" y="74" width="6" height="6" fill="#8cebc3" fill-opacity=".95"/>${avatarScan}</g><rect x="120" y="89" width="140" height="4" rx="2" fill="#d5b983"/>`);
// hud.js power plate; both mouse buttons deliberately remain non-dismissive.
svg('power-plate',[0,0,53,53],`<defs><clipPath id="p"><rect width="53" height="53" rx="4"/></clipPath><linearGradient id="stripe"><stop offset="0" stop-color="#c34142" stop-opacity="0"/><stop offset="${1.25/4.542}" stop-color="#c34142" stop-opacity="0"/><stop offset="${2.25/4.542}" stop-color="#c34142"/><stop offset="${3.54/4.542}" stop-color="#c34142"/><stop offset="1" stop-color="#c34142" stop-opacity="0"/></linearGradient><pattern id="stripes" width="4.542" height="4.542" x=".3" patternUnits="userSpaceOnUse" patternTransform="rotate(44.5)"><rect width="4.542" height="4.542" fill="url(#stripe)"/></pattern></defs><g clip-path="url(#p)"><rect width="53" height="53" fill="#080606" fill-opacity=".13"/><rect width="53" height="53" fill="url(#stripes)"/></g>`);
metadata.rasterAssets = rasterAssets;
writeFileSync(resolve(root, 'qml/MenuGeometry.js'), '// Generated from endfield_menu by tools/import-menu-art.mjs.\n.pragma library\nvar data = ' + JSON.stringify(metadata, null, 2) + ';\n');
console.log(`Imported ${metadata.cards.length} cards, ${metadata.rings.length} ring planes and scene geometry into ${out}`);
