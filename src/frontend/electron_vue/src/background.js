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
import store, { AppStatus } from "./store";

// Keep a global reference of the window object, if you don't, the window will
// be closed automatically when the JavaScript object is garbage collected.
let win;
let walletPath;
if (os.platform() === "linux") {
  walletPath = path.join(
    app.getPath("home"),
    isDevelopment ? ".novo_dev" : ".novo"
  );
} else {
  walletPath = app.getPath("userData");
  if (isDevelopment) walletPath = walletPath + "_dev";
}

let libUnity = new LibUnity({ walletPath });

/* TODO: refactor into function and add option to libgulden to remove existing wallet folder */
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
protocol.registerSchemesAsPrivileged([
  { scheme: "app", privileges: { secure: true, standard: true } }
]);

function createWindow() {
  // Create the browser window.
  win = new BrowserWindow({
    width: 800,
    minWidth: 800,
    height: 600,
    minHeight: 600,
    show: false,
    title: "Novo",
    webPreferences: {
      // Use pluginOptions.nodeIntegration, leave this alone
      // See nklayman.github.io/vue-cli-plugin-electron-builder/guide/security.html#node-integration for more info
      nodeIntegration: process.env.ELECTRON_NODE_INTEGRATION,
      enableRemoteModule: true
    },
    icon: path.join(__static, "icon.png")
  });

  var menuTemplate = [
    {
      label: "File",
      submenu: [{ role: "quit" }]
    }
  ];

  if (isDevelopment) {
    menuTemplate.push({
      label: "Debug",
      submenu: [{ role: "toggleDevTools"}, { label:'Generate genesis keys', click() { console.log(libUnity.backend.GenerateGenesisKeys()) }}]
    });
  }

  var menu = Menu.buildFromTemplate(menuTemplate);

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
    libUnity.SetWindow(win);
    win.show();
  });

  win.on("close", event => {
    console.log("win.on:close");
    if (process.platform !== "darwin") {
      EnsureUnityLibTerminated(event);
    }
  });

  win.on("closed", () => {
    console.log("win.on:closed");
    win = null;
  });

  // Force external hrefs to open in external browser
  win.webContents.on("new-window", function(e, url) {
    e.preventDefault();
    shell.openExternal(url);
  });
}

app.on("will-quit", event => {
  console.log("app.on:will-quit");
  if (libUnity === null || libUnity.isTerminated) return;
  store.dispatch({ type: "SET_STATUS", status: AppStatus.shutdown });
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
  libUnity.Initialize();

  createWindow();
});

function EnsureUnityLibTerminated(event) {
  if (libUnity === null || libUnity.isTerminated) return;
  store.dispatch({ type: "SET_STATUS", status: AppStatus.shutdown });
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
