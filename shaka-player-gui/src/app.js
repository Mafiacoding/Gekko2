/* global shaka */
const $ = (id) => document.getElementById(id);
const store = {
  get: (k, d) => { try { return JSON.parse(localStorage.getItem(k)) ?? d; } catch { return d; } },
  set: (k, v) => { try { localStorage.setItem(k, JSON.stringify(v)); } catch {} },
};

let player, ui;

function setStatus(t) { $('status').textContent = t; }

async function init() {
  shaka.polyfill.installAll();
  if (!shaka.Player.isBrowserSupported()) { setStatus('Browser wird nicht unterstützt'); return; }

  const video = $('video');
  player = new shaka.Player();
  await player.attach(video);
  ui = new shaka.ui.Overlay(player, $('video-container'), video);
  ui.configure({
    seekBarColors: { played: '#3a6ff7' },
    controlPanelElements: ['play_pause', 'time_and_duration', 'spacer', 'mute', 'volume', 'quality', 'language', 'captions', 'playback_rate', 'loop', 'picture_in_picture', 'fullscreen', 'overflow_menu'],
    overflowMenuButtons: ['quality', 'language', 'captions', 'playback_rate', 'loop', 'statistics', 'picture_in_picture'],
  });

  player.addEventListener('error', (e) => setStatus('Fehler ' + e.detail.code + ': ' + (e.detail.message || 'siehe Konsole')));
  player.addEventListener('buffering', (e) => { if (e.buffering) setStatus('Puffert…'); });
  video.addEventListener('playing', () => setStatus('Spielt'));
  video.addEventListener('pause', () => setStatus('Pausiert'));
  setInterval(updateStats, 1000);

  restoreSettings();
  bindUI();

  const initial = new URLSearchParams(location.search).get('open');
  if (initial) load(initial);
}

function restoreSettings() {
  const s = store.get('settings', {});
  for (const id of ['ck-pairs', 'ck-order', 'lic-wv', 'lic-pr', 'headers']) if (s[id]) $(id).value = s[id];
  for (const id of ['opt-abr', 'opt-loop', 'opt-stats', 'hdr-lic']) if (id in s) $(id).checked = s[id];
  const h = store.get('history', []);
  $('history').innerHTML = h.map((u) => `<option value="${u.replace(/"/g, '&quot;')}">`).join('');
}

function saveSettings() {
  const s = {};
  for (const id of ['ck-pairs', 'ck-order', 'lic-wv', 'lic-pr', 'headers']) s[id] = $(id).value;
  for (const id of ['opt-abr', 'opt-loop', 'opt-stats', 'hdr-lic']) s[id] = $(id).checked;
  store.set('settings', s);
}

function applyConfig(skipKeys) {
  player.resetConfiguration();
  const cfg = { abr: { enabled: $('opt-abr').checked }, drm: { servers: {}, clearKeys: {} } };
  const idFirst = $('ck-order').value === 'id-key';
  for (const line of (skipKeys ? '' : $('ck-pairs').value).split(/\r?\n/)) {
    const m = line.trim().replace(/-/g, '').match(/^([0-9a-fA-F]{32}):([0-9a-fA-F]{32})$/);
    if (!m) { if (line.trim()) setStatus('Ungültige Key-Zeile (erwarte 32 Hex : 32 Hex): ' + line.trim().slice(0, 20) + '…'); continue; }
    cfg.drm.clearKeys[(idFirst ? m[1] : m[2]).toLowerCase()] = (idFirst ? m[2] : m[1]).toLowerCase();
  }
  if ($('lic-wv').value.trim()) cfg.drm.servers['com.widevine.alpha'] = $('lic-wv').value.trim();
  if ($('lic-pr').value.trim()) cfg.drm.servers['com.microsoft.playready'] = $('lic-pr').value.trim();
  player.configure(cfg);

  player.getNetworkingEngine().clearAllRequestFilters();
  const hdr = parseHeaders($('headers').value);
  window.native.setHeaders(hdr);
  if (Object.keys(hdr).length) {
    const T = shaka.net.NetworkingEngine.RequestType;
    player.getNetworkingEngine().registerRequestFilter((type, req) => {
      if (type === T.LICENSE && !$('hdr-lic').checked) return;
      Object.assign(req.headers, hdr);
    });
  }
  $('video').loop = $('opt-loop').checked;
}

function parseHeaders(raw) {
  raw = raw.trim();
  if (!raw) return {};
  if (raw.startsWith('{')) { try { return JSON.parse(raw); } catch { setStatus('Header: ungültiges JSON'); return {}; } }
  const out = {};
  for (const line of raw.split(/\r?\n/)) {
    const i = line.indexOf(':');
    if (i > 0) out[line.slice(0, i).trim()] = line.slice(i + 1).trim();
  }
  return out;
}

async function load(uri, name) {
  if (!uri) return;
  saveSettings();
  applyConfig();
  setStatus('Lade ' + (name || uri) + '…');
  try {
    await player.load(uri);
    $('video-container').classList.add('has-media');
    document.title = (name || uri) + ' – Shaka Player GUI';
    setStatus('Spielt');
    if (!uri.startsWith('localfile:')) {
      const h = [uri, ...store.get('history', []).filter((x) => x !== uri)].slice(0, 20);
      store.set('history', h);
      restoreSettings();
    }
  } catch (e) {
    if (e.code === 6001 && $('ck-pairs').value.trim() && !load.retrying) {
      load.retrying = true;
      applyConfig(true);
      try { await player.load(uri); $('video-container').classList.add('has-media'); setStatus('Spielt (Quelle unverschlüsselt – Keys ignoriert)'); return; }
      catch (e2) { e = e2; } finally { load.retrying = false; }
    }
    setStatus('Fehler ' + (e.code || '') + ': ' + (e.message || 'Laden fehlgeschlagen'));
    console.error(e);
  }
}

function updateStats() {
  if (!player || !$('opt-stats').checked || !player.getAssetUri()) { $('stats').textContent = ''; return; }
  const s = player.getStats();
  $('stats').textContent = `${s.streamBandwidth ? Math.round(s.streamBandwidth / 1000) : "?"} kbit/s · ${s.width || '?'}×${s.height || '?'} · verworfene Frames ${s.droppedFrames} · Puffer ${Math.round(s.bufferingTime)}s`;
}

function bindUI() {
  $('btn-open').onclick = async () => { const f = await window.native.openFile(); if (f) { $('url').value = ''; load(f.url, f.name); } };
  $('btn-load').onclick = () => load($('url').value.trim());
  $('url').onkeydown = (e) => { if (e.key === 'Enter') load($('url').value.trim()); };
  $('btn-opts').onclick = () => { $('panel').hidden = !$('panel').hidden; };
  for (const id of ['opt-loop', 'opt-abr', 'hdr-lic']) $(id).onchange = () => { saveSettings(); if (player) applyConfig(); };
  $('opt-stats').onchange = saveSettings;
  window.addEventListener('keydown', (e) => { if (e.ctrlKey && e.key.toLowerCase() === 'o') $('btn-open').click(); });

  const box = $('video-container');
  window.addEventListener('dragover', (e) => { e.preventDefault(); box.classList.add('drag'); });
  window.addEventListener('dragleave', () => box.classList.remove('drag'));
  window.addEventListener('drop', async (e) => {
    e.preventDefault(); box.classList.remove('drag');
    const f = e.dataTransfer.files[0];
    if (f) load(await window.native.urlForDroppedFile(f), f.name);
    else { const t = e.dataTransfer.getData('text'); if (t) { $('url').value = t; load(t); } }
  });
}

document.addEventListener('DOMContentLoaded', init);
