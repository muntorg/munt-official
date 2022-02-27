import { app } from "electron";
import { ipcMain as ipc } from "electron-better-ipc";
import fs from "fs";
import axios from "axios";
import FormData from "form-data";
import store from "../store";

import libUnity from "native-ext-loader!./lib_unity.node";

class LibUnity {
  constructor(options) {
    this.initialized = false;
    this.isTerminated = false;
    this.isCoreReady = false;
    this.isMainWindowReady = false;

    this.options = {
      useTestnet: process.env.UNITY_USE_TESTNET
        ? process.env.UNITY_USE_TESTNET
        : false,
      extraArgs: process.env.UNITY_EXTRA_ARGS
        ? process.env.UNITY_EXTRA_ARGS
        : "",
      ...options
    };

    this.libraryController = new libUnity.NJSILibraryController();
    this.libraryListener = new libUnity.NJSILibraryListener();

    this.walletController = new libUnity.NJSIWalletController();
    this.walletListener = new libUnity.NJSIWalletListener();

    this.accountsController = new libUnity.NJSIAccountsController();
    this.accountsListener = new libUnity.NJSIAccountsListener();

    this.generationController = new libUnity.NJSIGenerationController();
    this.generationListener = new libUnity.NJSIGenerationListener();

    this.witnessController = new libUnity.NJSIWitnessController();

    this.rpcController = new libUnity.NJSIRpcController();

    let buildInfo = this.libraryController.BuildInfo();

    store.dispatch(
      "app/SET_UNITY_VERSION",
      buildInfo.substr(1, buildInfo.indexOf("-") - 1)
    );
  }

  SetMainWindowReady() {
    if (this.isMainWindowReady === true) return; // set core state once
    this.isMainWindowReady = true;
    this._setStateWhenCoreAndMainWindowReady();
  }

  Initialize() {
    if (this.initialized) return;

    this.initialized = true;

    this._registerIpcHandlers();
    this._registerSignalHandlers();
    this._startUnityLib();
  }

  TerminateUnityLib() {
    if (this.libraryController === null || this.isTerminated) return;
    console.log(`terminating unity lib`);
    // TODO:
    // Maybe the call to terminate comes before the core is ready.
    // Then it's better to wait for the coreReady signal and then call TerminateUnityLib
    this.libraryController.TerminateUnityLib();
  }

  _initializeWalletController() {
    console.log("_initializeWalletController");

    this.walletListener.notifyBalanceChange = function(new_balance) {
      console.log(`walletListener.notifyBalanceChange`);
      store.dispatch("wallet/SET_WALLET_BALANCE", new_balance);
    };

    this.walletListener.notifyNewMutation = function(
      mutation /*, self_committed*/
    ) {
      console.log("walletListener.notifyNewMutation");
      console.log(mutation);
    };

    this.walletListener.notifyUpdatedTransaction = function(transaction) {
      console.log("walletListener.notifyUpdatedTransaction");
      console.log(transaction);
    };

    this.walletController.setListener(this.walletListener);
  }

  _updateAccounts() {
    let accounts = this.accountsController.listAccounts();
    let accountBalances = this.accountsController.getAllAccountBalances();

    Object.keys(accountBalances).forEach(key => {
      let currentAccount = accounts.find(x => x.UUID === key);
      let currentBalance = accountBalances[key];

      currentAccount.balance = currentBalance.availableIncludingLocked + currentBalance.immatureIncludingLocked;
      currentAccount.spendable =  currentBalance.availableExcludingLocked - currentBalance.immatureExcludingLocked;
    });

    store.dispatch("wallet/SET_ACCOUNTS", accounts);
  }

  _initializeAccountsController() {
    console.log("_initializeAccountsController");

    let self = this;
    let libraryController = this.libraryController;

    this.accountsListener.onAccountNameChanged = function(
      accountUUID,
      newAccountName
    ) {
      store.dispatch("wallet/SET_ACCOUNT_NAME", {
        accountUUID,
        newAccountName
      });
    };

    this.accountsListener.onActiveAccountChanged = function(accountUUID) {
      store.dispatch("app/SET_ACTIVITY_INDICATOR", true);

      store.dispatch("wallet/SET_ACTIVE_ACCOUNT", accountUUID);

      store.dispatch(
        "wallet/SET_RECEIVE_ADDRESS",
        libraryController.GetReceiveAddress()
      );

      store.dispatch(
        "wallet/SET_MUTATIONS",
        libraryController.getMutationHistory()
      );
    };

    this.accountsListener.onAccountAdded = function() {
      self._updateAccounts();
    };

    this.accountsListener.onAccountDeleted = function() {
      self._updateAccounts();
    };

    this.accountsController.setListener(this.accountsListener);
  }

  _initializeGenerationController() {
    console.log("_initializeGenerationController");

    this.generationListener.onGenerationStarted = function() {
      console.log("GENERATION STARTED");
      store.dispatch("mining/SET_ACTIVE", true);
    };

    this.generationListener.onGenerationStopped = function() {
      console.log("GENERATION STOPPED");

      store.dispatch("mining/SET_ACTIVE", false);
      store.dispatch("mining/SET_STATS", null);
    };

    this.generationListener.onStatsUpdated = function(
      hashesPerSecond,
      hashesPerSecondUnit,
      rollingHashesPerSecond,
      rollingHashesPerSecondUnit,
      bestHashesPerSecond,
      bestHashesPerSecondUnit,
      arenaSetupTime
    ) {
      store.dispatch("mining/SET_STATS", {
        hashesPerSecond: `${hashesPerSecond.toFixed(2)}${hashesPerSecondUnit}`,
        rollingHashesPerSecond: `${rollingHashesPerSecond.toFixed(
          2
        )}${rollingHashesPerSecondUnit}`,
        bestHashesPerSecond: `${bestHashesPerSecond.toFixed(
          2
        )}${bestHashesPerSecondUnit}`,
        arenaSetupTime: arenaSetupTime
      });
    };

    this.generationController.setListener(this.generationListener);
  }

  _startUnityLib() {
    if (!fs.existsSync(this.options.walletPath)) {
      console.log(`create wallet folder ${this.options.walletPath}`);
      fs.mkdirSync(this.options.walletPath, { recursive: true });
    } else {
      console.log(`wallet folder ${this.options.walletPath} already exists`);
    }

    var staticFilterPath = "";
    var staticFilterOffset = -1;
    var staticFilterLength = -1;
    //Load static filter files here if/when we start using them for non spv clients.
    var spvMode = false;

    console.log(`init unity lib threaded`);
    this.libraryController.InitUnityLibThreaded(
      this.options.walletPath,
      staticFilterPath,
      staticFilterOffset,
      staticFilterLength,
      this.options.useTestnet,
      spvMode,
      this.libraryListener,
      this.options.extraArgs
    );
  }

