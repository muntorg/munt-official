// Modules to control application life and create native browser window
const {session, app, BrowserWindow, Menu, ipcMain} = require('electron')
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
var mainWindow=null

function showMainWindow() {
    if (mainWindow)
    {
        if (mainWindow.isMinimized())
            mainWindow.restore()
        mainWindow.show()
        mainWindow.focus()
    }
}

const singleInstanceLock = app.requestSingleInstanceLock()

if (!singleInstanceLock) {
    app.quit()
}
else
{
    app.on('second-instance', (event, commandLine, workingDirectory) => {
        showMainWindow()
    })
}

function requestAppClose() {
    
}

function createWindow () {
  // Create the browser window.
  mainWindow = new BrowserWindow({
    width: 800,
    height: 600,
    webPreferences: {
      nodeIntegration: true
    }
  })
  
  if (process.platform === "linux")
  {
        mainWindow.setIcon('img/icon_512.png')
  }
  
  var menu = Menu.buildFromTemplate([{
            label: 'File',
            submenu: [
                { label:'Exit', click() { app.quit() }}
            ]
        }
    ])
    Menu.setApplicationMenu(menu); 

  // Open the DevTools.
  //mainWindow.webContents.openDevTools()

  // Emitted when the window is closed.
  mainWindow.on('closed', function () {
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
          mainWindow.hide()
          novobackend.TerminateUnityLib()
      }
      else if (!allowExit)
      {
          console.log("set core to terminate from mainWindow close")
          terminateCore=true
          mainWindow.hide()
          e.preventDefault();
      }
      else
      {
         console.log("allow app to exit, from mainwindow close")
      }
  })

  /*session.defaultSession.webRequest.onHeadersReceived((details, callback) => {
    callback({
        responseHeaders: {
        ...details.responseHeaders,
        'Content-Security-Policy': ['default-src \'none\'']
        }
    })
  })*/
  
  guldenUnitySetup();
}

//TODO: Handle this better in the backend, we shouldn't store this browser side at all
var recoveryPhrase="";

function guldenUnitySetup()
{
    var basepath = app.getPath("userData");
    var balance = 0;

    libnovo = require('./libnovo_unity_node_js')
    novobackend = new libnovo.NJSGuldenUnifiedBackend
    signalhandler = new libnovo.NJSGuldenUnifiedFrontend();

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
            mainWindow.loadFile('html/app_balance.html')
            mainWindow.webContents.on('did-finish-load', () => {
                mainWindow.webContents.send('notifyBalanceChange', balance)
            })
        }
    }
    signalhandler.logPrint  = function(message) {
        if (mainWindow)
            mainWindow.webContents.send('logPrint', message)
    }
    signalhandler.notifyUnifiedProgress  = function (progress) {
        if (mainWindow)
            mainWindow.webContents.send('notifyUnifiedProgress', progress)
    }
    signalhandler.notifyBalanceChange = function (new_balance) {
        if (mainWindow)
            mainWindow.webContents.send('notifyBalanceChange', new_balance)
        balance = new_balance
    }
    signalhandler.notifyNewMutation  = function (mutation, self_committed) {
        if (mainWindow)
            mainWindow.webContents.send('notifyNewMutation', mutation, self_committed)
    }
    signalhandler.notifyUpdatedTransaction  = function (transaction) {
        if (mainWindow)
            mainWindow.webContents.send('notifyUpdatedTransaction', transaction)
    }
    signalhandler.notifyInitWithExistingWallet = function () {
        if (mainWindow)
            mainWindow.webContents.send('notifyInitWithExistingWallet')
    }
    signalhandler.notifyInitWithoutExistingWallet = function () {
        coreIsRunning = true
        recoveryPhrase = novobackend.GenerateRecoveryMnemonic()
        mainWindow.webContents.send('notifyInitWithoutExistingWallet')
        mainWindow.loadFile('html/app_start.html')
        mainWindow.webContents.on('did-finish-load', () => {
            mainWindow.webContents.send('notifyPhrase', recoveryPhrase)
        })
    }
    signalhandler.notifyShutdown = function () {
        console.log("notify shutdown")
        allowExit=true
        coreIsRunning=false
        signalhandler=null
        app.quit()
    }

    var fs = require('fs');
    var walletpath=basepath+"/"+"wallet"
    if (!fs.existsSync(walletpath)) fs.mkdir(walletpath, function(err){});

    // Start the Gulden unified backend
    novobackend.InitUnityLibThreaded(walletpath, "", -1, -1, false, false, signalhandler, "")
}

ipcMain.on('acknowledgePhrase', (event) => {
  mainWindow.loadFile('html/app_repeat_phrase.html')
})

ipcMain.on('verifiedPhrase', (event, validatePhrase) => {
  if (validatePhrase === recoveryPhrase)
  {
    mainWindow.loadFile('html/app_password.html')
  }
  else
  {
      mainWindow.webContents.send('notifyInvalidPhrase')
  }
})

ipcMain.on('initWithPassword', (event, password) => {
  novobackend.InitWalletFromRecoveryPhrase(recoveryPhrase, password)  
  recoveryPhrase=""
})

// This method will be called when Electron has finished
// initialization and is ready to create browser windows.
// Some APIs can only be used after this event occurs.
app.on('ready', function() {
    if (mainWindow === null)
    {
        createWindow()
    }
})

// Quit when all windows are closed.
app.on('window-all-closed', function () {
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
})

app.on('activate', function () {
    showMainWindow()
})
