// EndfieldHUD e9a6bdc2: the BP13 prefab, full sprite rectangles and original parents.
import {readFileSync, writeFileSync, mkdirSync, copyFileSync} from 'node:fs';
import {resolve, dirname} from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
if (!process.argv[2]) throw new Error('Usage: node tools/import-profile-card.mjs /path/to/EndfieldHUD');
const upstream = resolve(process.argv[2]);
const base = resolve(upstream, 'Resources/WatchSource');
const source = JSON.parse(readFileSync(resolve(base, 'Scene/desktop-profile-card.json')));
const scene = JSON.parse(readFileSync(resolve(base, 'Scene/scene.json')));
const parent = scene.nodes.find(n => n.id === source.parent_id);
if (!parent) throw new Error('Missing original card mount');
const out = resolve(root, 'assets/profile-card');
mkdirSync(out, {recursive:true});
const byID = new Map(source.scene.nodes.map(n => [n.id, n]));
function active(n) { return n.game_object.data.m_IsActive && (!byID.has(n.parent_id) || active(byID.get(n.parent_id))); }
function size(n) {
    const r = n.transform.raw, p = byID.has(n.parent_id) ? size(byID.get(n.parent_id)) : [0, 0];
    return [r.m_SizeDelta.x + (r.m_AnchorMax.x - r.m_AnchorMin.x) * p[0], r.m_SizeDelta.y + (r.m_AnchorMax.y - r.m_AnchorMin.y) * p[1]];
}
const exported = new Set();
function asset(sprite) {
    const name = sprite.name + '.png';
    if (!exported.has(name)) {
        const texture = source.sprites.source_textures.find(t => t.id === sprite.texture.id);
        const raw = sprite.raw_sprite.m_Rect, render = sprite.effective_render_data;
        if (render.settingsRaw !== 64) throw new Error('Packed sprite needs original mesh: ' + name);
        const x = render.textureRect.x - render.textureRectOffset.x;
        const y = texture.height - (render.textureRect.y - render.textureRectOffset.y) - raw.height;
        execFileSync('magick', [resolve(base, 'Scene', texture.png.file), '-crop', `${raw.width}x${raw.height}+${x}+${y}`, '+repage', resolve(out,name)]);
        exported.add(name);
    }
    return name;
}
const surfaces = [];
for (const n of source.scene.nodes) {
    if (!active(n)) continue;
    const image = n.components.find(c => c.script === 'UIImage')?.data;
    const text = n.components.find(c => c.script === 'UIText')?.data;
    if (!image && !text) continue;
    const [width,height] = size(n), binding = Object.entries(source.bindings).find(([,v]) => v.target_node_id === n.id)?.[0] || '';
    const s = {id:n.id, name:n.name, path:n.path, width,height,binding};
    if (text) { s.fontSize=text.m_fontSize; surfaces.push(s); continue; }
    let sprite = source.sprites.sprites.find(s => s.id === image.m_Sprite?.target_id);
    if (n.name === 'BgImage') sprite = source.sprites.sprites.find(s => s.name === 'business_card_topic_normal_1');
    s.color = [image.m_Color.r,image.m_Color.g,image.m_Color.b,image.m_Color.a];
    if (sprite) {
        s.asset=asset(sprite);
        if (image.m_Type === 1) { const b=sprite.raw_sprite.m_Border; s.border=[b.x,b.w,b.z,b.y]; }
    }
    s.dynamic = n.name === 'BgImage' ? 'card' : binding === 'playerHead' ? 'avatar' : binding === 'headFrameImg' ? 'frame' : n.name === 'Light' ? (n.parent_id === source.scene.root_node_id ? 'hover' : 'disabledGlow') : '';
    s.accent = ['levelSlider','headFrameImg'].includes(binding) || ['IconRight','ArrowImage'].includes(n.name);
    surfaces.push(s);
}
const nodes = source.scene.nodes.map(n => ({id:n.id,name:n.name,parent_id:n.id===source.scene.root_node_id ? source.parent_id : n.parent_id,path:parent.path+'/'+n.path,transform:n.transform}));
writeFileSync(resolve(out,'scene.json'),JSON.stringify({source:'EndfieldHUD e9a6bdc2',nodes}));
writeFileSync(resolve(root,'qml/ProfileCardSource.js'),'// Generated from the original BP13 prefab by tools/import-profile-card.mjs.\n.pragma library\nvar data = '+JSON.stringify({surfaces,root:{id:source.scene.root_node_id,width:364,height:128}},null,2)+';\n');
copyFileSync(resolve(upstream,'windows/resources/profile/business-card.bgra'),resolve(out,'business-card.bgra'));
const gauge = resolve(root,'assets/account'); mkdirSync(gauge,{recursive:true});
for (const name of ['account-numerals.json','wallet-back.rgba','wallet-deco.rgba','item-ap.rgba','wallet-silhouette.rgba'])
    copyFileSync(resolve(upstream,'windows/resources/account',name),resolve(gauge,name));
console.log(`Imported ${nodes.length} source transforms, ${surfaces.length} surfaces and ${exported.size} full sprites.`);
