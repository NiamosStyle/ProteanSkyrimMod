// READ-ONLY: list location-type keywords (KYWD "LocType*") with their FormIDs, and which LCTN records use them.
const fs = require('fs'), path = require('path');
const GAME = 'C:/Games/steamapps/common/Skyrim Special Edition/';
const xelib = require(GAME + 'node_modules/xeditlib');
const OUT = process.argv[2] || path.join(__dirname, '..', 'research', 'raw');
xelib.init(); xelib.setLanguage('English'); xelib.setGamePath(GAME); xelib.setGameMode(xelib.GM_SSE);
xelib.loadPlugins(['Skyrim.esm', 'Update.esm', 'Dawnguard.esm', 'HearthFires.esm', 'Dragonborn.esm'].join('\n'), false, false);
const g = (h, p) => { try { return xelib.getValue(h, p); } catch { return ''; } };
xelib.waitForLoader().then(() => {
  const kw = {};
  const loc = [];
  for (const f of ['Skyrim.esm', 'Update.esm', 'Dawnguard.esm', 'Dragonborn.esm']) {
    for (const r of xelib.getRecords(xelib.fileByName(f), 'KYWD', false)) {
      const e = g(r, 'EDID');
      if (/^Loc(Type|Ref)/.test(e)) kw[f + ':' + xelib.getFormID(r, false).toString(16).toUpperCase().padStart(8, '0').slice(2)] = e;
    }
    for (const r of xelib.getRecords(xelib.fileByName(f), 'LCTN', false)) {
      const j = JSON.parse(xelib.elementToJson(r));
      loc.push({ file: f, edid: j['EDID - Editor ID'], name: j['FULL - Name'], kw: j['KWDA - Keywords'] || [] });
    }
  }
  fs.writeFileSync(path.join(OUT, 'loc_keywords.json'), JSON.stringify({ kw, loc }));
  console.log('keywords', Object.keys(kw).length, 'locations', loc.length, 'RESULT: OK');
  process.exit(0);
}).catch(e => { console.log('RESULT: ERROR', e.message); process.exit(1); });
