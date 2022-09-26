// Modules to control application life and create native browser window
const {app, BrowserWindow} = require('electron')

// Keep global references of all these objects
let libcore
let corebackend
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

  // and load the index.html of the app.
  mainWindow.loadFile('index.html')

  // Open the DevTools.
   mainWindow.webContents.openDevTools()

  // Emitted when the window is closed.
  mainWindow.on('closed', function () {
    // Dereference the window object, usually you would store windows
    // in an array if your app supports multi windows, this is the time
    // when you should delete the corresponding element.
    mainWindow = null
  })

  mainWindow.on('close', (e) => {
      if (coreIsRunning)
      {
          console.log("terminate core from mainWindow close")
          e.preventDefault();
          corebackend.TerminateUnityLib()
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

  unitySetup();
}

function unitySetup() {
    var basepath = app.getAppPath();

    global.libcore = libcore = require('./lib_unity')
    global.corebackend = corebackend = new libcore.NJSILibraryController
    signalhandler = global.signalhandler = new libcore.NJSILibraryListener();

    // Receive signals from the core and marshall them as needed to the main window
    signalhandler.notifyCoreReady = function() {
        coreIsRunning=true
        console.log("received: notifyCoreReady")
        mainWindow.webContents.send('notifyCoreReady')
        if (terminateCore)
        {
            console.log("terminate core immediately after init")
            terminateCore=false
            corebackend.TerminateUnityLib()
        }
    }
    signalhandler.logPrint  = function(message) {
        console.log("unity_core: " + message)
        mainWindow.webContents.send('logPrint', message)
    }
    signalhandler.notifyUnifiedProgress  = function (progress) {
        console.log("received: notifyUnifiedProgress")
        mainWindow.webContents.send('notifyUnifiedProgress', progress)
    }
    signalhandler.notifyBalanceChange = function (new_balance) {
        console.log("received: notifyBalanceChange")
        mainWindow.webContents.send('notifyBalanceChange', new_balance)
    }
    signalhandler.notifyNewMutation  = function (mutation, self_committed) {
        console.log("received: notifyNewMutation")
        mainWindow.webContents.send('notifyNewMutation', mutation, self_committed)
    }
    signalhandler.notifyUpdatedTransaction  = function (transaction) {
        console.log("received: notifyUpdatedTransaction")
        mainWindow.webContents.send('notifyUpdatedTransaction', transaction)
    }
    signalhandler.notifyInitWithExistingWallet = function () {
        console.log("received: notifyInitWithExistingWallet")
        mainWindow.webContents.send('notifyInitWithExistingWallet')
    }
    signalhandler.notifyInitWithoutExistingWallet = function () {
        console.log("received: notifyInitWithoutExistingWallet")
        mainWindow.webContents.send('notifyInitWithoutExistingWallet')
        corebackend.InitWalletFromRecoveryPhrase(corebackend.GenerateRecoveryMnemonic().phrase_with_birth_number ,"password")
    }
    signalhandler.notifyShutdown = function () {

        console.log("call app.quit from notifyShutdown")
        allowExit=true
        coreIsRunning=false
        app.quit()
    }

    // Start the unified backend
    corebackend.InitUnityLibThreaded(basepath+"/"+"wallet", "", -1, -1, false, true, signalhandler, "")
}

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
          corebackend.TerminateUnityLib()
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

