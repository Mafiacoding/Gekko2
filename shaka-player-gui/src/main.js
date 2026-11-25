const { app, BrowserWindow, dialog, ipcMain, protocol, net, Menu, session } = require('electron');
const path = require('path');
const { pathToFileURL } = require('url');

protocol.registerSchemesAsPrivileged([
  { scheme: 'localfile', privileges: { standard: true, secure: true, supportFetchAPI: true, stream: true, corsEnabled: true, bypassCSP: true } },
]);

let win;
let customHeaders = {};
const MEDIA_EXT = ['mp4', 'm4v', 'webm', 'mkv', 'mp3', 'm4a', 'ogg', 'opus', 'flac', 'wav', 'mpd', 'm3u8', 'ism'];

function toLocalUrl(p) {
  return 'localfile://f/' + p.replace(/\\/g, '/').split('/').map(encodeURIComponent).join('/');
}

function fileArg(argv) {
  const a = argv.slice(app.isPackaged ? 1 : 2).find((x) => !x.startsWith('-') && MEDIA_EXT.includes(path.extname(x).slice(1).toLowerCase()));
  return a ? toLocalUrl(path.resolve(a)) : null;
}

function createWindow() {
  win = new BrowserWindow({
    width: 1280, height: 800, minWidth: 720, minHeight: 480,
    backgroundColor: '#111111', title: 'Shaka Player GUI',
    webPreferences: { preload: path.join(__dirname, 'preload.js'), contextIsolation: true, nodeIntegration: false, sandbox: true },
  });
  Menu.setApplicationMenu(null);
  win.loadFile(path.join(__dirname, 'index.html'), { query: { open: fileArg(process.argv) || '' } });
}

app.whenReady().then(() => {
  protocol.handle('localfile', (req) => {
    const u = new URL(req.url);
    const p = decodeURIComponent(u.pathname).replace(/^\//, '');
    return net.fetch(pathToFileURL(p).toString(), { headers: req.headers });
  });

  ipcMain.handle('open-file', async () => {
    const r = await dialog.showOpenDialog(win, {
      properties: ['openFile'],
      filters: [
        { name: 'Medien & Manifeste', extensions: MEDIA_EXT },
        { name: 'Alle Dateien', extensions: ['*'] },
      ],
    });
    return r.canceled ? null : { url: toLocalUrl(r.filePaths[0]), name: path.basename(r.filePaths[0]) };
  });
  session.defaultSession.webRequest.onBeforeSendHeaders({ urls: ['http://*/*', 'https://*/*'] }, (details, cb) => {
    const h = { ...details.requestHeaders };
    for (const [k, v] of Object.entries(customHeaders)) {
      for (const ek of Object.keys(h)) if (ek.toLowerCase() === k.toLowerCase()) delete h[ek];
      h[k] = v;
    }
    cb({ requestHeaders: h });
  });
  ipcMain.handle('set-headers', (_e, h) => { customHeaders = (h && typeof h === 'object') ? h : {}; });
  ipcMain.handle('path-to-url', (_e, p) => toLocalUrl(p));

  createWindow();
  app.on('activate', () => { if (!BrowserWindow.getAllWindows().length) createWindow(); });
});

app.on('window-all-closed', () => app.quit());
