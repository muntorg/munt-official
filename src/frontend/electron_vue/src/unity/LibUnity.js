import { app, ipcMain as ipc } from "electron";
import fs from "fs";

import store from "../store";

import libUnity from "native-ext-loader!./lib_unity.node";

class LibUnity {
  constructor(options) {
    this.initialized = false;
    this.isTerminated = false;

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

    store.dispatch({
      type: "SET_UNITY_VERSION",
      version: buildInfo.substr(1, buildInfo.indexOf("-") - 1)
    });
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
      store.dispatch({
        type: "SET_WALLET_BALANCE",
        walletBalance: new_balance
      });
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
    console.log("_updateAccounts");
    let accounts = this.accountsController.listAccounts();
    let accountBalances = this.accountsController.getAllAccountBalances();

    Object.keys(accountBalances).forEach(key => {
      accounts.find(x => x.UUID === key).balance =
        (accountBalances[key].availableIncludingLocked +
          accountBalances[key].immatureIncludingLocked) /
        100000000;
    });

    store.dispatch({
      type: "SET_ACCOUNTS",
      accounts: accounts
    });
  }

  _initializeAccountsController() {
    console.log("_initializeAccountsController");

    let self = this;
    let libraryController = this.libraryController;

    this.accountsListener.onAccountNameChanged = function(
      accountUUID,
      newAccountName
    ) {
      store.dispatch({ type: "SET_ACCOUNT_NAME", accountUUID, newAccountName });
    };

    this.accountsListener.onActiveAccountChanged = function(accountUUID) {
      store.dispatch({ type: "SET_ACTIVE_ACCOUNT", accountUUID });

      store.dispatch({
        type: "SET_MUTATIONS",
        mutations: libraryController.getMutationHistory()
      });

      store.dispatch({
        type: "SET_RECEIVE_ADDRESS",
        receiveAddress: libraryController.GetReceiveAddress()
      });
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
      store.dispatch({
        type: "SET_GENERATION_ACTIVE",
        generationActive: true
      });
    };

    this.generationListener.onGenerationStopped = function() {
      console.log("GENERATION STOPPED");
      store.dispatch({
        type: "SET_GENERATION_ACTIVE",
        generationActive: false
      });
      store.dispatch({
        type: "SET_GENERATION_STATS",
        generationStats: null
      });
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
      store.dispatch({
        type: "SET_GENERATION_STATS",
        generationStats: {
          hashesPerSecond: `${hashesPerSecond.toFixed(
            2
          )}${hashesPerSecondUnit}`,
          rollingHashesPerSecond: `${rollingHashesPerSecond.toFixed(
            2
          )}${rollingHashesPerSecondUnit}`,
          bestHashesPerSecond: `${bestHashesPerSecond.toFixed(
            2
          )}${bestHashesPerSecondUnit}`,
          arenaSetupTime: arenaSetupTime
        }
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

    console.log(`init unity lib threaded`);
    this.libraryController.InitUnityLibThreaded(
      this.options.walletPath,
      "",
      -1,
      -1,
      this.options.useTestnet,
      false, // non spv mode
      this.libraryListener,
      this.options.extraArgs
    );
  }

  _coreReady() {
    console.log("_coreReady");
    this._initializeWalletController();
    this._initializeAccountsController();
    this._initializeGenerationController();

    console.log("dispatch SET_WALLET_BALANCE");
    store.dispatch({
      type: "SET_WALLET_BALANCE",
      walletBalance: this.walletController.GetBalance()
    });

    this._updateAccounts();

    console.log("dispatch SET_ACTIVE_ACCOUNT");
    store.dispatch({
      type: "SET_ACTIVE_ACCOUNT",
      accountUUID: this.accountsController.getActiveAccount()
    });

    console.log("dispatch SET_RECEIVE_ADDRESS");
    store.dispatch({
      type: "SET_RECEIVE_ADDRESS",
      receiveAddress: this.libraryController.GetReceiveAddress()
    });

    console.log("dispatch SET_MUTATIONS");
    store.dispatch({
      type: "SET_MUTATIONS",
      mutations: this.libraryController.getMutationHistory()
    });

    console.log("dispatch SET_CORE_READY");
    store.dispatch({ type: "SET_CORE_READY", coreReady: true });
  }

  _registerSignalHandlers() {
    let self = this;
    let libraryListener = this.libraryListener;
    let libraryController = this.libraryController;

    libraryListener.notifyCoreReady = function() {
      console.log("received: notifyCoreReady");
      self._coreReady();
    };

    libraryListener.logPrint = function(message) {
      console.log("unity_core: " + message);
    };

    libraryListener.notifyUnifiedProgress = function(/*progress*/) {
      console.log("received: notifyUnifiedProgress");
      // todo: set progress property in store?
    };

    libraryListener.notifyBalanceChange = function(new_balance) {
      console.log("received: notifyBalanceChange");
      store.dispatch({ type: "SET_BALANCE", balance: new_balance });
    };

    libraryListener.notifyNewMutation = function(/*mutation, self_committed*/) {
      console.log("received: notifyNewMutation");

      store.dispatch({
        type: "SET_MUTATIONS",
        mutations: libraryController.getMutationHistory()
      });
      store.dispatch({
        type: "SET_RECEIVE_ADDRESS",
        receiveAddress: libraryController.GetReceiveAddress()
      });

      self._updateAccounts();
    };

    libraryListener.notifyUpdatedTransaction = function(/*transaction*/) {
      console.log("received: notifyUpdatedTransaction");
      store.dispatch({
        type: "SET_MUTATIONS",
        mutations: libraryController.getMutationHistory()
      });
    };

    libraryListener.notifyInitWithExistingWallet = function() {
      console.log("received: notifyInitWithExistingWallet");
      store.dispatch({ type: "SET_WALLET_EXISTS", walletExists: true });
    };

    libraryListener.notifyInitWithoutExistingWallet = function() {
      console.log("received: notifyInitWithoutExistingWallet");
      self.newRecoveryPhrase = libraryController.GenerateRecoveryMnemonic();
      store.dispatch({ type: "SET_WALLET_EXISTS", walletExists: false });
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
          success: true,
          data: result,
          command: filteredCommand
        };
      };

      rpcListener.onError = error => {
        console.error(`RPC error: ${error}`);
        event.returnValue = { success: false, data: error, command: command };
      };

      this.rpcController.execute(command, rpcListener);
    });

    /* inject:generated-code */
    // Register NJSILibraryController ipc handlers
    ipc.on("NJSILibraryController.BuildInfo", event => {
      console.log(`IPC: libraryController.BuildInfo()`);
      event.returnValue = this.libraryController.BuildInfo();
    });

    ipc.on(
      "NJSILibraryController.InitWalletFromRecoveryPhrase",
      (event, phrase, password) => {
        console.log(
          `IPC: libraryController.InitWalletFromRecoveryPhrase(${phrase}, ${password})`
        );
        event.returnValue = this.libraryController.InitWalletFromRecoveryPhrase(
          phrase,
          password
        );
      }
    );

    ipc.on("NJSILibraryController.IsValidLinkURI", (event, phrase) => {
      console.log(`IPC: libraryController.IsValidLinkURI(${phrase})`);
      event.returnValue = this.libraryController.IsValidLinkURI(phrase);
    });

    ipc.on(
      "NJSILibraryController.ReplaceWalletLinkedFromURI",
      (event, linked_uri, password) => {
        console.log(
          `IPC: libraryController.ReplaceWalletLinkedFromURI(${linked_uri}, ${password})`
        );
        event.returnValue = this.libraryController.ReplaceWalletLinkedFromURI(
          linked_uri,
          password
        );
      }
    );

    ipc.on("NJSILibraryController.EraseWalletSeedsAndAccounts", event => {
      console.log(`IPC: libraryController.EraseWalletSeedsAndAccounts()`);
      event.returnValue = this.libraryController.EraseWalletSeedsAndAccounts();
    });

    ipc.on("NJSILibraryController.IsValidRecoveryPhrase", (event, phrase) => {
      console.log(`IPC: libraryController.IsValidRecoveryPhrase(${phrase})`);
      event.returnValue = this.libraryController.IsValidRecoveryPhrase(phrase);
    });

    ipc.on("NJSILibraryController.GenerateRecoveryMnemonic", event => {
      console.log(`IPC: libraryController.GenerateRecoveryMnemonic()`);
      event.returnValue = this.libraryController.GenerateRecoveryMnemonic();
    });

    ipc.on("NJSILibraryController.GenerateGenesisKeys", event => {
      console.log(`IPC: libraryController.GenerateGenesisKeys()`);
      event.returnValue = this.libraryController.GenerateGenesisKeys();
    });

    ipc.on(
      "NJSILibraryController.ComposeRecoveryPhrase",
      (event, mnemonic, birthTime) => {
        console.log(
          `IPC: libraryController.ComposeRecoveryPhrase(${mnemonic}, ${birthTime})`
        );
        event.returnValue = this.libraryController.ComposeRecoveryPhrase(
          mnemonic,
          birthTime
        );
      }
    );

    ipc.on("NJSILibraryController.TerminateUnityLib", event => {
      console.log(`IPC: libraryController.TerminateUnityLib()`);
      event.returnValue = this.libraryController.TerminateUnityLib();
    });

    ipc.on(
      "NJSILibraryController.QRImageFromString",
      (event, qr_string, width_hint) => {
        console.log(
          `IPC: libraryController.QRImageFromString(${qr_string}, ${width_hint})`
        );
        event.returnValue = this.libraryController.QRImageFromString(
          qr_string,
          width_hint
        );
      }
    );

    ipc.on("NJSILibraryController.GetReceiveAddress", event => {
      console.log(`IPC: libraryController.GetReceiveAddress()`);
      event.returnValue = this.libraryController.GetReceiveAddress();
    });

    ipc.on("NJSILibraryController.GetRecoveryPhrase", event => {
      console.log(`IPC: libraryController.GetRecoveryPhrase()`);
      event.returnValue = this.libraryController.GetRecoveryPhrase();
    });

    ipc.on("NJSILibraryController.IsMnemonicWallet", event => {
      console.log(`IPC: libraryController.IsMnemonicWallet()`);
      event.returnValue = this.libraryController.IsMnemonicWallet();
    });

    ipc.on("NJSILibraryController.IsMnemonicCorrect", (event, phrase) => {
      console.log(`IPC: libraryController.IsMnemonicCorrect(${phrase})`);
      event.returnValue = this.libraryController.IsMnemonicCorrect(phrase);
    });

    ipc.on("NJSILibraryController.GetMnemonicDictionary", event => {
      console.log(`IPC: libraryController.GetMnemonicDictionary()`);
      event.returnValue = this.libraryController.GetMnemonicDictionary();
    });

    ipc.on("NJSILibraryController.UnlockWallet", (event, password) => {
      console.log(`IPC: libraryController.UnlockWallet(${password})`);
      event.returnValue = this.libraryController.UnlockWallet(password);
    });

    ipc.on("NJSILibraryController.LockWallet", event => {
      console.log(`IPC: libraryController.LockWallet()`);
      event.returnValue = this.libraryController.LockWallet();
    });

    ipc.on(
      "NJSILibraryController.ChangePassword",
      (event, oldPassword, newPassword) => {
        console.log(
          `IPC: libraryController.ChangePassword(${oldPassword}, ${newPassword})`
        );
        event.returnValue = this.libraryController.ChangePassword(
          oldPassword,
          newPassword
        );
      }
    );

    ipc.on("NJSILibraryController.DoRescan", event => {
      console.log(`IPC: libraryController.DoRescan()`);
      event.returnValue = this.libraryController.DoRescan();
    });

    ipc.on("NJSILibraryController.IsValidRecipient", (event, request) => {
      console.log(`IPC: libraryController.IsValidRecipient(${request})`);
      event.returnValue = this.libraryController.IsValidRecipient(request);
    });

    ipc.on("NJSILibraryController.IsValidNativeAddress", (event, address) => {
      console.log(`IPC: libraryController.IsValidNativeAddress(${address})`);
      event.returnValue = this.libraryController.IsValidNativeAddress(address);
    });

    ipc.on("NJSILibraryController.IsValidBitcoinAddress", (event, address) => {
      console.log(`IPC: libraryController.IsValidBitcoinAddress(${address})`);
      event.returnValue = this.libraryController.IsValidBitcoinAddress(address);
    });

    ipc.on("NJSILibraryController.feeForRecipient", (event, request) => {
      console.log(`IPC: libraryController.feeForRecipient(${request})`);
      event.returnValue = this.libraryController.feeForRecipient(request);
    });

    ipc.on(
      "NJSILibraryController.performPaymentToRecipient",
      (event, request, substract_fee) => {
        console.log(
          `IPC: libraryController.performPaymentToRecipient(${request}, ${substract_fee})`
        );
        event.returnValue = this.libraryController.performPaymentToRecipient(
          request,
          substract_fee
        );
      }
    );

    ipc.on("NJSILibraryController.getTransaction", (event, txHash) => {
      console.log(`IPC: libraryController.getTransaction(${txHash})`);
      event.returnValue = this.libraryController.getTransaction(txHash);
    });

    ipc.on("NJSILibraryController.resendTransaction", (event, txHash) => {
      console.log(`IPC: libraryController.resendTransaction(${txHash})`);
      event.returnValue = this.libraryController.resendTransaction(txHash);
    });

    ipc.on("NJSILibraryController.getAddressBookRecords", event => {
      console.log(`IPC: libraryController.getAddressBookRecords()`);
      event.returnValue = this.libraryController.getAddressBookRecords();
    });

    ipc.on("NJSILibraryController.addAddressBookRecord", (event, address) => {
      console.log(`IPC: libraryController.addAddressBookRecord(${address})`);
      event.returnValue = this.libraryController.addAddressBookRecord(address);
    });

    ipc.on(
      "NJSILibraryController.deleteAddressBookRecord",
      (event, address) => {
        console.log(
          `IPC: libraryController.deleteAddressBookRecord(${address})`
        );
        event.returnValue = this.libraryController.deleteAddressBookRecord(
          address
        );
      }
    );

    ipc.on("NJSILibraryController.ResetUnifiedProgress", event => {
      console.log(`IPC: libraryController.ResetUnifiedProgress()`);
      event.returnValue = this.libraryController.ResetUnifiedProgress();
    });

    ipc.on("NJSILibraryController.getLastSPVBlockInfos", event => {
      console.log(`IPC: libraryController.getLastSPVBlockInfos()`);
      event.returnValue = this.libraryController.getLastSPVBlockInfos();
    });

    ipc.on("NJSILibraryController.getUnifiedProgress", event => {
      console.log(`IPC: libraryController.getUnifiedProgress()`);
      event.returnValue = this.libraryController.getUnifiedProgress();
    });

    ipc.on("NJSILibraryController.getMonitoringStats", event => {
      console.log(`IPC: libraryController.getMonitoringStats()`);
      event.returnValue = this.libraryController.getMonitoringStats();
    });

    ipc.on(
      "NJSILibraryController.RegisterMonitorListener",
      (event, listener) => {
        console.log(
          `IPC: libraryController.RegisterMonitorListener(${listener})`
        );
        event.returnValue = this.libraryController.RegisterMonitorListener(
          listener
        );
      }
    );

    ipc.on(
      "NJSILibraryController.UnregisterMonitorListener",
      (event, listener) => {
        console.log(
          `IPC: libraryController.UnregisterMonitorListener(${listener})`
        );
        event.returnValue = this.libraryController.UnregisterMonitorListener(
          listener
        );
      }
    );

    ipc.on("NJSILibraryController.getClientInfo", event => {
      console.log(`IPC: libraryController.getClientInfo()`);
      event.returnValue = this.libraryController.getClientInfo();
    });

    // Register NJSIWalletController ipc handlers
    ipc.on("NJSIWalletController.HaveUnconfirmedFunds", event => {
      console.log(`IPC: walletController.HaveUnconfirmedFunds()`);
      event.returnValue = this.walletController.HaveUnconfirmedFunds();
    });

    ipc.on("NJSIWalletController.GetBalanceSimple", event => {
      console.log(`IPC: walletController.GetBalanceSimple()`);
      event.returnValue = this.walletController.GetBalanceSimple();
    });

    ipc.on("NJSIWalletController.GetBalance", event => {
      console.log(`IPC: walletController.GetBalance()`);
      event.returnValue = this.walletController.GetBalance();
    });

    // Register NJSIRpcController ipc handlers
    ipc.on("NJSIRpcController.getAutocompleteList", event => {
      console.log(`IPC: rpcController.getAutocompleteList()`);
      event.returnValue = this.rpcController.getAutocompleteList();
    });

    // Register NJSIP2pNetworkController ipc handlers
    ipc.on("NJSIP2pNetworkController.disableNetwork", event => {
      console.log(`IPC: p2pNetworkController.disableNetwork()`);
      event.returnValue = this.p2pNetworkController.disableNetwork();
    });

    ipc.on("NJSIP2pNetworkController.enableNetwork", event => {
      console.log(`IPC: p2pNetworkController.enableNetwork()`);
      event.returnValue = this.p2pNetworkController.enableNetwork();
    });

    ipc.on("NJSIP2pNetworkController.getPeerInfo", event => {
      console.log(`IPC: p2pNetworkController.getPeerInfo()`);
      event.returnValue = this.p2pNetworkController.getPeerInfo();
    });

    // Register NJSIAccountsController ipc handlers
    ipc.on("NJSIAccountsController.setActiveAccount", (event, accountUUID) => {
      console.log(`IPC: accountsController.setActiveAccount(${accountUUID})`);
      event.returnValue = this.accountsController.setActiveAccount(accountUUID);
    });

    ipc.on("NJSIAccountsController.getActiveAccount", event => {
      console.log(`IPC: accountsController.getActiveAccount()`);
      event.returnValue = this.accountsController.getActiveAccount();
    });

    ipc.on(
      "NJSIAccountsController.createAccount",
      (event, accountName, accountType) => {
        console.log(
          `IPC: accountsController.createAccount(${accountName}, ${accountType})`
        );
        event.returnValue = this.accountsController.createAccount(
          accountName,
          accountType
        );
      }
    );

    ipc.on(
      "NJSIAccountsController.renameAccount",
      (event, accountUUID, newAccountName) => {
        console.log(
          `IPC: accountsController.renameAccount(${accountUUID}, ${newAccountName})`
        );
        event.returnValue = this.accountsController.renameAccount(
          accountUUID,
          newAccountName
        );
      }
    );

    ipc.on("NJSIAccountsController.getAccountLinkURI", (event, accountUUID) => {
      console.log(`IPC: accountsController.getAccountLinkURI(${accountUUID})`);
      event.returnValue = this.accountsController.getAccountLinkURI(
        accountUUID
      );
    });

    ipc.on("NJSIAccountsController.getWitnessKeyURI", (event, accountUUID) => {
      console.log(`IPC: accountsController.getWitnessKeyURI(${accountUUID})`);
      event.returnValue = this.accountsController.getWitnessKeyURI(accountUUID);
    });

    ipc.on(
      "NJSIAccountsController.createAccountFromWitnessKeyURI",
      (event, witnessKeyURI, newAccountName) => {
        console.log(
          `IPC: accountsController.createAccountFromWitnessKeyURI(${witnessKeyURI}, ${newAccountName})`
        );
        event.returnValue = this.accountsController.createAccountFromWitnessKeyURI(
          witnessKeyURI,
          newAccountName
        );
      }
    );

    ipc.on("NJSIAccountsController.deleteAccount", (event, accountUUID) => {
      console.log(`IPC: accountsController.deleteAccount(${accountUUID})`);
      event.returnValue = this.accountsController.deleteAccount(accountUUID);
    });

    ipc.on("NJSIAccountsController.purgeAccount", (event, accountUUID) => {
      console.log(`IPC: accountsController.purgeAccount(${accountUUID})`);
      event.returnValue = this.accountsController.purgeAccount(accountUUID);
    });

    ipc.on("NJSIAccountsController.listAccounts", event => {
      console.log(`IPC: accountsController.listAccounts()`);
      event.returnValue = this.accountsController.listAccounts();
    });

    ipc.on("NJSIAccountsController.getActiveAccountBalance", event => {
      console.log(`IPC: accountsController.getActiveAccountBalance()`);
      event.returnValue = this.accountsController.getActiveAccountBalance();
    });

    ipc.on("NJSIAccountsController.getAccountBalance", (event, accountUUID) => {
      console.log(`IPC: accountsController.getAccountBalance(${accountUUID})`);
      event.returnValue = this.accountsController.getAccountBalance(
        accountUUID
      );
    });

    ipc.on("NJSIAccountsController.getAllAccountBalances", event => {
      console.log(`IPC: accountsController.getAllAccountBalances()`);
      event.returnValue = this.accountsController.getAllAccountBalances();
    });

    ipc.on(
      "NJSIAccountsController.getTransactionHistory",
      (event, accountUUID) => {
        console.log(
          `IPC: accountsController.getTransactionHistory(${accountUUID})`
        );
        event.returnValue = this.accountsController.getTransactionHistory(
          accountUUID
        );
      }
    );

    ipc.on(
      "NJSIAccountsController.getMutationHistory",
      (event, accountUUID) => {
        console.log(
          `IPC: accountsController.getMutationHistory(${accountUUID})`
        );
        event.returnValue = this.accountsController.getMutationHistory(
          accountUUID
        );
      }
    );

    // Register NJSIWitnessController ipc handlers
    ipc.on("NJSIWitnessController.getNetworkLimits", event => {
      console.log(`IPC: witnessController.getNetworkLimits()`);
      event.returnValue = this.witnessController.getNetworkLimits();
    });

    ipc.on(
      "NJSIWitnessController.getEstimatedWeight",
      (event, amount_to_lock, lock_period_in_blocks) => {
        console.log(
          `IPC: witnessController.getEstimatedWeight(${amount_to_lock}, ${lock_period_in_blocks})`
        );
        event.returnValue = this.witnessController.getEstimatedWeight(
          amount_to_lock,
          lock_period_in_blocks
        );
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
        event.returnValue = this.witnessController.fundWitnessAccount(
          funding_account_UUID,
          witness_account_UUID,
          funding_amount,
          requestedLockPeriodInBlocks
        );
      }
    );

    ipc.on(
      "NJSIWitnessController.getAccountWitnessStatistics",
      (event, witnessAccountUUID) => {
        console.log(
          `IPC: witnessController.getAccountWitnessStatistics(${witnessAccountUUID})`
        );
        event.returnValue = this.witnessController.getAccountWitnessStatistics(
          witnessAccountUUID
        );
      }
    );

    ipc.on(
      "NJSIWitnessController.setAccountCompounding",
      (event, witnessAccountUUID, should_compound) => {
        console.log(
          `IPC: witnessController.setAccountCompounding(${witnessAccountUUID}, ${should_compound})`
        );
        event.returnValue = this.witnessController.setAccountCompounding(
          witnessAccountUUID,
          should_compound
        );
      }
    );

    ipc.on(
      "NJSIWitnessController.isAccountCompounding",
      (event, witnessAccountUUID) => {
        console.log(
          `IPC: witnessController.isAccountCompounding(${witnessAccountUUID})`
        );
        event.returnValue = this.witnessController.isAccountCompounding(
          witnessAccountUUID
        );
      }
    );

    // Register NJSIGenerationController ipc handlers
    ipc.on(
      "NJSIGenerationController.startGeneration",
      (event, numThreads, memoryLimit) => {
        console.log(
          `IPC: generationController.startGeneration(${numThreads}, ${memoryLimit})`
        );
        event.returnValue = this.generationController.startGeneration(
          numThreads,
          memoryLimit
        );
      }
    );

    ipc.on("NJSIGenerationController.stopGeneration", event => {
      console.log(`IPC: generationController.stopGeneration()`);
      event.returnValue = this.generationController.stopGeneration();
    });

    ipc.on("NJSIGenerationController.getGenerationAddress", event => {
      console.log(`IPC: generationController.getGenerationAddress()`);
      event.returnValue = this.generationController.getGenerationAddress();
    });

    ipc.on("NJSIGenerationController.getGenerationOverrideAddress", event => {
      console.log(`IPC: generationController.getGenerationOverrideAddress()`);
      event.returnValue = this.generationController.getGenerationOverrideAddress();
    });

    ipc.on(
      "NJSIGenerationController.setGenerationOverrideAddress",
      (event, overrideAddress) => {
        console.log(
          `IPC: generationController.setGenerationOverrideAddress(${overrideAddress})`
        );
        event.returnValue = this.generationController.setGenerationOverrideAddress(
          overrideAddress
        );
      }
    );

    ipc.on("NJSIGenerationController.getAvailableCores", event => {
      console.log(`IPC: generationController.getAvailableCores()`);
      event.returnValue = this.generationController.getAvailableCores();
    });

    ipc.on("NJSIGenerationController.getMinimumMemory", event => {
      console.log(`IPC: generationController.getMinimumMemory()`);
      event.returnValue = this.generationController.getMinimumMemory();
    });

    ipc.on("NJSIGenerationController.getMaximumMemory", event => {
      console.log(`IPC: generationController.getMaximumMemory()`);
      event.returnValue = this.generationController.getMaximumMemory();
    });
    /* inject:generated-code */
  }
}

export default LibUnity;
