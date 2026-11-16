const { contextBridge, ipcRenderer, webUtils } = require('electron');
contextBridge.exposeInMainWorld('native', {
  setHeaders: (h) => ipcRenderer.invoke('set-headers', h),
  openFile: () => ipcRenderer.invoke('open-file'),
  urlForDroppedFile: (file) => ipcRenderer.invoke('path-to-url', webUtils.getPathForFile(file)),
});
