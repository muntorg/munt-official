import { app, ipcMain as ipc } from "electron";
import fs from "fs";
import axios from "axios";
import FormData from "form-data";
import store from "../store";

import libUnity from "native-ext-loader!./lib_unity.node";

// Needed to get index/offset of staticfiltercp file inside asar
import disk from "asar/lib/disk";
import path from "path";
const isDevelopment = process.env.NODE_ENV !== "production";

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

      currentAccount.balance = FormatBalance(
        currentBalance.availableIncludingLocked +
          currentBalance.immatureIncludingLocked
      );
      currentAccount.spendable = FormatBalance(
        currentBalance.availableExcludingLocked -
          currentBalance.immatureExcludingLocked
      );
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
      store.dispatch("wallet/SET_ACTIVE_ACCOUNT", accountUUID);

      store.dispatch(
        "wallet/SET_MUTATIONS",
        libraryController.getMutationHistory()
      );

      store.dispatch(
        "wallet/SET_RECEIVE_ADDRESS",
        libraryController.GetReceiveAddress()
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

  _startUnityLib() {
    if (!fs.existsSync(this.options.walletPath)) {
      console.log(`create wallet folder ${this.options.walletPath}`);
      fs.mkdirSync(this.options.walletPath, { recursive: true });
    } else {
      console.log(`wallet folder ${this.options.walletPath} already exists`);
    }

    var staticFilterPath = "";
    var staticFilterOffset = 0;
    var staticFilterLength = 0;
    if (isDevelopment) {
      staticFilterPath = path.join(
        app.getAppPath(),
        "../../../data/staticfiltercp"
      );
    } else {
      staticFilterPath = app.getAppPath();
      if (staticFilterPath.endsWith(".asar")) {
        const filesystem = disk.readFilesystemSync(staticFilterPath);
        const fileInfo = filesystem.getFile("staticfiltercp", true);
        staticFilterOffset =
          parseInt(fileInfo.offset) +
          parseInt(8) +
          parseInt(filesystem.headerSize);
        staticFilterLength = parseInt(fileInfo.size);
      } else {
        staticFilterPath = path.join(staticFilterPath, "staticfiltercp");
      }
    }
    var spvMode = true;

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
    ipc.on("NJSIRpcController.Execute", (event, command) => {
      let rpcListener = new libUnity.NJSIRpcListener();

      rpcListener.onSuccess = (filteredCommand, result) => {
        console.log(`RPC success: ${result}`);
        event.returnValue = {
          result: {
            success: true,
            data: result,
            command: filteredCommand
          }
        };
      };

      rpcListener.onError = (filteredCommand, error) => {
        console.error(`RPC error: ${error}`);
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
    /* inject:generated-code */

    ipc.on("BackendUtilities.GetBuySessionUrl", async event => {
      console.log(`IPC: BackendUtilities.GetBuySessionUrl()`);
      try {
        var formData = new FormData();
        formData.append("address", store.state.wallet.receiveAddress);
        formData.append("currency", "gulden");
        formData.append("wallettype", "lite");
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
          result: `https://blockhut.com/buy.php?sessionid=${response.data.sessionid}`
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
        formData.append("currency", "gulden");
        formData.append("wallettype", "lite");
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
          result: `https://blockhut.com/sell.php?sessionid=${response.data.sessionid}`
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

function FormatBalance(balance) {
  return Math.floor(balance / 1000000) / 100;
}

export default LibUnity;
