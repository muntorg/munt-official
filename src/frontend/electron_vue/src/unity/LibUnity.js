import { app } from "electron";
import { ipcMain as ipc } from "electron-better-ipc";
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

    this.rpcController = null;

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

  async _initializeAccountsController() {
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

    this.accountsListener.onAccountAdded = async function() {
      store.dispatch({
        type: "SET_ACCOUNTS",
        accounts: await self._getAccountsWithBalancesAsync()
      });
    };

    this.accountsListener.onAccountDeleted = async function() {
      store.dispatch({
        type: "SET_ACCOUNTS",
        accounts: await self._getAccountsWithBalancesAsync()
      });
    };

    this.accountsController.setListener(this.accountsListener);
  }

  _initializeGenerationController() {
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
          hashesPerSecond: `${hashesPerSecond}${hashesPerSecondUnit}`,
          rollingHashesPerSecond: `${rollingHashesPerSecond}${rollingHashesPerSecondUnit}`,
          bestHashesPerSecond: `${bestHashesPerSecond}${bestHashesPerSecondUnit}`,
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

  async _getAccountsWithBalancesAsync() {
    let accounts = this.accountsController.listAccounts();
    let accountBalances = await this._executeRpcAsync("getaccountbalances");

    for (var i = 0; i < accountBalances.length; i++) {
      let accountBalance = accountBalances[i];
      accounts.find(x => x.UUID === accountBalance.UUID).balance =
        accountBalance.balance;
    }

    return accounts;
  }

  async _coreReady() {
    this._initializeWalletController();
    this._initializeAccountsController();
    this._initializeGenerationController();

    console.log(`balanceSimple: ${this.walletController.GetBalanceSimple()}`);

    store.dispatch({
      type: "SET_WALLET_BALANCE",
      walletBalance: this.walletController.GetBalance()
    });

    store.dispatch({
      type: "SET_ACCOUNTS",
      accounts: await this._getAccountsWithBalancesAsync()
    });

    store.dispatch({
      type: "SET_ACTIVE_ACCOUNT",
      accountUUID: this.accountsController.getActiveAccount()
    });

    store.dispatch({
      type: "SET_RECEIVE_ADDRESS",
      receiveAddress: this.libraryController.GetReceiveAddress()
    });

    store.dispatch({
      type: "SET_MUTATIONS",
      mutations: this.libraryController.getMutationHistory()
    });

    store.dispatch({ type: "SET_CORE_READY", coreReady: true });
  }

  async _registerSignalHandlers() {
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

    libraryListener.notifyBalanceChange = async function(new_balance) {
      console.log("received: notifyBalanceChange");
      store.dispatch({ type: "SET_BALANCE", balance: new_balance });
    };

    libraryListener.notifyNewMutation = async function(/*mutation, self_committed*/) {
      console.log("received: notifyNewMutation");

      store.dispatch({
        type: "SET_MUTATIONS",
        mutations: libraryController.getMutationHistory()
      });
      store.dispatch({
        type: "SET_RECEIVE_ADDRESS",
        receiveAddress: libraryController.GetReceiveAddress()
      });
      // todo: find a better way to update the balances for the accounts
      store.dispatch({
        type: "SET_ACCOUNTS",
        accounts: await self._getAccountsWithBalancesAsync()
      });
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

  _preExecuteIpcCommand(event) {
    switch (event) {
      case "GenerateRecoveryMnemonic":
        return this.newRecoveryPhrase; // when a new recovery phrase is created it is stored in memory once to prevent multiple callse to GenerateRecoveryMnemonic
    }
    return undefined;
  }

  _postExecuteIpcCommand(event, result) {
    switch (event) {
      case "InitWalletFromRecoveryPhrase":
        if (result === true) {
          delete this.newRecoveryPhrase; // remove recoveryPhrase property because it isn't needed anymore
          store.dispatch({
            type: "SET_WALLET_EXISTS",
            walletExists: true
          });
        }
        break;
    }
  }

  async _executeRpcAsync(command) {
    let rpcListener = new libUnity.NJSIRpcListener();

    if (this.rpcController === null) {
      this.rpcController = new libUnity.NJSIRpcController();
    }

    this.rpcController.execute(command, rpcListener);

    return new Promise((resolve, reject) => {
      rpcListener.onSuccess = (filteredCommand, result) => {
        console.log(`RPC success: ${result}`);

        try {
          resolve(JSON.parse(result));
        } catch {
          resolve(result);
        }
      };

      rpcListener.onError = error => {
        console.error(`RPC error: ${error}`);
        reject(error);
      };
    });
  }

  _registerIpcHandlers() {
    // ipc for rpc controller
    ipc.on("ExecuteRpc", (event, command) => {
      let rpcListener = new libUnity.NJSIRpcListener();

      rpcListener.onSuccess = (filteredCommand, result) => {
        console.log(`RPC success: ${result}`);
        event.returnValue = { success: true, data: result };
      };

      rpcListener.onError = error => {
        console.error(`RPC error: ${error}`);
        event.returnValue = { success: false, data: error };
      };

      if (this.rpcController === null) {
        this.rpcController = new libUnity.NJSIRpcController();
      }

      this.rpcController.execute(command, rpcListener);
    });

    ipc.answerRenderer("ExecuteRpc", async data => {
      let rpcListener = new libUnity.NJSIRpcListener();

      rpcListener.onSuccess = (filteredCommand, result) => {
        console.log(`RPC success: ${result}`);
        return { success: true, data: result };
      };

      rpcListener.onError = error => {
        console.error(`RPC error: ${error}`);
        return { success: false, data: error };
      };

      if (this.rpcController === null) {
        this.rpcController = new libUnity.NJSIRpcController();
      }

      this.rpcController.execute(data.command, rpcListener);
    });

    /* inject:code */
    ipc.on("InitWalletFromRecoveryPhrase", (event, phrase, password) => {
      let result = this._preExecuteIpcCommand("InitWalletFromRecoveryPhrase");
      if (result === undefined) {
        result = this.libraryController.InitWalletFromRecoveryPhrase(
          phrase,
          password
        );
      }
      this._postExecuteIpcCommand("InitWalletFromRecoveryPhrase", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("InitWalletFromRecoveryPhrase", async data => {
      let result = this._preExecuteIpcCommand("InitWalletFromRecoveryPhrase");
      if (result === undefined) {
        result = this.libraryController.InitWalletFromRecoveryPhrase(
          data.phrase,
          data.password
        );
      }
      this._postExecuteIpcCommand("InitWalletFromRecoveryPhrase", result);
      return result;
    });

    ipc.on("IsValidRecoveryPhrase", (event, phrase) => {
      let result = this._preExecuteIpcCommand("IsValidRecoveryPhrase");
      if (result === undefined) {
        result = this.libraryController.IsValidRecoveryPhrase(phrase);
      }
      this._postExecuteIpcCommand("IsValidRecoveryPhrase", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("IsValidRecoveryPhrase", async data => {
      let result = this._preExecuteIpcCommand("IsValidRecoveryPhrase");
      if (result === undefined) {
        result = this.libraryController.IsValidRecoveryPhrase(data.phrase);
      }
      this._postExecuteIpcCommand("IsValidRecoveryPhrase", result);
      return result;
    });

    ipc.on("GenerateRecoveryMnemonic", event => {
      let result = this._preExecuteIpcCommand("GenerateRecoveryMnemonic");
      if (result === undefined) {
        result = this.libraryController.GenerateRecoveryMnemonic();
      }
      this._postExecuteIpcCommand("GenerateRecoveryMnemonic", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GenerateRecoveryMnemonic", async () => {
      let result = this._preExecuteIpcCommand("GenerateRecoveryMnemonic");
      if (result === undefined) {
        result = this.libraryController.GenerateRecoveryMnemonic();
      }
      this._postExecuteIpcCommand("GenerateRecoveryMnemonic", result);
      return result;
    });

    ipc.on("GetRecoveryPhrase", event => {
      let result = this._preExecuteIpcCommand("GetRecoveryPhrase");
      if (result === undefined) {
        result = this.libraryController.GetRecoveryPhrase();
      }
      this._postExecuteIpcCommand("GetRecoveryPhrase", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetRecoveryPhrase", async () => {
      let result = this._preExecuteIpcCommand("GetRecoveryPhrase");
      if (result === undefined) {
        result = this.libraryController.GetRecoveryPhrase();
      }
      this._postExecuteIpcCommand("GetRecoveryPhrase", result);
      return result;
    });

    ipc.on("GetMnemonicDictionary", event => {
      let result = this._preExecuteIpcCommand("GetMnemonicDictionary");
      if (result === undefined) {
        result = this.libraryController.GetMnemonicDictionary();
      }
      this._postExecuteIpcCommand("GetMnemonicDictionary", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetMnemonicDictionary", async () => {
      let result = this._preExecuteIpcCommand("GetMnemonicDictionary");
      if (result === undefined) {
        result = this.libraryController.GetMnemonicDictionary();
      }
      this._postExecuteIpcCommand("GetMnemonicDictionary", result);
      return result;
    });

    ipc.on("UnlockWallet", (event, password) => {
      let result = this._preExecuteIpcCommand("UnlockWallet");
      if (result === undefined) {
        result = this.libraryController.UnlockWallet(password);
      }
      this._postExecuteIpcCommand("UnlockWallet", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("UnlockWallet", async data => {
      let result = this._preExecuteIpcCommand("UnlockWallet");
      if (result === undefined) {
        result = this.libraryController.UnlockWallet(data.password);
      }
      this._postExecuteIpcCommand("UnlockWallet", result);
      return result;
    });

    ipc.on("LockWallet", event => {
      let result = this._preExecuteIpcCommand("LockWallet");
      if (result === undefined) {
        result = this.libraryController.LockWallet();
      }
      this._postExecuteIpcCommand("LockWallet", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("LockWallet", async () => {
      let result = this._preExecuteIpcCommand("LockWallet");
      if (result === undefined) {
        result = this.libraryController.LockWallet();
      }
      this._postExecuteIpcCommand("LockWallet", result);
      return result;
    });

    ipc.on("ChangePassword", (event, oldPassword, newPassword) => {
      let result = this._preExecuteIpcCommand("ChangePassword");
      if (result === undefined) {
        result = this.libraryController.ChangePassword(
          oldPassword,
          newPassword
        );
      }
      this._postExecuteIpcCommand("ChangePassword", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("ChangePassword", async data => {
      let result = this._preExecuteIpcCommand("ChangePassword");
      if (result === undefined) {
        result = this.libraryController.ChangePassword(
          data.oldPassword,
          data.newPassword
        );
      }
      this._postExecuteIpcCommand("ChangePassword", result);
      return result;
    });

    ipc.on("IsValidRecipient", (event, request) => {
      let result = this._preExecuteIpcCommand("IsValidRecipient");
      if (result === undefined) {
        result = this.libraryController.IsValidRecipient(request);
      }
      this._postExecuteIpcCommand("IsValidRecipient", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("IsValidRecipient", async data => {
      let result = this._preExecuteIpcCommand("IsValidRecipient");
      if (result === undefined) {
        result = this.libraryController.IsValidRecipient(data.request);
      }
      this._postExecuteIpcCommand("IsValidRecipient", result);
      return result;
    });

    ipc.on("IsValidNativeAddress", (event, address) => {
      let result = this._preExecuteIpcCommand("IsValidNativeAddress");
      if (result === undefined) {
        result = this.libraryController.IsValidNativeAddress(address);
      }
      this._postExecuteIpcCommand("IsValidNativeAddress", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("IsValidNativeAddress", async data => {
      let result = this._preExecuteIpcCommand("IsValidNativeAddress");
      if (result === undefined) {
        result = this.libraryController.IsValidNativeAddress(data.address);
      }
      this._postExecuteIpcCommand("IsValidNativeAddress", result);
      return result;
    });

    ipc.on("PerformPaymentToRecipient", (event, request, substract_fee) => {
      let result = this._preExecuteIpcCommand("PerformPaymentToRecipient");
      if (result === undefined) {
        result = this.libraryController.performPaymentToRecipient(
          request,
          substract_fee
        );
      }
      this._postExecuteIpcCommand("PerformPaymentToRecipient", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("PerformPaymentToRecipient", async data => {
      let result = this._preExecuteIpcCommand("PerformPaymentToRecipient");
      if (result === undefined) {
        result = this.libraryController.performPaymentToRecipient(
          data.request,
          data.substract_fee
        );
      }
      this._postExecuteIpcCommand("PerformPaymentToRecipient", result);
      return result;
    });

    ipc.on("ResendTransaction", (event, txHash) => {
      let result = this._preExecuteIpcCommand("ResendTransaction");
      if (result === undefined) {
        result = this.libraryController.resendTransaction(txHash);
      }
      this._postExecuteIpcCommand("ResendTransaction", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("ResendTransaction", async data => {
      let result = this._preExecuteIpcCommand("ResendTransaction");
      if (result === undefined) {
        result = this.libraryController.resendTransaction(data.txHash);
      }
      this._postExecuteIpcCommand("ResendTransaction", result);
      return result;
    });

    ipc.on("GetTransactionHistory", event => {
      let result = this._preExecuteIpcCommand("GetTransactionHistory");
      if (result === undefined) {
        result = this.libraryController.getTransactionHistory();
      }
      this._postExecuteIpcCommand("GetTransactionHistory", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetTransactionHistory", async () => {
      let result = this._preExecuteIpcCommand("GetTransactionHistory");
      if (result === undefined) {
        result = this.libraryController.getTransactionHistory();
      }
      this._postExecuteIpcCommand("GetTransactionHistory", result);
      return result;
    });
    ipc.on("SetActiveAccount", (event, accountUUID) => {
      let result = this._preExecuteIpcCommand("SetActiveAccount");
      if (result === undefined) {
        result = this.accountsController.setActiveAccount(accountUUID);
      }
      this._postExecuteIpcCommand("SetActiveAccount", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("SetActiveAccount", async data => {
      let result = this._preExecuteIpcCommand("SetActiveAccount");
      if (result === undefined) {
        result = this.accountsController.setActiveAccount(data.accountUUID);
      }
      this._postExecuteIpcCommand("SetActiveAccount", result);
      return result;
    });

    ipc.on("CreateAccount", (event, accountName, accountType) => {
      let result = this._preExecuteIpcCommand("CreateAccount");
      if (result === undefined) {
        result = this.accountsController.createAccount(
          accountName,
          accountType
        );
      }
      this._postExecuteIpcCommand("CreateAccount", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("CreateAccount", async data => {
      let result = this._preExecuteIpcCommand("CreateAccount");
      if (result === undefined) {
        result = this.accountsController.createAccount(
          data.accountName,
          data.accountType
        );
      }
      this._postExecuteIpcCommand("CreateAccount", result);
      return result;
    });

    ipc.on("GetActiveAccountBalance", event => {
      let result = this._preExecuteIpcCommand("GetActiveAccountBalance");
      if (result === undefined) {
        result = this.accountsController.getActiveAccountBalance();
      }
      this._postExecuteIpcCommand("GetActiveAccountBalance", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetActiveAccountBalance", async () => {
      let result = this._preExecuteIpcCommand("GetActiveAccountBalance");
      if (result === undefined) {
        result = this.accountsController.getActiveAccountBalance();
      }
      this._postExecuteIpcCommand("GetActiveAccountBalance", result);
      return result;
    });
    ipc.on("StartGeneration", (event, numThreads, memoryLimit) => {
      let result = this._preExecuteIpcCommand("StartGeneration");
      if (result === undefined) {
        result = this.generationController.startGeneration(
          numThreads,
          memoryLimit
        );
      }
      this._postExecuteIpcCommand("StartGeneration", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("StartGeneration", async data => {
      let result = this._preExecuteIpcCommand("StartGeneration");
      if (result === undefined) {
        result = this.generationController.startGeneration(
          data.numThreads,
          data.memoryLimit
        );
      }
      this._postExecuteIpcCommand("StartGeneration", result);
      return result;
    });

    ipc.on("StopGeneration", event => {
      let result = this._preExecuteIpcCommand("StopGeneration");
      if (result === undefined) {
        result = this.generationController.stopGeneration();
      }
      this._postExecuteIpcCommand("StopGeneration", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("StopGeneration", async () => {
      let result = this._preExecuteIpcCommand("StopGeneration");
      if (result === undefined) {
        result = this.generationController.stopGeneration();
      }
      this._postExecuteIpcCommand("StopGeneration", result);
      return result;
    });
    ipc.on("GetNetworkLimits", event => {
      let result = this._preExecuteIpcCommand("GetNetworkLimits");
      if (result === undefined) {
        result = this.witnessController.getNetworkLimits();
      }
      this._postExecuteIpcCommand("GetNetworkLimits", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetNetworkLimits", async () => {
      let result = this._preExecuteIpcCommand("GetNetworkLimits");
      if (result === undefined) {
        result = this.witnessController.getNetworkLimits();
      }
      this._postExecuteIpcCommand("GetNetworkLimits", result);
      return result;
    });
    /* inject:code */
  }
}

export default LibUnity;
