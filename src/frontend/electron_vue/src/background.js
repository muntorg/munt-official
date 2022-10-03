"use strict";
/* global __static */

import { app, protocol, Menu, BrowserWindow, shell } from "electron";
import {
  createProtocol
  /* installVueDevtools */
} from "vue-cli-plugin-electron-builder/lib";

const isDevelopment = process.env.NODE_ENV !== "production";
const os = require("os");

import path from "path";
import fs from "fs";

import LibUnity from "./unity/LibUnity";
import walletPath from "./walletPath";

import store from "./store";
import AppStatus from "./AppStatus";
import axios from "axios";

// Keep a global reference of the window object, if you don't, the window will
// be closed automatically when the JavaScript object is garbage collected.
let winMain;
let winDebug;
let libUnity = new LibUnity({ walletPath });

// Handle URI links (munt: munt://)
// If we are launching a second instance then terminate and let the first instance handle it instead
//NB! This must happen before all other app related code (especially libulden init) as otherwise second process can crash
const gotTheLock = app.requestSingleInstanceLock();
if (!gotTheLock) {
  app.quit();
} else {
  app.on("second-instance", (_event, _commandLine, _workingDirectory) => {
    focusMainWindow();
  });
  // Protocol handler for osx
  app.on("open-url", (event, _url) => {
    event.preventDefault();
    focusMainWindow();
  });
}
if (process.defaultApp) {
  if (process.argv.length >= 2) {
    app.setAsDefaultProtocolClient("munt", process.execPath, [path.resolve(process.argv[1])]);
  }
} else {
  app.setAsDefaultProtocolClient("munt");
}
// End of URI handling

/* TODO: refactor into function and add option to libmunt to remove existing wallet folder */
if (isDevelopment) {
  let args = process.argv.slice(2);
  for (var i = 0; i < args.length; i++) {
    switch (args[i].toLowerCase()) {
      case "new-wallet":
        fs.rmdirSync(walletPath, {
          recursive: true
        });
        break;
      default:
        console.error(`unknown argument: ${args[i]}`);
        break;
    }
  }
}

// Scheme must be registered before the app is ready
protocol.registerSchemesAsPrivileged([{ scheme: "app", privileges: { secure: true, standard: true } }]);

function createMainWindow() {
  console.log("createMainWindow");
  let options = {
    width: 800,
    minWidth: 800,
    height: 600,
    minHeight: 600,
    show: false,
    title: "Munt",
    webPreferences: {
      // Use pluginOptions.nodeIntegration, leave this alone
      // See nklayman.github.io/vue-cli-plugin-electron-builder/guide/security.html#node-integration for more info
      nodeIntegration: process.env.ELECTRON_NODE_INTEGRATION,
      contextIsolation: false
    }
  };
  if (os.platform() === "linux") {
    options = Object.assign({}, options, {
      icon: path.join(__static, "icon.png")
    });
  }
  winMain = new BrowserWindow(options);

  var menuTemplate = [
    {
      label: "File",
      submenu: [{ role: "quit" }]
    },
    {
      label: "Edit",
      submenu: [{ role: "cut" }, { role: "copy" }, { role: "paste" }]
    },
    {
      label: "Help",
      submenu: [
        {
          label: "Debug window",
          enabled: store.state.app.coreReady,
          click() {
            createDebugWindow();
          }
        }
      ]
    }
  ];

  if (isDevelopment) {
    menuTemplate.push({
      label: "Development",
      submenu: [{ role: "toggleDevTools" }]
    });
  }

  var menu = Menu.buildFromTemplate(menuTemplate);

  Menu.setApplicationMenu(menu);

  if (process.env.WEBPACK_DEV_SERVER_URL) {
    // Load the url of the dev server if in development mode
    winMain.loadURL(process.env.WEBPACK_DEV_SERVER_URL);
    //if (!process.env.IS_TEST) win.webContents.openDevTools();
  } else {
    createProtocol("app");
    // Load the index.html when not in development
    winMain.loadURL("app://./index.html");
  }

  winMain.on("ready-to-show", () => {
    console.log("ready-to-show main window");
    libUnity.SetMainWindowReady();
    winMain.show();
  });

  winMain.on("close", event => {
    console.log("winMain.on:close");
    if (process.platform !== "darwin") {
      EnsureUnityLibTerminated(event);
    }
  });

  winMain.on("closed", () => {
    console.log("winMain.on:closed");
    winMain = null;
  });

  // Force external hrefs to open in external browser
  winMain.webContents.on("new-window", function(e, url) {
    e.preventDefault();
    shell.openExternal(url);
  });
}

