const fs = require('fs'), path = require('path');
const root = path.join(__dirname, '..');
const dest = path.join(root, 'src', 'vendor');
fs.mkdirSync(dest, { recursive: true });
const dist = path.join(root, 'node_modules', 'shaka-player', 'dist');
for (const f of ['shaka-player.ui.js', 'controls.css']) {
  fs.copyFileSync(path.join(dist, f), path.join(dest, f));
}
console.log('Shaka Player nach src/vendor kopiert');