  _setStateWhenCoreAndMainWindowReady() {
    if (!this.isCoreReady || !this.isMainWindowReady)
      return console.log(
        `isCoreReady: ${this.isCoreReady}, isMainWindowReady: ${this.isMainWindowReady} -> return`
      );
    console.log("_setStateWhenCoreAndMainWindowReady: start");

    console.log("GetBalance: start");
    let balance = this.walletController.GetBalance();
    console.log("GetBalance: end");

    store.dispatch("wallet/SET_WALLET_BALANCE", balance);

    console.log("_updateAccounts: start");
    this._updateAccounts();
    console.log("_updateAccounts: end");

    console.log("getActiveAccount: start");
    let activeAccount = this.accountsController.getActiveAccount();
    console.log("getActiveAccount: end");

    console.log("GetReceiveAddress: start");
    let receiveAddress = this.libraryController.GetReceiveAddress();
    console.log("GetReceiveAddress: end");

    console.log("getMutationHistory: start");
    let mutations = this.libraryController.getMutationHistory();
    console.log("getMutationHistory: end");

    store.dispatch("wallet/SET_ACTIVE_ACCOUNT", activeAccount);
    store.dispatch("wallet/SET_RECEIVE_ADDRESS", receiveAddress);
    store.dispatch("wallet/SET_MUTATIONS", mutations);
    store.dispatch("app/SET_CORE_READY");

    console.log("_setStateWhenCoreAndMainWindowReady: end");
  }

  _registerSignalHandlers() {
    let self = this;
    let libraryListener = this.libraryListener;
    let libraryController = this.libraryController;

    libraryListener.notifyCoreReady = function() {
      console.log("received: notifyCoreReady");

      self._initializeWalletController();
      self._initializeAccountsController();
      self._initializeGenerationController();

      self.isCoreReady = true;
      self._setStateWhenCoreAndMainWindowReady();
    };

    libraryListener.logPrint = function(message) {
      console.log("unity_core: " + message);
    };

    libraryListener.notifyBalanceChange = function(new_balance) {
      console.log("received: notifyBalanceChange");
      store.dispatch("wallet/SET_BALANCE", new_balance);
    };

    libraryListener.notifyNewMutation = function(/*mutation, self_committed*/) {
      console.log("received: notifyNewMutation");
      store.dispatch(
        "wallet/SET_MUTATIONS",
        libraryController.getMutationHistory()
      );
      store.dispatch(
        "wallet/SET_RECEIVE_ADDRESS",
        libraryController.GetReceiveAddress()
      );

      self._updateAccounts();
    };

    libraryListener.notifyUpdatedTransaction = function(/*transaction*/) {
      console.log("received: notifyUpdatedTransaction");
      store.dispatch(
        "wallet/SET_MUTATIONS",
        libraryController.getMutationHistory()
      );
    };
    libraryListener.notifyInitWithExistingWallet = function() {
      console.log("received: notifyInitWithExistingWallet");
      store.dispatch("app/SET_WALLET_EXISTS", true);
    };

    libraryListener.notifyInitWithoutExistingWallet = function() {
      console.log("received: notifyInitWithoutExistingWallet");
      self.newRecoveryPhrase = libraryController.GenerateRecoveryMnemonic();
      store.dispatch("app/SET_WALLET_EXISTS", false);
    };

    libraryListener.notifyShutdown = function() {
      //NB! It is important to set the libraryListener to null here as this is the last thing the core lib is waiting on for clean exit; once we set this to null we are free to quit the app
      libraryListener = null;
      console.log("received: notifyShutdown");
      self.isTerminated = true;
      app.quit();
    };
  }

