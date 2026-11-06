// The DCC TTRPG arithmetic the sheet needs, mirroring Source/DungeonCrawlerDenver/Stats/DCCRuleset.cpp so the
// companion and the game never disagree. If one changes, change the other.
//
// Confirmed from the books (see CLAUDE.md, "What the real DCC ruleset actually says"):
//   - five stats: STR, INT, CON, DEX, CHA. No Wisdom.
//   - scores start 2-6 (Standard Array 2,3,4,5,6 = 20 points) and climb into the hundreds
//   - the modifier is a banded lookup and is never negative
//   - full word ("Strength") = the score; abbreviation ("Str") = the modifier added to d20 rolls
//   - Max Mana = Intelligence SCORE, 1:1
//   - Skill check = d20 + Skill Rank + Stat Mod
// Still our own placeholders (the formulas are not in the preview excerpts): Max HP and Evade.

export const STATS = [
  { key: 'str', name: 'Strength', abbr: 'STR' },
  { key: 'int', name: 'Intelligence', abbr: 'INT' },
  { key: 'con', name: 'Constitution', abbr: 'CON' },
  { key: 'dex', name: 'Dexterity', abbr: 'DEX' },
  { key: 'cha', name: 'Charisma', abbr: 'CHA' },
];

/** Rules for Survival p.11, Table 1. 150-299 is not in the excerpt; it is interpolated as +8/+9. */
export function modifier(score) {
  const s = Math.max(0, Math.floor(Number(score) || 0));
  if (s <= 2) return 1;
  if (s <= 5) return 2;
  if (s <= 9) return 3;
  if (s <= 19) return 4;
  if (s <= 49) return 5;
  if (s <= 99) return 6;
  if (s <= 149) return 7;
  if (s <= 224) return 8;
  if (s <= 299) return 9;
  return 10;
}

export const fmtMod = (m) => (m >= 0 ? `+${m}` : `${m}`);

/** Unenhanced + gear/buffs, floored at 1 like the game (cursed gear makes you bad at something, never deletes it). */
export function enhancedScore(sheet, key) {
  const base = Number(sheet.stats?.[key]) || 0;
  let bonus = 0;
  for (const g of Object.values(sheet.gear || {})) bonus += Number(g?.bonus?.[key]) || 0;
  for (const b of sheet.buffs || []) bonus += Number(b?.bonus?.[key]) || 0;
  return Math.max(1, base + bonus);
}

export const maxMana = (sheet) => enhancedScore(sheet, 'int');

/** Placeholder, same as the game's GetMaxHitPoints: 10 + CON mod x Level. */
export const maxHp = (sheet) => 10 + modifier(enhancedScore(sheet, 'con')) * Math.max(1, Number(sheet.level) || 1);

/** Placeholder, same as the game: 10 + Dex mod + gear. */
export function evade(sheet) {
  let gear = 0;
  for (const g of Object.values(sheet.gear || {})) gear += Number(g?.evade) || 0;
  return 10 + modifier(enhancedScore(sheet, 'dex')) + gear + (Number(sheet.evadeBuffs) || 0);
}

/** Carry limit reads the SCORE, per the books' own example. Kilograms, our scale. */
export const carryKg = (sheet) => enhancedScore(sheet, 'str') * 10;

/** The game's XP curve (DCCStatsComponent): 100*L + 50*L*(L-1) to reach the next level. Capped at 5 on the tutorial floors. */
export const xpForLevel = (level) => 100 * level + 50 * level * (level - 1);

export function rollD20() {
  return 1 + Math.floor(Math.random() * 20);
}

/** A skill check exactly as the books describe it. Disadvantage rolls two and keeps the lower. */
export function skillCheck({ rank = 0, mod = 0, disadvantage = false }) {
  const a = rollD20();
  const b = disadvantage ? rollD20() : null;
  const die = disadvantage ? Math.min(a, b) : a;
  return { die, dice: disadvantage ? [a, b] : [a], rank, mod, total: die + rank + mod };
}
