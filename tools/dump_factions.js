// READ-ONLY: dump every FACT (faction) record of the vanilla masters with its relations, as JSON.
//   node tools/dump_factions.js [outDir]       (run from the game folder so xeditlib resolves)
const fs = require('fs'), path = require('path');
const GAME = 'C:/Games/steamapps/common/Skyrim Special Edition/';
const xelib = require(GAME + 'node_modules/xeditlib');
const OUT = process.argv[2] || path.join(__dirname, '..', 'research', 'raw');
fs.mkdirSync(OUT, { recursive: true });
xelib.init(); xelib.setLanguage('English'); xelib.setGamePath(GAME); xelib.setGameMode(xelib.GM_SSE);
xelib.loadPlugins(['Skyrim.esm', 'Update.esm', 'Dawnguard.esm', 'HearthFires.esm', 'Dragonborn.esm'].join('\n'), false, false);
xelib.waitForLoader().then(() => {
  const out = {};
  for (const f of ['Skyrim.esm', 'Update.esm', 'Dawnguard.esm', 'Dragonborn.esm']) {
    for (const r of xelib.getRecords(xelib.fileByName(f), 'FACT', false)) {
      const j = JSON.parse(xelib.elementToJson(r));
      out[j['Record Header']['FormID']] = j;
    }
  }
  fs.writeFileSync(path.join(OUT, 'factions.json'), JSON.stringify(out));
  console.log('factions', Object.keys(out).length, 'RESULT: OK');
  process.exit(0);
}).catch(e => { console.log('RESULT: ERROR', e.message); process.exit(1); });
