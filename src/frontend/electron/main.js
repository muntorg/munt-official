// Modules to control application life and create native browser window
const {app, BrowserWindow, Menu, ipcMain} = require('electron')
const contextMenu = require('electron-context-menu');

// Keep global references of all these objects
let libnovo
let novobackend
let signalhandler

let coreIsRunning=false
let allowExit=false
let terminateCore=false

// Keep a global reference of the window object, if you don't, the window will
// be closed automatically when the JavaScript object is garbage collected.
let mainWindow

function createWindow () {
  // Create the browser window.
  mainWindow = new BrowserWindow({
    width: 800,
    height: 600,
    webPreferences: {
      nodeIntegration: true
    }
  })
  
  var menu = Menu.buildFromTemplate([{
            label: 'File',
            submenu: [
                {label:'Exit'}
            ]
        }
    ])
    Menu.setApplicationMenu(menu); 

  // Start with splash screen loaded
  mainWindow.loadFile('splash.html')

  // Open the DevTools.
  //mainWindow.webContents.openDevTools()

  // Emitted when the window is closed.
  mainWindow.on('closed', function () {
    // Dereference the window object, usually you would store windows
    // in an array if your app supports multi windows, this is the time
    // when you should delete the corresponding element.
    mainWindow = null
  })
  
  // Force external hrefs to open in external browser
  mainWindow.webContents.on('new-window', function(e, url) {
    e.preventDefault();
    require('electron').shell.openExternal(url);
  })

  mainWindow.on('close', (e) => {
      if (coreIsRunning)
      {
          console.log("terminate core from mainWindow close")
          e.preventDefault();
          novobackend.TerminateUnityLib()
      }
      else if (!allowExit)
      {
          console.log("set core to terminate from mainWindow close")
          terminateCore=true
          e.preventDefault();
      }
      else
      {
         console.log("allow app to exit, from mainwindow close")
      }
  })

  guldenUnitySetup();
}

function guldenUnitySetup() {
    var basepath = app.getPath("userData");
    var balance = 0;

    global.libnovo = libnovo = require('./libnovo_unity_node_js')
    global.novobackend = novobackend = new libnovo.NJSGuldenUnifiedBackend
    signalhandler = global.signalhandler = new libnovo.NJSGuldenUnifiedFrontend();

    // Receive signals from the core and marshall them as needed to the main window
    signalhandler.notifyCoreReady = function() {
        coreIsRunning=true
        if (terminateCore)
        {
            console.log("terminate core immediately after init")
            terminateCore=false
            novobackend.TerminateUnityLib()
        }
        else
        {
            mainWindow.loadFile('index.html')
            mainWindow.webContents.on('did-finish-load', () => {
                mainWindow.webContents.send('notifyBalanceChange', balance)
            })
        }
    }
    signalhandler.logPrint  = function(message) {
        mainWindow.webContents.send('logPrint', message)
    }
    signalhandler.notifyUnifiedProgress  = function (progress) {
        mainWindow.webContents.send('notifyUnifiedProgress', progress)
    }
    signalhandler.notifyBalanceChange = function (new_balance) {
        mainWindow.webContents.send('notifyBalanceChange', new_balance)
        balance = new_balance
    }
    signalhandler.notifyNewMutation  = function (mutation, self_committed) {
        mainWindow.webContents.send('notifyNewMutation', mutation, self_committed)
    }
    signalhandler.notifyUpdatedTransaction  = function (transaction) {
        mainWindow.webContents.send('notifyUpdatedTransaction', transaction)
    }
    signalhandler.notifyInitWithExistingWallet = function () {
        mainWindow.webContents.send('notifyInitWithExistingWallet')
    }
    signalhandler.notifyInitWithoutExistingWallet = function () {
        mainWindow.webContents.send('notifyInitWithoutExistingWallet')
        mainWindow.loadFile('phrase.html')
        mainWindow.webContents.on('did-finish-load', () => {
            mainWindow.webContents.send('notifyPhrase', novobackend.GenerateRecoveryMnemonic())
        })
    }
    signalhandler.notifyShutdown = function () {
        allowExit=true
        coreIsRunning=false
        app.quit()
    }

    var fs = require('fs');
    var walletpath=basepath+"/"+"wallet"
    if (!fs.existsSync(walletpath)) fs.mkdir(walletpath, function(err){});

    // Start the Gulden unified backend
    novobackend.InitUnityLibThreaded(walletpath, "", -1, -1, false, signalhandler, "")
}

ipcMain.on('init-with-password', (event, phrase, password) => {
  novobackend.InitWalletFromRecoveryPhrase(phrase, password)  
})

// This method will be called when Electron has finished
// initialization and is ready to create browser windows.
// Some APIs can only be used after this event occurs.
app.on('ready', createWindow)

// Quit when all windows are closed.
app.on('window-all-closed', function () {
  // On macOS it is common for applications and their menu bar
  // to stay active until the user quits explicitly with Cmd + Q
  if (process.platform !== 'darwin')
  {
      if (coreIsRunning)
      {
            console.log("terminate core from window-all-closed")
          novobackend.TerminateUnityLib()
      }
      else
      {
          console.log("set core to terminate after init window-all-closed")
          terminateCore=true
      }
  }
})

app.on('activate', function () {
  // On macOS it's common to re-create a window in the app when the
  // dock icon is clicked and there are no other windows open.
  if (mainWindow === null) createWindow()
})

