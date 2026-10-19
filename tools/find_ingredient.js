// READ-ONLY: dump ingredient records whose name/EditorID matches argv[2] as JSON.
const GAME = 'C:/Games/steamapps/common/Skyrim Special Edition/';
const xelib = require(GAME + 'node_modules/xeditlib');
xelib.init(); xelib.setLanguage('English'); xelib.setGamePath(GAME); xelib.setGameMode(xelib.GM_SSE);
xelib.loadPlugins(['Skyrim.esm', 'Update.esm', 'Dawnguard.esm', 'Dragonborn.esm'].join('\n'), false, false);
const rx = new RegExp(process.argv[2] || 'heart', 'i');
xelib.waitForLoader().then(() => {
  for (const f of ['Skyrim.esm', 'Update.esm', 'Dawnguard.esm', 'Dragonborn.esm']) {
    for (const r of xelib.getRecords(xelib.fileByName(f), 'INGR', false)) {
      const j = JSON.parse(xelib.elementToJson(r));
      if (rx.test(j['EDID - Editor ID'] || '') || rx.test(j['FULL - Name'] || '')) console.log(f, JSON.stringify(j));
    }
  }
  console.log('RESULT: OK'); process.exit(0);
}).catch(e => { console.log('ERR', e.message); process.exit(1); });