  _registerIpcHandlers() {
    // ipc for rpc controller
    ipc.answerRenderer("NJSIRpcController.ExecuteAsync", async data => {
      let rpcListener = new libUnity.NJSIRpcListener();

      return new Promise(resolve => {
        rpcListener.onSuccess = (filteredCommand, result) => {
          console.log(`RPC success: ${filteredCommand}`);
          resolve({
            result: {
              success: true,
              data: result,
              command: filteredCommand
            }
          });
        };

        rpcListener.onError = (filteredCommand, error) => {
          console.error(`RPC error: ${filteredCommand}`);
          resolve({
            result: {
              success: false,
              data: error,
              command: filteredCommand
            }
          });
        };

        this.rpcController.execute(data.command, rpcListener);
      });
    });

    ipc.on("NJSIRpcController.Execute", (event, command) => {
      let rpcListener = new libUnity.NJSIRpcListener();

      rpcListener.onSuccess = (filteredCommand, result) => {
        console.log(`RPC success: ${filteredCommand}`);
        event.returnValue = {
          result: {
            success: true,
            data: result,
            command: filteredCommand
          }
        };
      };

      rpcListener.onError = (filteredCommand, error) => {
        console.error(`RPC error: ${filteredCommand}`);
        event.returnValue = {
          result: {
            success: false,
            data: error,
            command: filteredCommand
          }
        };
      };

      this.rpcController.execute(command, rpcListener);
    });

    /* inject:generated-code */
    // Register NJSILibraryController ipc handlers
    ipc.answerRenderer("NJSILibraryController.BuildInfoAsync", async () => {
      console.log(`IPC: libraryController.BuildInfoAsync()`);
      try {
        let result = this.libraryController.BuildInfo();
        return {
          success: true,
          result: result
        };
      } catch (e) {
        return handleError(e);
      }
    });

    ipc.on("NJSILibraryController.BuildInfo", event => {
      console.log(`IPC: libraryController.BuildInfo()`);
      try {
        let result = this.libraryController.BuildInfo();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.InitWalletFromRecoveryPhraseAsync",
      async data => {
        console.log(
          `IPC: libraryController.InitWalletFromRecoveryPhraseAsync(${data.phrase}, ${data.password})`
        );
        try {
          let result = this.libraryController.InitWalletFromRecoveryPhrase(
            data.phrase,
            data.password
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSILibraryController.InitWalletFromRecoveryPhrase",
      (event, phrase, password) => {
        console.log(
          `IPC: libraryController.InitWalletFromRecoveryPhrase(${phrase}, ${password})`
        );
        try {
          let result = this.libraryController.InitWalletFromRecoveryPhrase(
            phrase,
            password
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSILibraryController.IsValidLinkURIAsync",
      async data => {
        console.log(
          `IPC: libraryController.IsValidLinkURIAsync(${data.phrase})`
        );
        try {
          let result = this.libraryController.IsValidLinkURI(data.phrase);
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.IsValidLinkURI", (event, phrase) => {
      console.log(`IPC: libraryController.IsValidLinkURI(${phrase})`);
      try {
        let result = this.libraryController.IsValidLinkURI(phrase);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.ReplaceWalletLinkedFromURIAsync",
      async data => {
        console.log(
          `IPC: libraryController.ReplaceWalletLinkedFromURIAsync(${data.linked_uri}, ${data.password})`
        );
        try {
          let result = this.libraryController.ReplaceWalletLinkedFromURI(
            data.linked_uri,
            data.password
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSILibraryController.ReplaceWalletLinkedFromURI",
      (event, linked_uri, password) => {
        console.log(
          `IPC: libraryController.ReplaceWalletLinkedFromURI(${linked_uri}, ${password})`
        );
        try {
          let result = this.libraryController.ReplaceWalletLinkedFromURI(
            linked_uri,
            password
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSILibraryController.EraseWalletSeedsAndAccountsAsync",
      async () => {
        console.log(
          `IPC: libraryController.EraseWalletSeedsAndAccountsAsync()`
        );
        try {
          let result = this.libraryController.EraseWalletSeedsAndAccounts();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.EraseWalletSeedsAndAccounts", event => {
      console.log(`IPC: libraryController.EraseWalletSeedsAndAccounts()`);
      try {
        let result = this.libraryController.EraseWalletSeedsAndAccounts();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.IsValidRecoveryPhraseAsync",
      async data => {
        console.log(
          `IPC: libraryController.IsValidRecoveryPhraseAsync(${data.phrase})`
        );
        try {
          let result = this.libraryController.IsValidRecoveryPhrase(
            data.phrase
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.IsValidRecoveryPhrase", (event, phrase) => {
      console.log(`IPC: libraryController.IsValidRecoveryPhrase(${phrase})`);
      try {
        let result = this.libraryController.IsValidRecoveryPhrase(phrase);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.GenerateRecoveryMnemonicAsync",
      async () => {
        console.log(`IPC: libraryController.GenerateRecoveryMnemonicAsync()`);
        try {
          let result = this.libraryController.GenerateRecoveryMnemonic();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.GenerateRecoveryMnemonic", event => {
      console.log(`IPC: libraryController.GenerateRecoveryMnemonic()`);
      try {
        let result = this.libraryController.GenerateRecoveryMnemonic();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.GenerateGenesisKeysAsync",
      async () => {
        console.log(`IPC: libraryController.GenerateGenesisKeysAsync()`);
        try {
          let result = this.libraryController.GenerateGenesisKeys();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.GenerateGenesisKeys", event => {
      console.log(`IPC: libraryController.GenerateGenesisKeys()`);
      try {
        let result = this.libraryController.GenerateGenesisKeys();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.ComposeRecoveryPhraseAsync",
      async data => {
        console.log(
          `IPC: libraryController.ComposeRecoveryPhraseAsync(${data.mnemonic}, ${data.birthTime})`
        );
        try {
          let result = this.libraryController.ComposeRecoveryPhrase(
            data.mnemonic,
            data.birthTime
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSILibraryController.ComposeRecoveryPhrase",
      (event, mnemonic, birthTime) => {
        console.log(
          `IPC: libraryController.ComposeRecoveryPhrase(${mnemonic}, ${birthTime})`
        );
        try {
          let result = this.libraryController.ComposeRecoveryPhrase(
            mnemonic,
            birthTime
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSILibraryController.TerminateUnityLibAsync",
      async () => {
        console.log(`IPC: libraryController.TerminateUnityLibAsync()`);
        try {
          let result = this.libraryController.TerminateUnityLib();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.TerminateUnityLib", event => {
      console.log(`IPC: libraryController.TerminateUnityLib()`);
      try {
        let result = this.libraryController.TerminateUnityLib();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.QRImageFromStringAsync",
      async data => {
        console.log(
          `IPC: libraryController.QRImageFromStringAsync(${data.qr_string}, ${data.width_hint})`
        );
        try {
          let result = this.libraryController.QRImageFromString(
            data.qr_string,
            data.width_hint
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSILibraryController.QRImageFromString",
      (event, qr_string, width_hint) => {
        console.log(
          `IPC: libraryController.QRImageFromString(${qr_string}, ${width_hint})`
        );
        try {
          let result = this.libraryController.QRImageFromString(
            qr_string,
            width_hint
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSILibraryController.GetReceiveAddressAsync",
      async () => {
        console.log(`IPC: libraryController.GetReceiveAddressAsync()`);
        try {
          let result = this.libraryController.GetReceiveAddress();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.GetReceiveAddress", event => {
      console.log(`IPC: libraryController.GetReceiveAddress()`);
      try {
        let result = this.libraryController.GetReceiveAddress();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.GetRecoveryPhraseAsync",
      async () => {
        console.log(`IPC: libraryController.GetRecoveryPhraseAsync()`);
        try {
          let result = this.libraryController.GetRecoveryPhrase();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.GetRecoveryPhrase", event => {
      console.log(`IPC: libraryController.GetRecoveryPhrase()`);
      try {
        let result = this.libraryController.GetRecoveryPhrase();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.IsMnemonicWalletAsync",
      async () => {
        console.log(`IPC: libraryController.IsMnemonicWalletAsync()`);
        try {
          let result = this.libraryController.IsMnemonicWallet();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.IsMnemonicWallet", event => {
      console.log(`IPC: libraryController.IsMnemonicWallet()`);
      try {
        let result = this.libraryController.IsMnemonicWallet();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.IsMnemonicCorrectAsync",
      async data => {
        console.log(
          `IPC: libraryController.IsMnemonicCorrectAsync(${data.phrase})`
        );
        try {
          let result = this.libraryController.IsMnemonicCorrect(data.phrase);
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.IsMnemonicCorrect", (event, phrase) => {
      console.log(`IPC: libraryController.IsMnemonicCorrect(${phrase})`);
      try {
        let result = this.libraryController.IsMnemonicCorrect(phrase);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.GetMnemonicDictionaryAsync",
      async () => {
        console.log(`IPC: libraryController.GetMnemonicDictionaryAsync()`);
        try {
          let result = this.libraryController.GetMnemonicDictionary();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.GetMnemonicDictionary", event => {
      console.log(`IPC: libraryController.GetMnemonicDictionary()`);
      try {
        let result = this.libraryController.GetMnemonicDictionary();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.UnlockWalletAsync",
      async data => {
        console.log(
          `IPC: libraryController.UnlockWalletAsync(${data.password})`
        );
        try {
          let result = this.libraryController.UnlockWallet(data.password);
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.UnlockWallet", (event, password) => {
      console.log(`IPC: libraryController.UnlockWallet(${password})`);
      try {
        let result = this.libraryController.UnlockWallet(password);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer("NJSILibraryController.LockWalletAsync", async () => {
      console.log(`IPC: libraryController.LockWalletAsync()`);
      try {
        let result = this.libraryController.LockWallet();
        return {
          success: true,
          result: result
        };
      } catch (e) {
        return handleError(e);
      }
    });

    ipc.on("NJSILibraryController.LockWallet", event => {
      console.log(`IPC: libraryController.LockWallet()`);
      try {
        let result = this.libraryController.LockWallet();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.IsWalletLockedAsync",
      async () => {
        console.log(`IPC: libraryController.IsWalletLockedAsync()`);
        try {
          let result = this.libraryController.IsWalletLocked();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.IsWalletLocked", event => {
      console.log(`IPC: libraryController.IsWalletLocked()`);
      try {
        let result = this.libraryController.IsWalletLocked();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.ChangePasswordAsync",
      async data => {
        console.log(
          `IPC: libraryController.ChangePasswordAsync(${data.oldPassword}, ${data.newPassword})`
        );
        try {
          let result = this.libraryController.ChangePassword(
            data.oldPassword,
            data.newPassword
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSILibraryController.ChangePassword",
      (event, oldPassword, newPassword) => {
        console.log(
          `IPC: libraryController.ChangePassword(${oldPassword}, ${newPassword})`
        );
        try {
          let result = this.libraryController.ChangePassword(
            oldPassword,
            newPassword
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer("NJSILibraryController.DoRescanAsync", async () => {
      console.log(`IPC: libraryController.DoRescanAsync()`);
      try {
        let result = this.libraryController.DoRescan();
        return {
          success: true,
          result: result
        };
      } catch (e) {
        return handleError(e);
      }
    });

    ipc.on("NJSILibraryController.DoRescan", event => {
      console.log(`IPC: libraryController.DoRescan()`);
      try {
        let result = this.libraryController.DoRescan();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.IsValidRecipientAsync",
      async data => {
        console.log(
          `IPC: libraryController.IsValidRecipientAsync(${data.request})`
        );
        try {
          let result = this.libraryController.IsValidRecipient(data.request);
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.IsValidRecipient", (event, request) => {
      console.log(`IPC: libraryController.IsValidRecipient(${request})`);
      try {
        let result = this.libraryController.IsValidRecipient(request);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.IsValidNativeAddressAsync",
      async data => {
        console.log(
          `IPC: libraryController.IsValidNativeAddressAsync(${data.address})`
        );
        try {
          let result = this.libraryController.IsValidNativeAddress(
            data.address
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.IsValidNativeAddress", (event, address) => {
      console.log(`IPC: libraryController.IsValidNativeAddress(${address})`);
      try {
        let result = this.libraryController.IsValidNativeAddress(address);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.IsValidBitcoinAddressAsync",
      async data => {
        console.log(
          `IPC: libraryController.IsValidBitcoinAddressAsync(${data.address})`
        );
        try {
          let result = this.libraryController.IsValidBitcoinAddress(
            data.address
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.IsValidBitcoinAddress", (event, address) => {
      console.log(`IPC: libraryController.IsValidBitcoinAddress(${address})`);
      try {
        let result = this.libraryController.IsValidBitcoinAddress(address);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.feeForRecipientAsync",
      async data => {
        console.log(
          `IPC: libraryController.feeForRecipientAsync(${data.request})`
        );
        try {
          let result = this.libraryController.feeForRecipient(data.request);
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.feeForRecipient", (event, request) => {
      console.log(`IPC: libraryController.feeForRecipient(${request})`);
      try {
        let result = this.libraryController.feeForRecipient(request);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.performPaymentToRecipientAsync",
      async data => {
        console.log(
          `IPC: libraryController.performPaymentToRecipientAsync(${data.request}, ${data.substract_fee})`
        );
        try {
          let result = this.libraryController.performPaymentToRecipient(
            data.request,
            data.substract_fee
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSILibraryController.performPaymentToRecipient",
      (event, request, substract_fee) => {
        console.log(
          `IPC: libraryController.performPaymentToRecipient(${request}, ${substract_fee})`
        );
        try {
          let result = this.libraryController.performPaymentToRecipient(
            request,
            substract_fee
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSILibraryController.getTransactionAsync",
      async data => {
        console.log(
          `IPC: libraryController.getTransactionAsync(${data.txHash})`
        );
        try {
          let result = this.libraryController.getTransaction(data.txHash);
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.getTransaction", (event, txHash) => {
      console.log(`IPC: libraryController.getTransaction(${txHash})`);
      try {
        let result = this.libraryController.getTransaction(txHash);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.resendTransactionAsync",
      async data => {
        console.log(
          `IPC: libraryController.resendTransactionAsync(${data.txHash})`
        );
        try {
          let result = this.libraryController.resendTransaction(data.txHash);
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.resendTransaction", (event, txHash) => {
      console.log(`IPC: libraryController.resendTransaction(${txHash})`);
      try {
        let result = this.libraryController.resendTransaction(txHash);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.getAddressBookRecordsAsync",
      async () => {
        console.log(`IPC: libraryController.getAddressBookRecordsAsync()`);
        try {
          let result = this.libraryController.getAddressBookRecords();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.getAddressBookRecords", event => {
      console.log(`IPC: libraryController.getAddressBookRecords()`);
      try {
        let result = this.libraryController.getAddressBookRecords();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.addAddressBookRecordAsync",
      async data => {
        console.log(
          `IPC: libraryController.addAddressBookRecordAsync(${data.address})`
        );
        try {
          let result = this.libraryController.addAddressBookRecord(
            data.address
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.addAddressBookRecord", (event, address) => {
      console.log(`IPC: libraryController.addAddressBookRecord(${address})`);
      try {
        let result = this.libraryController.addAddressBookRecord(address);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.deleteAddressBookRecordAsync",
      async data => {
        console.log(
          `IPC: libraryController.deleteAddressBookRecordAsync(${data.address})`
        );
        try {
          let result = this.libraryController.deleteAddressBookRecord(
            data.address
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSILibraryController.deleteAddressBookRecord",
      (event, address) => {
        console.log(
          `IPC: libraryController.deleteAddressBookRecord(${address})`
        );
        try {
          let result = this.libraryController.deleteAddressBookRecord(address);
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSILibraryController.ResetUnifiedProgressAsync",
      async () => {
        console.log(`IPC: libraryController.ResetUnifiedProgressAsync()`);
        try {
          let result = this.libraryController.ResetUnifiedProgress();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.ResetUnifiedProgress", event => {
      console.log(`IPC: libraryController.ResetUnifiedProgress()`);
      try {
        let result = this.libraryController.ResetUnifiedProgress();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.getLastSPVBlockInfosAsync",
      async () => {
        console.log(`IPC: libraryController.getLastSPVBlockInfosAsync()`);
        try {
          let result = this.libraryController.getLastSPVBlockInfos();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.getLastSPVBlockInfos", event => {
      console.log(`IPC: libraryController.getLastSPVBlockInfos()`);
      try {
        let result = this.libraryController.getLastSPVBlockInfos();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.getUnifiedProgressAsync",
      async () => {
        console.log(`IPC: libraryController.getUnifiedProgressAsync()`);
        try {
          let result = this.libraryController.getUnifiedProgress();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.getUnifiedProgress", event => {
      console.log(`IPC: libraryController.getUnifiedProgress()`);
      try {
        let result = this.libraryController.getUnifiedProgress();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.getMonitoringStatsAsync",
      async () => {
        console.log(`IPC: libraryController.getMonitoringStatsAsync()`);
        try {
          let result = this.libraryController.getMonitoringStats();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSILibraryController.getMonitoringStats", event => {
      console.log(`IPC: libraryController.getMonitoringStats()`);
      try {
        let result = this.libraryController.getMonitoringStats();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSILibraryController.RegisterMonitorListenerAsync",
      async data => {
        console.log(
          `IPC: libraryController.RegisterMonitorListenerAsync(${data.listener})`
        );
        try {
          let result = this.libraryController.RegisterMonitorListener(
            data.listener
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSILibraryController.RegisterMonitorListener",
      (event, listener) => {
        console.log(
          `IPC: libraryController.RegisterMonitorListener(${listener})`
        );
        try {
          let result = this.libraryController.RegisterMonitorListener(listener);
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSILibraryController.UnregisterMonitorListenerAsync",
      async data => {
        console.log(
          `IPC: libraryController.UnregisterMonitorListenerAsync(${data.listener})`
        );
        try {
          let result = this.libraryController.UnregisterMonitorListener(
            data.listener
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSILibraryController.UnregisterMonitorListener",
      (event, listener) => {
        console.log(
          `IPC: libraryController.UnregisterMonitorListener(${listener})`
        );
        try {
          let result = this.libraryController.UnregisterMonitorListener(
            listener
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer("NJSILibraryController.getClientInfoAsync", async () => {
      console.log(`IPC: libraryController.getClientInfoAsync()`);
      try {
        let result = this.libraryController.getClientInfo();
        return {
          success: true,
          result: result
        };
      } catch (e) {
        return handleError(e);
      }
    });

    ipc.on("NJSILibraryController.getClientInfo", event => {
      console.log(`IPC: libraryController.getClientInfo()`);
      try {
        let result = this.libraryController.getClientInfo();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    // Register NJSIWalletController ipc handlers
    ipc.answerRenderer(
      "NJSIWalletController.HaveUnconfirmedFundsAsync",
      async () => {
        console.log(`IPC: walletController.HaveUnconfirmedFundsAsync()`);
        try {
          let result = this.walletController.HaveUnconfirmedFunds();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIWalletController.HaveUnconfirmedFunds", event => {
      console.log(`IPC: walletController.HaveUnconfirmedFunds()`);
      try {
        let result = this.walletController.HaveUnconfirmedFunds();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIWalletController.GetBalanceSimpleAsync",
      async () => {
        console.log(`IPC: walletController.GetBalanceSimpleAsync()`);
        try {
          let result = this.walletController.GetBalanceSimple();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIWalletController.GetBalanceSimple", event => {
      console.log(`IPC: walletController.GetBalanceSimple()`);
      try {
        let result = this.walletController.GetBalanceSimple();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer("NJSIWalletController.GetBalanceAsync", async () => {
      console.log(`IPC: walletController.GetBalanceAsync()`);
      try {
        let result = this.walletController.GetBalance();
        return {
          success: true,
          result: result
        };
      } catch (e) {
        return handleError(e);
      }
    });

    ipc.on("NJSIWalletController.GetBalance", event => {
      console.log(`IPC: walletController.GetBalance()`);
      try {
        let result = this.walletController.GetBalance();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIWalletController.AbandonTransactionAsync",
      async data => {
        console.log(
          `IPC: walletController.AbandonTransactionAsync(${data.txHash})`
        );
        try {
          let result = this.walletController.AbandonTransaction(data.txHash);
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIWalletController.AbandonTransaction", (event, txHash) => {
      console.log(`IPC: walletController.AbandonTransaction(${txHash})`);
      try {
        let result = this.walletController.AbandonTransaction(txHash);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer("NJSIWalletController.GetUUIDAsync", async () => {
      console.log(`IPC: walletController.GetUUIDAsync()`);
      try {
        let result = this.walletController.GetUUID();
        return {
          success: true,
          result: result
        };
      } catch (e) {
        return handleError(e);
      }
    });

    ipc.on("NJSIWalletController.GetUUID", event => {
      console.log(`IPC: walletController.GetUUID()`);
      try {
        let result = this.walletController.GetUUID();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    // Register NJSIRpcController ipc handlers
    ipc.answerRenderer(
      "NJSIRpcController.getAutocompleteListAsync",
      async () => {
        console.log(`IPC: rpcController.getAutocompleteListAsync()`);
        try {
          let result = this.rpcController.getAutocompleteList();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIRpcController.getAutocompleteList", event => {
      console.log(`IPC: rpcController.getAutocompleteList()`);
      try {
        let result = this.rpcController.getAutocompleteList();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    // Register NJSIP2pNetworkController ipc handlers
    ipc.answerRenderer(
      "NJSIP2pNetworkController.disableNetworkAsync",
      async () => {
        console.log(`IPC: p2pNetworkController.disableNetworkAsync()`);
        try {
          let result = this.p2pNetworkController.disableNetwork();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIP2pNetworkController.disableNetwork", event => {
      console.log(`IPC: p2pNetworkController.disableNetwork()`);
      try {
        let result = this.p2pNetworkController.disableNetwork();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIP2pNetworkController.enableNetworkAsync",
      async () => {
        console.log(`IPC: p2pNetworkController.enableNetworkAsync()`);
        try {
          let result = this.p2pNetworkController.enableNetwork();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIP2pNetworkController.enableNetwork", event => {
      console.log(`IPC: p2pNetworkController.enableNetwork()`);
      try {
        let result = this.p2pNetworkController.enableNetwork();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIP2pNetworkController.getPeerInfoAsync",
      async () => {
        console.log(`IPC: p2pNetworkController.getPeerInfoAsync()`);
        try {
          let result = this.p2pNetworkController.getPeerInfo();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIP2pNetworkController.getPeerInfo", event => {
      console.log(`IPC: p2pNetworkController.getPeerInfo()`);
      try {
        let result = this.p2pNetworkController.getPeerInfo();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    // Register NJSIAccountsController ipc handlers
    ipc.answerRenderer("NJSIAccountsController.listAccountsAsync", async () => {
      console.log(`IPC: accountsController.listAccountsAsync()`);
      try {
        let result = this.accountsController.listAccounts();
        return {
          success: true,
          result: result
        };
      } catch (e) {
        return handleError(e);
      }
    });

    ipc.on("NJSIAccountsController.listAccounts", event => {
      console.log(`IPC: accountsController.listAccounts()`);
      try {
        let result = this.accountsController.listAccounts();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIAccountsController.setActiveAccountAsync",
      async data => {
        console.log(
          `IPC: accountsController.setActiveAccountAsync(${data.accountUUID})`
        );
        try {
          let result = this.accountsController.setActiveAccount(
            data.accountUUID
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIAccountsController.setActiveAccount", (event, accountUUID) => {
      console.log(`IPC: accountsController.setActiveAccount(${accountUUID})`);
      try {
        let result = this.accountsController.setActiveAccount(accountUUID);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIAccountsController.getActiveAccountAsync",
      async () => {
        console.log(`IPC: accountsController.getActiveAccountAsync()`);
        try {
          let result = this.accountsController.getActiveAccount();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIAccountsController.getActiveAccount", event => {
      console.log(`IPC: accountsController.getActiveAccount()`);
      try {
        let result = this.accountsController.getActiveAccount();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIAccountsController.createAccountAsync",
      async data => {
        console.log(
          `IPC: accountsController.createAccountAsync(${data.accountName}, ${data.accountType})`
        );
        try {
          let result = this.accountsController.createAccount(
            data.accountName,
            data.accountType
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSIAccountsController.createAccount",
      (event, accountName, accountType) => {
        console.log(
          `IPC: accountsController.createAccount(${accountName}, ${accountType})`
        );
        try {
          let result = this.accountsController.createAccount(
            accountName,
            accountType
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSIAccountsController.getAccountNameAsync",
      async data => {
        console.log(
          `IPC: accountsController.getAccountNameAsync(${data.accountUUID})`
        );
        try {
          let result = this.accountsController.getAccountName(data.accountUUID);
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIAccountsController.getAccountName", (event, accountUUID) => {
      console.log(`IPC: accountsController.getAccountName(${accountUUID})`);
      try {
        let result = this.accountsController.getAccountName(accountUUID);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIAccountsController.renameAccountAsync",
      async data => {
        console.log(
          `IPC: accountsController.renameAccountAsync(${data.accountUUID}, ${data.newAccountName})`
        );
        try {
          let result = this.accountsController.renameAccount(
            data.accountUUID,
            data.newAccountName
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSIAccountsController.renameAccount",
      (event, accountUUID, newAccountName) => {
        console.log(
          `IPC: accountsController.renameAccount(${accountUUID}, ${newAccountName})`
        );
        try {
          let result = this.accountsController.renameAccount(
            accountUUID,
            newAccountName
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSIAccountsController.deleteAccountAsync",
      async data => {
        console.log(
          `IPC: accountsController.deleteAccountAsync(${data.accountUUID})`
        );
        try {
          let result = this.accountsController.deleteAccount(data.accountUUID);
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIAccountsController.deleteAccount", (event, accountUUID) => {
      console.log(`IPC: accountsController.deleteAccount(${accountUUID})`);
      try {
        let result = this.accountsController.deleteAccount(accountUUID);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIAccountsController.purgeAccountAsync",
      async data => {
        console.log(
          `IPC: accountsController.purgeAccountAsync(${data.accountUUID})`
        );
        try {
          let result = this.accountsController.purgeAccount(data.accountUUID);
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIAccountsController.purgeAccount", (event, accountUUID) => {
      console.log(`IPC: accountsController.purgeAccount(${accountUUID})`);
      try {
        let result = this.accountsController.purgeAccount(accountUUID);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIAccountsController.getAccountLinkURIAsync",
      async data => {
        console.log(
          `IPC: accountsController.getAccountLinkURIAsync(${data.accountUUID})`
        );
        try {
          let result = this.accountsController.getAccountLinkURI(
            data.accountUUID
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIAccountsController.getAccountLinkURI", (event, accountUUID) => {
      console.log(`IPC: accountsController.getAccountLinkURI(${accountUUID})`);
      try {
        let result = this.accountsController.getAccountLinkURI(accountUUID);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIAccountsController.getWitnessKeyURIAsync",
      async data => {
        console.log(
          `IPC: accountsController.getWitnessKeyURIAsync(${data.accountUUID})`
        );
        try {
          let result = this.accountsController.getWitnessKeyURI(
            data.accountUUID
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIAccountsController.getWitnessKeyURI", (event, accountUUID) => {
      console.log(`IPC: accountsController.getWitnessKeyURI(${accountUUID})`);
      try {
        let result = this.accountsController.getWitnessKeyURI(accountUUID);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIAccountsController.createAccountFromWitnessKeyURIAsync",
      async data => {
        console.log(
          `IPC: accountsController.createAccountFromWitnessKeyURIAsync(${data.witnessKeyURI}, ${data.newAccountName})`
        );
        try {
          let result = this.accountsController.createAccountFromWitnessKeyURI(
            data.witnessKeyURI,
            data.newAccountName
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSIAccountsController.createAccountFromWitnessKeyURI",
      (event, witnessKeyURI, newAccountName) => {
        console.log(
          `IPC: accountsController.createAccountFromWitnessKeyURI(${witnessKeyURI}, ${newAccountName})`
        );
        try {
          let result = this.accountsController.createAccountFromWitnessKeyURI(
            witnessKeyURI,
            newAccountName
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSIAccountsController.getReceiveAddressAsync",
      async data => {
        console.log(
          `IPC: accountsController.getReceiveAddressAsync(${data.accountUUID})`
        );
        try {
          let result = this.accountsController.getReceiveAddress(
            data.accountUUID
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIAccountsController.getReceiveAddress", (event, accountUUID) => {
      console.log(`IPC: accountsController.getReceiveAddress(${accountUUID})`);
      try {
        let result = this.accountsController.getReceiveAddress(accountUUID);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIAccountsController.getTransactionHistoryAsync",
      async data => {
        console.log(
          `IPC: accountsController.getTransactionHistoryAsync(${data.accountUUID})`
        );
        try {
          let result = this.accountsController.getTransactionHistory(
            data.accountUUID
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSIAccountsController.getTransactionHistory",
      (event, accountUUID) => {
        console.log(
          `IPC: accountsController.getTransactionHistory(${accountUUID})`
        );
        try {
          let result = this.accountsController.getTransactionHistory(
            accountUUID
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSIAccountsController.getMutationHistoryAsync",
      async data => {
        console.log(
          `IPC: accountsController.getMutationHistoryAsync(${data.accountUUID})`
        );
        try {
          let result = this.accountsController.getMutationHistory(
            data.accountUUID
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSIAccountsController.getMutationHistory",
      (event, accountUUID) => {
        console.log(
          `IPC: accountsController.getMutationHistory(${accountUUID})`
        );
        try {
          let result = this.accountsController.getMutationHistory(accountUUID);
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSIAccountsController.getActiveAccountBalanceAsync",
      async () => {
        console.log(`IPC: accountsController.getActiveAccountBalanceAsync()`);
        try {
          let result = this.accountsController.getActiveAccountBalance();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIAccountsController.getActiveAccountBalance", event => {
      console.log(`IPC: accountsController.getActiveAccountBalance()`);
      try {
        let result = this.accountsController.getActiveAccountBalance();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIAccountsController.getAccountBalanceAsync",
      async data => {
        console.log(
          `IPC: accountsController.getAccountBalanceAsync(${data.accountUUID})`
        );
        try {
          let result = this.accountsController.getAccountBalance(
            data.accountUUID
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIAccountsController.getAccountBalance", (event, accountUUID) => {
      console.log(`IPC: accountsController.getAccountBalance(${accountUUID})`);
      try {
        let result = this.accountsController.getAccountBalance(accountUUID);
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIAccountsController.getAllAccountBalancesAsync",
      async () => {
        console.log(`IPC: accountsController.getAllAccountBalancesAsync()`);
        try {
          let result = this.accountsController.getAllAccountBalances();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIAccountsController.getAllAccountBalances", event => {
      console.log(`IPC: accountsController.getAllAccountBalances()`);
      try {
        let result = this.accountsController.getAllAccountBalances();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    // Register NJSIWitnessController ipc handlers
    ipc.answerRenderer(
      "NJSIWitnessController.getNetworkLimitsAsync",
      async () => {
        console.log(`IPC: witnessController.getNetworkLimitsAsync()`);
        try {
          let result = this.witnessController.getNetworkLimits();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIWitnessController.getNetworkLimits", event => {
      console.log(`IPC: witnessController.getNetworkLimits()`);
      try {
        let result = this.witnessController.getNetworkLimits();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIWitnessController.getEstimatedWeightAsync",
      async data => {
        console.log(
          `IPC: witnessController.getEstimatedWeightAsync(${data.amount_to_lock}, ${data.lock_period_in_blocks})`
        );
        try {
          let result = this.witnessController.getEstimatedWeight(
            data.amount_to_lock,
            data.lock_period_in_blocks
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSIWitnessController.getEstimatedWeight",
      (event, amount_to_lock, lock_period_in_blocks) => {
        console.log(
          `IPC: witnessController.getEstimatedWeight(${amount_to_lock}, ${lock_period_in_blocks})`
        );
        try {
          let result = this.witnessController.getEstimatedWeight(
            amount_to_lock,
            lock_period_in_blocks
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSIWitnessController.fundWitnessAccountAsync",
      async data => {
        console.log(
          `IPC: witnessController.fundWitnessAccountAsync(${data.funding_account_UUID}, ${data.witness_account_UUID}, ${data.funding_amount}, ${data.requestedLockPeriodInBlocks})`
        );
        try {
          let result = this.witnessController.fundWitnessAccount(
            data.funding_account_UUID,
            data.witness_account_UUID,
            data.funding_amount,
            data.requestedLockPeriodInBlocks
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSIWitnessController.fundWitnessAccount",
      (
        event,
        funding_account_UUID,
        witness_account_UUID,
        funding_amount,
        requestedLockPeriodInBlocks
      ) => {
        console.log(
          `IPC: witnessController.fundWitnessAccount(${funding_account_UUID}, ${witness_account_UUID}, ${funding_amount}, ${requestedLockPeriodInBlocks})`
        );
        try {
          let result = this.witnessController.fundWitnessAccount(
            funding_account_UUID,
            witness_account_UUID,
            funding_amount,
            requestedLockPeriodInBlocks
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSIWitnessController.renewWitnessAccountAsync",
      async data => {
        console.log(
          `IPC: witnessController.renewWitnessAccountAsync(${data.funding_account_UUID}, ${data.witness_account_UUID})`
        );
        try {
          let result = this.witnessController.renewWitnessAccount(
            data.funding_account_UUID,
            data.witness_account_UUID
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSIWitnessController.renewWitnessAccount",
      (event, funding_account_UUID, witness_account_UUID) => {
        console.log(
          `IPC: witnessController.renewWitnessAccount(${funding_account_UUID}, ${witness_account_UUID})`
        );
        try {
          let result = this.witnessController.renewWitnessAccount(
            funding_account_UUID,
            witness_account_UUID
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSIWitnessController.getAccountWitnessStatisticsAsync",
      async data => {
        console.log(
          `IPC: witnessController.getAccountWitnessStatisticsAsync(${data.witnessAccountUUID})`
        );
        try {
          let result = this.witnessController.getAccountWitnessStatistics(
            data.witnessAccountUUID
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSIWitnessController.getAccountWitnessStatistics",
      (event, witnessAccountUUID) => {
        console.log(
          `IPC: witnessController.getAccountWitnessStatistics(${witnessAccountUUID})`
        );
        try {
          let result = this.witnessController.getAccountWitnessStatistics(
            witnessAccountUUID
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSIWitnessController.setAccountCompoundingAsync",
      async data => {
        console.log(
          `IPC: witnessController.setAccountCompoundingAsync(${data.witnessAccountUUID}, ${data.should_compound})`
        );
        try {
          let result = this.witnessController.setAccountCompounding(
            data.witnessAccountUUID,
            data.should_compound
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSIWitnessController.setAccountCompounding",
      (event, witnessAccountUUID, should_compound) => {
        console.log(
          `IPC: witnessController.setAccountCompounding(${witnessAccountUUID}, ${should_compound})`
        );
        try {
          let result = this.witnessController.setAccountCompounding(
            witnessAccountUUID,
            should_compound
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSIWitnessController.isAccountCompoundingAsync",
      async data => {
        console.log(
          `IPC: witnessController.isAccountCompoundingAsync(${data.witnessAccountUUID})`
        );
        try {
          let result = this.witnessController.isAccountCompounding(
            data.witnessAccountUUID
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSIWitnessController.isAccountCompounding",
      (event, witnessAccountUUID) => {
        console.log(
          `IPC: witnessController.isAccountCompounding(${witnessAccountUUID})`
        );
        try {
          let result = this.witnessController.isAccountCompounding(
            witnessAccountUUID
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    // Register NJSIGenerationController ipc handlers
    ipc.answerRenderer(
      "NJSIGenerationController.startGenerationAsync",
      async data => {
        console.log(
          `IPC: generationController.startGenerationAsync(${data.numThreads}, ${data.numArenaThreads}, ${data.memoryLimit})`
        );
        try {
          let result = this.generationController.startGeneration(
            data.numThreads,
            data.numArenaThreads,
            data.memoryLimit
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSIGenerationController.startGeneration",
      (event, numThreads, numArenaThreads, memoryLimit) => {
        console.log(
          `IPC: generationController.startGeneration(${numThreads}, ${numArenaThreads}, ${memoryLimit})`
        );
        try {
          let result = this.generationController.startGeneration(
            numThreads,
            numArenaThreads,
            memoryLimit
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSIGenerationController.stopGenerationAsync",
      async () => {
        console.log(`IPC: generationController.stopGenerationAsync()`);
        try {
          let result = this.generationController.stopGeneration();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIGenerationController.stopGeneration", event => {
      console.log(`IPC: generationController.stopGeneration()`);
      try {
        let result = this.generationController.stopGeneration();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIGenerationController.getGenerationAddressAsync",
      async () => {
        console.log(`IPC: generationController.getGenerationAddressAsync()`);
        try {
          let result = this.generationController.getGenerationAddress();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIGenerationController.getGenerationAddress", event => {
      console.log(`IPC: generationController.getGenerationAddress()`);
      try {
        let result = this.generationController.getGenerationAddress();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIGenerationController.getGenerationOverrideAddressAsync",
      async () => {
        console.log(
          `IPC: generationController.getGenerationOverrideAddressAsync()`
        );
        try {
          let result = this.generationController.getGenerationOverrideAddress();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIGenerationController.getGenerationOverrideAddress", event => {
      console.log(`IPC: generationController.getGenerationOverrideAddress()`);
      try {
        let result = this.generationController.getGenerationOverrideAddress();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIGenerationController.setGenerationOverrideAddressAsync",
      async data => {
        console.log(
          `IPC: generationController.setGenerationOverrideAddressAsync(${data.overrideAddress})`
        );
        try {
          let result = this.generationController.setGenerationOverrideAddress(
            data.overrideAddress
          );
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on(
      "NJSIGenerationController.setGenerationOverrideAddress",
      (event, overrideAddress) => {
        console.log(
          `IPC: generationController.setGenerationOverrideAddress(${overrideAddress})`
        );
        try {
          let result = this.generationController.setGenerationOverrideAddress(
            overrideAddress
          );
          event.returnValue = {
            success: true,
            result: result
          };
        } catch (e) {
          event.returnValue = handleError(e);
        }
      }
    );

    ipc.answerRenderer(
      "NJSIGenerationController.getAvailableCoresAsync",
      async () => {
        console.log(`IPC: generationController.getAvailableCoresAsync()`);
        try {
          let result = this.generationController.getAvailableCores();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIGenerationController.getAvailableCores", event => {
      console.log(`IPC: generationController.getAvailableCores()`);
      try {
        let result = this.generationController.getAvailableCores();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIGenerationController.getMinimumMemoryAsync",
      async () => {
        console.log(`IPC: generationController.getMinimumMemoryAsync()`);
        try {
          let result = this.generationController.getMinimumMemory();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIGenerationController.getMinimumMemory", event => {
      console.log(`IPC: generationController.getMinimumMemory()`);
      try {
        let result = this.generationController.getMinimumMemory();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.answerRenderer(
      "NJSIGenerationController.getMaximumMemoryAsync",
      async () => {
        console.log(`IPC: generationController.getMaximumMemoryAsync()`);
        try {
          let result = this.generationController.getMaximumMemory();
          return {
            success: true,
            result: result
          };
        } catch (e) {
          return handleError(e);
        }
      }
    );

    ipc.on("NJSIGenerationController.getMaximumMemory", event => {
      console.log(`IPC: generationController.getMaximumMemory()`);
      try {
        let result = this.generationController.getMaximumMemory();
        event.returnValue = {
          success: true,
          result: result
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });
    /* inject:generated-code */

    ipc.on("BackendUtilities.GetBuySessionUrl", async event => {
      console.log(`IPC: BackendUtilities.GetBuySessionUrl()`);
      try {
        var formData = new FormData();
        formData.append("address", store.state.wallet.receiveAddress);
        formData.append("currency", "florin");
        formData.append("wallettype", "pro");
        formData.append("uuid", this.walletController.GetUUID());

        let response = await axios.post(
          "https://www.blockhut.com/buysession.php",
          formData,
          {
            headers: formData.getHeaders()
          }
        );

        event.returnValue = {
          success: response.data.status_message === "OK",
          result: `https://blockhut.com/buyflorin.php?sessionid=${response.data.sessionid}`
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });

    ipc.on("BackendUtilities.GetSellSessionUrl", async event => {
      console.log(`IPC: BackendUtilities.GetSellSessionUrl()`);
      try {
        var formData = new FormData();
        formData.append("address", store.state.wallet.receiveAddress);
        formData.append("currency", "florin");
        formData.append("wallettype", "pro");
        formData.append("uuid", this.walletController.GetUUID());

        let response = await axios.post(
          "https://www.blockhut.com/buysession.php",
          formData,
          {
            headers: formData.getHeaders()
          }
        );

        event.returnValue = {
          success: response.data.status_message === "OK",
          result: `https://blockhut.com/sellflorin.php?sessionid=${response.data.sessionid}`
        };
      } catch (e) {
        event.returnValue = handleError(e);
      }
    });
  }
}

function handleError(error) {
  console.error(error);
  return {
    error: error,
    result: null
  };
}


export default LibUnity;
