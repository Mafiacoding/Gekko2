(() => {
  const repo = 'Mafiacoding/Gekko2';
  fetch(`https://api.github.com/repos/${repo}/releases/latest`, {headers:{Accept:'application/vnd.github+json'}})
    .then(r => { if (!r.ok) throw new Error('no release'); return r.json(); })
    .then(release => {
      const name = document.querySelector('#release-name');
      const note = document.querySelector('#release-note');
      const link = document.querySelector('#release-link');
      if (name) name.textContent = release.name || release.tag_name;
      if (note) note.textContent = release.published_at ? `Published ${new Date(release.published_at).toLocaleDateString()}` : 'Latest public release';
      if (link) { link.href = release.html_url; link.textContent = 'Open release ↗'; }
    })
    .catch(() => {});
})();
