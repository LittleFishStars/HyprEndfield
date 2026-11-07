// Import the upstream icon/logo roster and translation catalog without redrawing assets.
import fs from 'node:fs';
import path from 'node:path';
const [source, destination] = process.argv.slice(2);
if (!source || !destination) throw new Error('Usage: node tools/import-settings-assets.mjs EndfieldHUD HyprEndfield');
const target = path.join(destination, 'assets/settings');
fs.mkdirSync(target, { recursive: true });
for (const name of ['application-icons', 'center-logos'])
    fs.cpSync(path.join(source, 'windows/resources', name), path.join(target, name), { recursive: true });
fs.copyFileSync(path.join(source, 'windows/core/localization_catalog.inc'), path.join(destination, 'src/localization_catalog.inc'));
fs.copyFileSync(path.join(source, 'LICENSE'), path.join(target, 'EndfieldHUD-LICENSE'));
fs.copyFileSync(path.join(source, 'Resources/EndfieldIndustriesSource.png'), path.join(target, 'center-logos/endfield.png'));
