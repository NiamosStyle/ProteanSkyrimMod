// READ-ONLY. Dumps creature RACE / NPC_ / SPEL data from the vanilla masters to JSON.
// Writes nothing to any plugin. Usage (from the game folder, so xeditlib resolves):
//   node <repo>/tools/dump_game_data.js [outDir]      default outDir = <repo>/research/raw
// Env: GAME_ROOT (default C:/Games/steamapps/common/Skyrim Special Edition/)
const fs = require('fs'), path = require('path');
const GAME = (process.env.GAME_ROOT || 'C:/Games/steamapps/common/Skyrim Special Edition/').split('\\').join('/').replace(/\/?$/, '/');
const xelib = require(GAME + 'node_modules/xeditlib');
const OUT = process.argv[2] || path.join(__dirname, '..', 'research', 'raw');
fs.mkdirSync(OUT, { recursive: true });
const FILES = ['Skyrim.esm', 'Update.esm', 'Dawnguard.esm', 'HearthFires.esm', 'Dragonborn.esm'];
const NPC_FILES = ['Skyrim.esm', 'Dawnguard.esm', 'Dragonborn.esm'];
// NPCs are kept when their race is one of these OR their name is in NAMES (variants such as
// Pit Wolf are template NPCs whose own race record is a FoxRace placeholder).
const NAMES = new Set(require('./creature_names.json'));
xelib.init(); xelib.setLanguage('English');
xelib.setGamePath(GAME);
xelib.setGameMode(xelib.GM_SSE);
xelib.loadPlugins(FILES.join('\n'), false, false);
const g = (h, p) => { try { return xelib.getValue(h, p); } catch { return ''; } };
const hexId = r => xelib.getFormID(r, false).toString(16).toUpperCase().padStart(8, '0').slice(2);
xelib.waitForLoader().then(() => {
  const races = {};
  for (const f of NPC_FILES.concat(['Update.esm'])) {
    for (const r of xelib.getRecords(xelib.fileByName(f), 'RACE', false)) {
      races[g(r, 'EDID')] = { file: f, id: hexId(r), json: JSON.parse(xelib.elementToJson(r)) };
    }
  }
  fs.writeFileSync(path.join(OUT, 'races.json'), JSON.stringify(races));
  const raceByKey = {};
  for (const [edid, v] of Object.entries(races)) raceByKey[v.file + ':' + v.id] = edid;
  const npcs = [];
  for (const f of NPC_FILES) {
    for (const r of xelib.getRecords(xelib.fileByName(f), 'NPC_', false)) {
      const json = JSON.parse(xelib.elementToJson(r));
      const rk = json['RNAM - Race'] || '';
      const raceEdid = raceByKey[rk.split(' ')[0]] || rk;
      const name = json['FULL - Name'] || '';
      const creatureRace = /^(?!.*(Nord|Imperial|Breton|Redguard|Orc|Khajiit|Argonian|ElfRace|Elder|Dremora|Draugr|Skeleton|Vampire|Default|Dwarven|Falmer|Hagraven|Atronach|Spriggan|Dragon|Wisp|Witchlight|Manakin|Swarm|Ghost|Gargoyle)).*(Wolf|Bear|Sabre|Chaurus|Skeever|Mudcrab|Slaughter|Horker|Troll|Were|Chicken|Dog|Cow|Goat|Horse|Deer|Elk|Mammoth|Hare|Boar|Giant|Spider|AshHopper|Husky|Stag)/.test(raceEdid);
      if (!creatureRace && !NAMES.has(name)) continue;
      npcs.push({ file: f, race: raceEdid, json });
    }
  }
  fs.writeFileSync(path.join(OUT, 'npcs.json'), JSON.stringify(npcs));
  const spells = {};
  for (const f of FILES) {
    for (const r of xelib.getRecords(xelib.fileByName(f), 'SPEL', false)) {
      spells[f + ':' + hexId(r)] = { edid: g(r, 'EDID'), name: g(r, 'FULL') };
    }
  }
  fs.writeFileSync(path.join(OUT, 'spells.json'), JSON.stringify(spells));
  console.log('races', Object.keys(races).length, 'npcs', npcs.length, 'spells', Object.keys(spells).length);
  console.log('RESULT: OK');
  process.exit(0);
}).catch(e => { console.log('RESULT: ERROR', e.message); process.exit(1); });
