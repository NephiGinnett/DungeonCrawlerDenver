// A fresh crawler sheet, laid out like the official Dungeon Crawler Carl RPG sheet: identity, health, AI Favor,
// five stats, Evade/DR, Mana, gear, buffs/debuffs, attacks, skills, spells, inventory, pet, mount/vehicle,
// three sponsor boxes. Field names follow the community Obsidian plugin's schema where it had one, so a sheet
// can be exported to it later without a mapping table.

const PERSONAL_ITEMS = {
  CJ: { name: "CJ's Board Game", slot: 'trinket', note: 'Two pieces missing. Rules annotated in three colours of pen.', bonus: { int: 1, cha: 1 } },
  Scott: { name: "Scott's Lacrosse Stick", slot: 'mainHand', note: 'Shot mesh, ancient tape. Reach weapon.', dice: '1d6', bonus: { dex: 1 } },
  Marcel: { name: "Marcel's Lucky Dice", slot: 'trinket', note: 'Rolled a natural 20 once, in front of witnesses.', bonus: { cha: 1 } },
};

export function newSheet(id, displayName) {
  const item = PERSONAL_ITEMS[id];
  const gear = { head: null, body: null, mainHand: null, offHand: null, feet: null, trinket: null };
  if (item) gear[item.slot] = { name: item.name, note: item.note, bonus: item.bonus || {}, dice: item.dice || '' };

  return {
    version: 1,
    id,
    // Identity
    name: displayName,
    pronouns: '',
    race: 'Human',
    raceNote: 'Race and class are chosen on the third floor.',
    class: '',
    level: 1,
    xp: 0,
    crawlerNumber: '',
    floor: 1,
    // Health and movement
    hp: null, // null = full
    move: 6,
    step: 1,
    size: 'Medium',
    // The audience
    favor: 0,
    // Unenhanced scores (the Standard Array is 2,3,4,5,6 - 20 points)
    stats: { str: 4, int: 4, con: 4, dex: 4, cha: 4 },
    mana: null, // null = full
    evadeBuffs: 0,
    dr: 0,
    gear,
    accessories: [],
    buffs: [],
    debuffs: [],
    attacks: [
      { name: 'Unarmed Strike', rank: 0, stat: 'str', dice: '1d4', damageStat: 'str', damageType: 'Bludgeoning', effect: '' },
    ],
    skills: [
      { name: 'Athletics', rank: 0, stat: 'str', type: 'Utility', marked: false, notes: '' },
      { name: 'Perception', rank: 0, stat: 'int', type: 'Utility', marked: false, notes: '' },
    ],
    spells: [],
    hotlist: [],
    inventory: [{ item: 'Gold', quantity: 0, notes: '' }],
    pet: { name: '', kind: '', notes: '' },
    mount: { name: '', kind: '', notes: '' },
    sponsors: [
      { name: '', terms: '' },
      { name: '', terms: '' },
      { name: '', terms: '' },
    ],
    achievements: [],
    kills: 0,
    notes: '',
  };
}
