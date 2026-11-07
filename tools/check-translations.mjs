import fs from 'node:fs';
import path from 'node:path';
const root = path.resolve(import.meta.dirname, '..');
const settings = fs.readFileSync(path.join(root, 'src/settingscontroller.cpp'), 'utf8');
const entries = settings.match(/static constexpr Entry entries\[\] = \{([\s\S]*?)\n    \};/)[1];
const catalog = entries + [...entries.matchAll(/#include "([^"]+\.inc)"/g)]
  .map(match => fs.readFileSync(path.join(root, 'src', match[1]), 'utf8')).join('\n');
const missing = new Map();
for (const dir of ['src', 'qml']) for (const name of fs.readdirSync(path.join(root, dir))) {
  if (!/\.(cpp|qml|js)$/.test(name)) continue;
  const source = fs.readFileSync(path.join(root, dir, name), 'utf8');
  const pattern = /(?:qsTr|\btr)\("((?:[^"\\]|\\.)*)"|QCoreApplication::translate\("[^"\\]*",\s*"((?:[^"\\]|\\.)*)"/g;
  for (const match of source.matchAll(pattern)) {
    const text = match[1] ?? match[2];
    if (/\p{Script=Han}/u.test(text) && !catalog.includes(`"${text}"`)) missing.set(text, `${dir}/${name}`);
  }
}
for (const [text, file] of missing) console.log(`${file}\t${text}`);
console.log(`${missing.size} untranslated strings`);
process.exitCode = missing.size ? 1 : 0;
