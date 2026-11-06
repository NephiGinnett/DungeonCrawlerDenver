// Packaging for friends: `npm run dist` makes dist/Crawler Companion Setup <version>.exe.
//
// Everyone's model and the chatter table live outside this folder (../SourceArt, ../Docs), so they are copied
// into the installer as extra resources at the same relative paths main.js expects. The list is read from
// data/roster.json, so adding a crawler there is enough to ship their model too.

const fs = require('fs');
const path = require('path');

const roster = JSON.parse(fs.readFileSync(path.join(__dirname, 'data', 'roster.json'), 'utf8'));
const meshyFile = path.join(__dirname, 'data', 'meshy-library.json');
const meshy = fs.existsSync(meshyFile) ? JSON.parse(fs.readFileSync(meshyFile, 'utf8')) : { files: [] };
const models = [...new Set([...roster.crawlers.map((c) => c.model), ...roster.animationLibrary, ...meshy.files].filter(Boolean))];

const extraResources = [
  ...models.map((m) => ({ from: path.join('..', m), to: m })),
  { from: path.join('..', 'Docs', 'DT_DCCChatterLines.csv'), to: 'Docs/DT_DCCChatterLines.csv' },
];

for (const r of extraResources) {
  if (!fs.existsSync(path.join(__dirname, r.from))) throw new Error(`Missing file for the installer: ${r.from}`);
}

module.exports = {
  appId: 'com.dungeoncrawlerdenver.companion',
  productName: 'Crawler Companion',
  directories: { output: 'release' },
  files: [
    'main.js',
    'preload.js',
    'index.html',
    'buddy.html',
    'src/**/*',
    '!src/studio/**',
    'data/roster.json',
    'data/meshy-library.json',
    'assets/**/*',
    'node_modules/three/build/three.module.js',
    'node_modules/three/package.json',
  ],
  extraResources,
  win: {
    target: [{ target: 'nsis', arch: ['x64'] }],
    icon: 'assets/companion.ico',
    // Unsigned on purpose: a code-signing certificate costs money. Friends will see a SmartScreen warning
    // the first time ("More info" -> "Run anyway"); the README says so.
    signAndEditExecutable: false,
  },
  nsis: {
    oneClick: true,           // one double-click, no wizard pages to get lost in
    perMachine: false,        // installs for the current user, so it needs no admin password
    createDesktopShortcut: true,
    createStartMenuShortcut: true,
    shortcutName: 'Crawler Companion',
    installerIcon: 'assets/companion.ico',
    uninstallerIcon: 'assets/companion.ico',
    runAfterFinish: true,
  },
};
