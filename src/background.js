"use strict";

import { app, protocol, Menu, BrowserWindow, shell } from "electron";
import {
  createProtocol
  /* installVueDevtools */
} from "vue-cli-plugin-electron-builder/lib";

const isDevelopment = process.env.NODE_ENV !== "production";

import path from "path";
import fs from "fs";

import LibNovo from "./libnovo";
import store, { AppStatus } from "./store";

import contextMenu from "electron-context-menu";

// Keep a global reference of the window object, if you don't, the window will
// be closed automatically when the JavaScript object is garbage collected.
let win;
let libNovo = new LibNovo();

/* TODO: refactor into function and add option to libgulden to remove existing wallet folder */
let args = process.argv.slice(2);
for (var i = 0; i < args.length; i++) {
  switch(args[i].toLowerCase()) {
    case "new-wallet":
      fs.rmdirSync(path.join(app.getPath("userData"), "wallet"), { recursive: true });
      break;
    default:
      console.error(`unknown argument: ${args[i]}`);
      break;
  }
}

// Scheme must be registered before the app is ready
protocol.registerSchemesAsPrivileged([
  { scheme: "app", privileges: { secure: true, standard: true } }
]);

function createWindow() {
  // Create the browser window.
  win = new BrowserWindow({
    width: 800,
    height: 600,
    show: false,
    webPreferences: {
      // Use pluginOptions.nodeIntegration, leave this alone
      // See nklayman.github.io/vue-cli-plugin-electron-builder/guide/security.html#node-integration for more info
      nodeIntegration: process.env.ELECTRON_NODE_INTEGRATION,
      enableRemoteModule: true
    }
  });

  if (process.platform !== "darwin")
  {
    let iconPath = null;
    if (isDevelopment) {
      iconPath = path.join(__dirname.replace("\\dist_electron", ""), "/public/favicon.ico");
    } else {
      iconPath = path.join(__dirname, "favicon.ico");
    }
    if (fs.existsSync(iconPath)) {
      win.setIcon(iconPath);
    }
  }

  var menu = Menu.buildFromTemplate(
    [
      {
        label: 'File',
        submenu: [
            { role:'quit' }
        ]
      },
      {
        label: 'View',
        visible: isDevelopment,
        submenu: [
          { role: 'toggleDevTools' }
        ]
      }   
  ])
  Menu.setApplicationMenu(menu); 

  if (process.env.WEBPACK_DEV_SERVER_URL) {
    // Load the url of the dev server if in development mode
    win.loadURL(process.env.WEBPACK_DEV_SERVER_URL);
    //if (!process.env.IS_TEST) win.webContents.openDevTools();
  } else {
    createProtocol("app");
    // Load the index.html when not in development
    win.loadURL("app://./index.html");
  }

  win.on("ready-to-show", () => {
    win.show();
  });

  win.on("closed", () => {
    console.log("win.on:closed");
    win = null;
  });

  // Force external hrefs to open in external browser
  win.webContents.on('new-window', function(e, url) {
    e.preventDefault();
    shell.openExternal(url);
  })
}

app.on("will-quit", event => {
  console.log("app.on:will-quit");
  if (libNovo === null || libNovo.isTerminated) return;
  store.dispatch({ type: "SET_STATUS", status: AppStatus.shutdown });
  event.preventDefault();
  libNovo.TerminateUnityLib();
});

// Quit when all windows are closed.
app.on("window-all-closed", event => {
  console.log("app.on:window-all-closed");
  // On macOS it is common for applications and their menu bar
  // to stay active until the user quits explicitly with Cmd + Q
  if (process.platform !== "darwin") {
    store.dispatch({ type: "SET_STATUS", status: AppStatus.shutdown });
    if (libNovo === null || libNovo.isTerminated) return;
    event.preventDefault();
    libNovo.TerminateUnityLib();
  }
});

app.on("activate", () => {
  // On macOS it's common to re-create a window in the app when the
  // dock icon is clicked and there are no other windows open.
  if (win === null) {
    createWindow();
  }
});

// This method will be called when Electron has finished
// initialization and is ready to create browser windows.
// Some APIs can only be used after this event occurs.
app.on("ready", async () => {
  if (isDevelopment && !process.env.IS_TEST) {
    // Install Vue Devtools
    // Devtools extensions are broken in Electron 6.0.0 and greater
    // See https://github.com/nklayman/vue-cli-plugin-electron-builder/issues/378 for more info
    // Electron will not launch with Devtools extensions installed on Windows 10 with dark mode
    // If you are not using Windows 10 dark mode, you may uncomment these lines
    // In addition, if the linked issue is closed, you can upgrade electron and uncomment these lines
    // try {
    //   await installVueDevtools()
    // } catch (e) {
    //   console.error('Vue Devtools failed to install:', e.toString())
    // }
  }

  store.dispatch({ type: "SET_WALLET_VERSION", version: app.getVersion() });
  libNovo.Initialize();

  createWindow();
});

// Exit cleanly on request from parent process in development mode.
if (isDevelopment) {
  if (process.platform === "win32") {
    process.on("message", data => {
      if (data === "graceful-exit") {
        app.quit();
      }
    });
  } else {
    process.on("SIGTERM", () => {
      app.quit();
    });
  }
}
