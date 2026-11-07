// EndfieldHUD's actual MiddleDecoNode, not the HTML reconstruction.
// Image geometry follows HUDSourceImageGeometry.swift (Unity uGUI, MIT).
import {readFileSync, writeFileSync, mkdirSync, copyFileSync} from 'node:fs';
import {resolve, dirname, basename} from 'node:path';
import {fileURLToPath} from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
if (!process.argv[2]) throw new Error('Usage: node tools/import-watch-dial.mjs /path/to/EndfieldHUD');
const source = resolve(process.argv[2], 'Resources/WatchSource');
const read = name => JSON.parse(readFileSync(resolve(source, name)));
const scene = read('Scene/scene.json'), sprites = read('Scene/sprites.json');
const textures = read('textures.json'), materials = read('materials.json');
const fields = read('fx-shader.json').constant_buffers.find(c => c.name === 'UnityPerMaterial').fields;
const out = resolve(root, 'assets/watch-dial');
mkdirSync(out, {recursive:true});
const textureData = {}, materialData = {}, meshData = {};
const base = 'WatchPanel_PC/Content/WatchNode/canvas_watch/MiddleDecoNode/';
const linear = c => c <= .04045 ? c / 12.92 : Math.pow((c + .055) / 1.055, 2.4);
function texture(id) {
    if (textureData[id]) return id;
    const t = textures.find(t => t.texture_id === id || t.path_id === id || `${t.cab}:${t.path_id}` === id);
    if (!t || ![4,25].includes(t.texture_format)) throw new Error('Missing original texture: ' + id);
    const file = t.texture_format === 25 ? t.decoded_mips_file : t.data_file;
    copyFileSync(resolve(source, file), resolve(out, basename(file)));
    textureData[id] = {file:basename(file), width:t.width, height:t.height,
        mipCount:t.mip_count, bgra:t.texture_format === 25, srgb:t.color_space === 0, sampler:t.sampler};
    return id;
}
function material(id) {
    if (materialData[id]) return id;
    const m = materials.find(m => m.id === id);
    if (!m) throw new Error('Missing original material: ' + id);
    const raw = m.serialized.m_SavedProperties;
    const values = Object.fromEntries(raw.m_Floats.map(([k,v]) => [k,[v]]));
    for (const [k,v] of raw.m_Colors) values[k] = [v.r,v.g,v.b,v.a];
    for (const p of m.shader_properties) {
        if (!values[p.name] || (p.type !== 0 && !(p.flags & 32))) continue;
        const count = p.type === 2 || p.type === 3 ? 1 : 3;
        values[p.name] = values[p.name].map((v,i) => i < count ? linear(v) : v);
    }
    const slots = {};
    for (const [name,v] of raw.m_TexEnvs) {
        values[name + '_ST'] = [v.m_Scale.x,v.m_Scale.y,v.m_Offset.x,v.m_Offset.y];
    }
    for (const binding of m.texture_bindings) {
        if (['_MainTex','_MaskTex','_DissolveTex'].includes(binding.slot) && binding.texture)
            slots[binding.slot] = texture(`${binding.texture.cab}:${binding.texture.path_id}`);
    }
    const fx = m.shader.path_id === '-7864008769510089003';
    if(fx && (!slots._MainTex || !slots._MaskTex)) throw new Error('Missing FX texture bindings: '+m.name);
    const uniforms = Array(80).fill(0);
    if (fx) for (const f of fields) (values[f.name] || []).forEach((v,i) => uniforms[f.offset / 4 + i] = v);
    const passes = m.static_pass_states.filter(p => !p.disabled_in_serialized_material
        && ['Default','Default-Stencil-Alpha-Blend'].includes(p.name)).map(p => ({
            stencilAlpha:p.name === 'Default-Stencil-Alpha-Blend' ? values._StencilAlpha[0] : 1,
            blend:Object.fromEntries(Object.entries(p.state.rtBlend0).map(([k,v]) => [k,v.value])),
            stencil:Object.fromEntries(Object.entries(p.state.stencilOp).map(([k,v]) => [k,v.value])),
            reference:p.state.stencilRef.value, readMask:p.state.stencilReadMask.value,
            writeMask:p.state.stencilWriteMask.value
        }));
    materialData[id] = {name:m.name, fx, uniforms, textures:slots, passes};
    return id;
}
const lerp = (a,b,t) => a + (b-a)*t;
const points = (lo,hi) => [lo,[lo[0],hi[1]],hi,[hi[0],lo[1]]];
const sub = (q,lo,hi) => points(lo.map((v,i)=>lerp(q[0][i],q[2][i],v)),hi.map((v,i)=>lerp(q[0][i],q[2][i],v)));
function cut(input, cosine, sine, invert, corner) {
    const q = input.map(p=>p.slice()), i0=corner,i1=(corner+1)%4,i2=(corner+2)%4,i3=(corner+3)%4;
    if (corner & 1) {
        if (sine > cosine) { cosine/=sine; sine=1; if(invert) q[i2][0]=q[i1][0]=lerp(q[i0][0],q[i2][0],cosine); }
        else if(cosine>sine) { sine/=cosine; cosine=1; if(!invert) q[i3][1]=q[i2][1]=lerp(q[i0][1],q[i2][1],sine); }
        else cosine=sine=1;
        if(!invert) q[i3][0]=lerp(q[i0][0],q[i2][0],cosine);
        else q[i1][1]=lerp(q[i0][1],q[i2][1],sine);
    } else {
        if(cosine>sine) { sine/=cosine; cosine=1; if(!invert) q[i2][1]=q[i1][1]=lerp(q[i0][1],q[i2][1],sine); }
        else if(sine>cosine) { cosine/=sine; sine=1; if(invert) q[i3][0]=q[i2][0]=lerp(q[i0][0],q[i2][0],cosine); }
        else cosine=sine=1;
        if(invert) q[i3][1]=lerp(q[i0][1],q[i2][1],sine);
        else q[i1][0]=lerp(q[i0][0],q[i2][0],cosine);
    }
    return q;
}
function imageGeometry(node,img,sprite) {
    const t=node.transform.raw, w=t.m_SizeDelta.x,h=t.m_SizeDelta.y;
    const origin=[-w*t.m_Pivot.x,-h*t.m_Pivot.y];
    const raw=sprite.raw_sprite,rd=sprite.effective_render_data,r=rd.textureRect,o=rd.textureRectOffset;
    const sw=raw.m_Rect.width,sh=raw.m_Rect.height;
    const tex=sprites.source_textures.find(t=>t.id===sprite.texture.id);
    if (sprite.packing.packing_rotation !== 'kSPRNone' || img.m_PreserveAspect || img.m_UseSpriteMesh)
        throw new Error('Unsupported source image: '+node.path);
    const pad=[o.x,o.y,sw-o.x-r.width,sh-o.y-r.height], b=raw.m_Border;
    if ([b.x,b.y,b.z,b.w].some(v=>v!==0)) throw new Error('Unexpected sliced dial image');
    const xy=points([origin[0]+w*pad[0]/sw,origin[1]+h*pad[1]/sh],
        [origin[0]+w*(sw-pad[2])/sw,origin[1]+h*(sh-pad[3])/sh]);
    const uv=points([r.x/tex.width,r.y/tex.height],[(r.x+r.width)/tex.width,(r.y+r.height)/tex.height]);
    const vertices=[],indices=[];
    const quad=(q,u)=>{
        const start=vertices.length;
        q.forEach((p,i)=>vertices.push([p[0],p[1],0,u[i][0],u[i][1]]));
        indices.push(start,start+1,start+2,start+2,start+3,start);
    };
    const radial=(q,u,amount,invert,corner)=>{
        if(amount<.001) return;
        if(corner&1) invert=!invert;
        if(!invert&&amount>.999) {quad(q,u);return;}
        const a=(invert?1-amount:amount)*Math.PI/2;
        quad(cut(q,Math.cos(a),Math.sin(a),invert,corner),cut(u,Math.cos(a),Math.sin(a),invert,corner));
    };
    if(img.m_Type===2) {
        const ppu=raw.m_PixelsToUnits/100*img.m_PixelsPerUnitMultiplier;
        const sx=sw/ppu,sy=sh/ppu;
        // With zero borders Unity's centre tiles sample the full sprite rect.
        const iu=points([(r.x-pad[0])/tex.width,(r.y-pad[1])/tex.height],
            [(r.x+r.width+pad[2])/tex.width,(r.y+r.height+pad[3])/tex.height]);
        for(let y=0;y<h;y+=sy) for(let x=0;x<w;x+=sx) {
            const dx=Math.min(sx,w-x),dy=Math.min(sy,h-y);
            quad(points([origin[0]+x,origin[1]+y],[origin[0]+x+dx,origin[1]+y+dy]),sub(iu,[0,0],[dx/sx,dy/sy]));
        }
    } else if(img.m_Type!==3 || img.m_FillAmount>=1) quad(xy,uv);
    else if(img.m_FillMethod===0 || img.m_FillMethod===1) {
        const low=[0,0],high=[1,1],axis=img.m_FillMethod,amount=img.m_FillAmount;
        low[axis]=img.m_FillOrigin===1?1-amount:0;high[axis]=img.m_FillOrigin===1?1:amount;
        quad(sub(xy,low,high),sub(uv,low,high));
    } else if(img.m_FillMethod===4) {
        for(let corner=0;corner<4;corner++) {
            const lo=[corner<2?0:.5,corner===0||corner===3?0:.5],hi=lo.map(v=>v+.5);
            const index=(corner+img.m_FillOrigin)%4;
            const amount=Math.min(1,Math.max(0,img.m_FillAmount*4-(img.m_FillClockwise?index:3-index)));
            radial(sub(xy,lo,hi),sub(uv,lo,hi),amount,!!img.m_FillClockwise,(corner+2)%4);
        }
    } else throw new Error('Unsupported dial fill: '+node.path);
    return {vertices,indices};
}
const draws=[];
for(const n of scene.nodes.filter(n=>n.path.startsWith(base))) {
    const img=n.components.find(c=>c.script==='UIImage')?.data;
    const filter=n.components.find(c=>c.type==='MeshFilter')?.data;
    if(img) {
        const s=sprites.sprites.find(s=>s.id===img.m_Sprite?.target_id
            || (img.imgRefPath && img.imgRefPath.toLowerCase().endsWith('/'+s.name.toLowerCase()+'.png')));
        if(!s && img.m_Color.a===0) continue;
        if(!s) throw new Error('Missing dial sprite: '+n.path);
        meshData[n.id]=imageGeometry(n,img,s);
        draws.push({id:n.id,path:n.path,mesh:n.id,material:material(img.m_Material.target_id),
            texture:texture(s.texture.id),order:6080,isMesh:false,
            opacity:n.name==='triangle_fx1'?.88:n.name==='EndfieldTextGlow'?.78:1});
    } else if(filter) {
        const name=n.name==='RingFoMesh'?'Equipring':'watchline';
        if(!meshData[name]) {
            const m=read('Meshes/'+name+'.json');
            meshData[name]={vertices:m.positions.map((p,i)=>[...p,...m.uv0[i]]),indices:m.indices};
            if(m.colors.length) throw new Error('Unexpected mesh vertex colours');
        }
        const render=n.components.find(c=>c.type==='MeshRenderer').data;
        draws.push({id:n.id,path:n.path,mesh:name,material:material(render.m_Materials[0].target_id),
            order:render.m_SortingOrder,isMesh:true,opacity:n.name==='RingFoMesh'?.88:1});
    }
}
draws.sort((a,b)=>a.order-b.order);
writeFileSync(resolve(out,'dial.json'),JSON.stringify({draws,meshes:meshData,materials:materialData,textures:textureData}));
console.log(`Imported ${draws.length} original dial layers, ${Object.keys(textureData).length} textures and original mesh/fill geometry.`);