function createDebugWindow() {
  if (winDebug) {
    return winDebug.show();
  }

  let options = {
    width: 600,
    minWidth: 400,
    height: 600,
    minHeight: 400,
    show: false,
    parent: winMain,
    title: "Munt Debug Window",
    skipTaskBar: true,
    autoHideMenuBar: true,
    webPreferences: {
      // Use pluginOptions.nodeIntegration, leave this alone
      // See nklayman.github.io/vue-cli-plugin-electron-builder/guide/security.html#node-integration for more info
      nodeIntegration: process.env.ELECTRON_NODE_INTEGRATION,
      contextIsolation: false
    }
  };
  if (os.platform() === "linux") {
    options = Object.assign({}, options, {
      icon: path.join(__static, "icon.png")
    });
  }
  winDebug = new BrowserWindow(options);

  let url = process.env.WEBPACK_DEV_SERVER_URL ? `${process.env.WEBPACK_DEV_SERVER_URL}#/debug` : `app://./index.html#/debug`;

  if (process.env.WEBPACK_DEV_SERVER_URL) {
    // Load the url of the dev server if in development mode
    winDebug.loadURL(url);
    //if (!process.env.IS_TEST) win.webContents.openDevTools();
  } else {
    createProtocol("app");
    // Load the index.html when not in development
    winDebug.loadURL(url);
  }

  winDebug.on("ready-to-show", () => {
    winDebug.show();
  });

  winDebug.on("close", event => {
    console.log("winDebug.on:close");
    if (libUnity === null || libUnity.isTerminated) return;
    event.preventDefault();
    winDebug.hide();
  });

  winDebug.on("closed", () => {
    console.log("winDebug.on:closed");
    winDebug = null;
  });
}

app.on("will-quit", event => {
  console.log("app.on:will-quit");
  if (libUnity === null || libUnity.isTerminated) return;
  store.dispatch("app/SET_STATUS", AppStatus.shutdown);
  event.preventDefault();
  libUnity.TerminateUnityLib();
});

// Quit when all windows are closed.
app.on("window-all-closed", event => {
  console.log("app.on:window-all-closed");
  // On macOS it is common for applications and their menu bar
  // to stay active until the user quits explicitly with Cmd + Q
  if (process.platform !== "darwin") {
    EnsureUnityLibTerminated(event);
  }
});

app.on("activate", () => {
  // On macOS it's common to re-create a window in the app when the
  // dock icon is clicked and there are no other windows open.
  if (winMain === null) {
    createMainWindow();
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

  store.dispatch("app/SET_WALLET_VERSION", app.getVersion());
  libUnity.Initialize();

  updateRate(60);

  createMainWindow();
});

async function updateRate(seconds) {
  try {
    // use blockhut api instead of https://api.gulden.com/api/v1/ticker
    const response = await axios.get("https://blockhut.com/munt/munteuro.json");

    store.dispatch("app/SET_RATE", response.data.eurxfl);
  } catch (error) {
    console.error(error);
  } finally {
    setTimeout(() => {
      updateRate(seconds);
    }, seconds * 1000);
  }
}

function EnsureUnityLibTerminated(event) {
  if (libUnity === null || libUnity.isTerminated) return;
  store.dispatch("app/SET_STATUS", AppStatus.shutdown);
  event.preventDefault();
  libUnity.TerminateUnityLib();
}

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

function focusMainWindow() {
  if (winMain) {
    if (winMain.isMinimized()) winMain.restore();
    else winMain.show();
    winMain.focus();
  }
}
