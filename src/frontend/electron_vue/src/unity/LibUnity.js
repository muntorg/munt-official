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

  _initializeAccountsController() {
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
      store.dispatch({
        type: "SET_ACCOUNTS",
        accounts: self._getAccountsWithBalances()
      });
    };

    this.accountsListener.onAccountDeleted = function() {
      store.dispatch({
        type: "SET_ACCOUNTS",
        accounts: self._getAccountsWithBalances()
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

  _getAccountsWithBalances() {
    let accounts = this.accountsController.listAccounts();
    let accountBalances = this.accountsController.getAllAccountBalances();

    Object.keys(accountBalances).forEach(key => {
      accounts.find(x => x.UUID === key).balance =
        (accountBalances[key].availableIncludingLocked +
          accountBalances[key].immatureIncludingLocked) /
        100000000;
    });

    return accounts;
  }

  _coreReady() {
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
      accounts: this._getAccountsWithBalances()
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

      store.dispatch({
        type: "SET_ACCOUNTS",
        accounts: self._getAccountsWithBalances()
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

  _registerIpcHandlers() {
    // ipc for rpc controller
    ipc.on("NJSIRpcController.Execute", (event, command) => {
      let rpcListener = new libUnity.NJSIRpcListener();

      rpcListener.onSuccess = (filteredCommand, result) => {
        console.log(`RPC success: ${result}`);
        event.returnValue = { success: true, data: result };
      };

      rpcListener.onError = error => {
        console.error(`RPC error: ${error}`);
        event.returnValue = { success: false, data: error };
      };

      this.rpcController.execute(command, rpcListener);
    });

    /* inject:generated-code */
    // Register NJSILibraryController ipc handlers
    ipc.on("NJSILibraryController.BuildInfo", event => {
      event.returnValue = this.libraryController.BuildInfo();
    });

    ipc.on(
      "NJSILibraryController.InitWalletFromRecoveryPhrase",
      (event, phrase, password) => {
        event.returnValue = this.libraryController.InitWalletFromRecoveryPhrase(
          phrase,
          password
        );
      }
    );

    ipc.on("NJSILibraryController.IsValidLinkURI", (event, phrase) => {
      event.returnValue = this.libraryController.IsValidLinkURI(phrase);
    });

    ipc.on(
      "NJSILibraryController.ReplaceWalletLinkedFromURI",
      (event, linked_uri, password) => {
        event.returnValue = this.libraryController.ReplaceWalletLinkedFromURI(
          linked_uri,
          password
        );
      }
    );

    ipc.on("NJSILibraryController.EraseWalletSeedsAndAccounts", event => {
      event.returnValue = this.libraryController.EraseWalletSeedsAndAccounts();
    });

    ipc.on("NJSILibraryController.IsValidRecoveryPhrase", (event, phrase) => {
      event.returnValue = this.libraryController.IsValidRecoveryPhrase(phrase);
    });

    ipc.on("NJSILibraryController.GenerateRecoveryMnemonic", event => {
      event.returnValue = this.libraryController.GenerateRecoveryMnemonic();
    });

    ipc.on("NJSILibraryController.GenerateGenesisKeys", event => {
      event.returnValue = this.libraryController.GenerateGenesisKeys();
    });

    ipc.on(
      "NJSILibraryController.ComposeRecoveryPhrase",
      (event, mnemonic, birthTime) => {
        event.returnValue = this.libraryController.ComposeRecoveryPhrase(
          mnemonic,
          birthTime
        );
      }
    );

    ipc.on("NJSILibraryController.TerminateUnityLib", event => {
      event.returnValue = this.libraryController.TerminateUnityLib();
    });

    ipc.on(
      "NJSILibraryController.QRImageFromString",
      (event, qr_string, width_hint) => {
        event.returnValue = this.libraryController.QRImageFromString(
          qr_string,
          width_hint
        );
      }
    );

    ipc.on("NJSILibraryController.GetReceiveAddress", event => {
      event.returnValue = this.libraryController.GetReceiveAddress();
    });

    ipc.on("NJSILibraryController.GetRecoveryPhrase", event => {
      event.returnValue = this.libraryController.GetRecoveryPhrase();
    });

    ipc.on("NJSILibraryController.IsMnemonicWallet", event => {
      event.returnValue = this.libraryController.IsMnemonicWallet();
    });

    ipc.on("NJSILibraryController.IsMnemonicCorrect", (event, phrase) => {
      event.returnValue = this.libraryController.IsMnemonicCorrect(phrase);
    });

    ipc.on("NJSILibraryController.GetMnemonicDictionary", event => {
      event.returnValue = this.libraryController.GetMnemonicDictionary();
    });

    ipc.on("NJSILibraryController.UnlockWallet", (event, password) => {
      event.returnValue = this.libraryController.UnlockWallet(password);
    });

    ipc.on("NJSILibraryController.LockWallet", event => {
      event.returnValue = this.libraryController.LockWallet();
    });

    ipc.on(
      "NJSILibraryController.ChangePassword",
      (event, oldPassword, newPassword) => {
        event.returnValue = this.libraryController.ChangePassword(
          oldPassword,
          newPassword
        );
      }
    );

    ipc.on("NJSILibraryController.DoRescan", event => {
      event.returnValue = this.libraryController.DoRescan();
    });

    ipc.on("NJSILibraryController.IsValidRecipient", (event, request) => {
      event.returnValue = this.libraryController.IsValidRecipient(request);
    });

    ipc.on("NJSILibraryController.IsValidNativeAddress", (event, address) => {
      event.returnValue = this.libraryController.IsValidNativeAddress(address);
    });

    ipc.on("NJSILibraryController.IsValidBitcoinAddress", (event, address) => {
      event.returnValue = this.libraryController.IsValidBitcoinAddress(address);
    });

    ipc.on("NJSILibraryController.feeForRecipient", (event, request) => {
      event.returnValue = this.libraryController.feeForRecipient(request);
    });

    ipc.on(
      "NJSILibraryController.performPaymentToRecipient",
      (event, request, substract_fee) => {
        event.returnValue = this.libraryController.performPaymentToRecipient(
          request,
          substract_fee
        );
      }
    );

    ipc.on("NJSILibraryController.getTransaction", (event, txHash) => {
      event.returnValue = this.libraryController.getTransaction(txHash);
    });

    ipc.on("NJSILibraryController.resendTransaction", (event, txHash) => {
      event.returnValue = this.libraryController.resendTransaction(txHash);
    });

    ipc.on("NJSILibraryController.getAddressBookRecords", event => {
      event.returnValue = this.libraryController.getAddressBookRecords();
    });

    ipc.on("NJSILibraryController.addAddressBookRecord", (event, address) => {
      event.returnValue = this.libraryController.addAddressBookRecord(address);
    });

    ipc.on(
      "NJSILibraryController.deleteAddressBookRecord",
      (event, address) => {
        event.returnValue = this.libraryController.deleteAddressBookRecord(
          address
        );
      }
    );

    ipc.on("NJSILibraryController.ResetUnifiedProgress", event => {
      event.returnValue = this.libraryController.ResetUnifiedProgress();
    });

    ipc.on("NJSILibraryController.getLastSPVBlockInfos", event => {
      event.returnValue = this.libraryController.getLastSPVBlockInfos();
    });

    ipc.on("NJSILibraryController.getUnifiedProgress", event => {
      event.returnValue = this.libraryController.getUnifiedProgress();
    });

    ipc.on("NJSILibraryController.getMonitoringStats", event => {
      event.returnValue = this.libraryController.getMonitoringStats();
    });

    ipc.on(
      "NJSILibraryController.RegisterMonitorListener",
      (event, listener) => {
        event.returnValue = this.libraryController.RegisterMonitorListener(
          listener
        );
      }
    );

    ipc.on(
      "NJSILibraryController.UnregisterMonitorListener",
      (event, listener) => {
        event.returnValue = this.libraryController.UnregisterMonitorListener(
          listener
        );
      }
    );

    ipc.on("NJSILibraryController.getClientInfo", event => {
      event.returnValue = this.libraryController.getClientInfo();
    });

    // Register NJSIWalletController ipc handlers
    ipc.on("NJSIWalletController.HaveUnconfirmedFunds", event => {
      event.returnValue = this.walletController.HaveUnconfirmedFunds();
    });

    ipc.on("NJSIWalletController.GetBalanceSimple", event => {
      event.returnValue = this.walletController.GetBalanceSimple();
    });

    ipc.on("NJSIWalletController.GetBalance", event => {
      event.returnValue = this.walletController.GetBalance();
    });

    // Register NJSIRpcController ipc handlers
    ipc.on("NJSIRpcController.getAutocompleteList", event => {
      event.returnValue = this.rpcController.getAutocompleteList();
    });

    // Register NJSIP2pNetworkController ipc handlers
    ipc.on("NJSIP2pNetworkController.disableNetwork", event => {
      event.returnValue = this.p2pNetworkController.disableNetwork();
    });

    ipc.on("NJSIP2pNetworkController.enableNetwork", event => {
      event.returnValue = this.p2pNetworkController.enableNetwork();
    });

    ipc.on("NJSIP2pNetworkController.getPeerInfo", event => {
      event.returnValue = this.p2pNetworkController.getPeerInfo();
    });

    // Register NJSIAccountsController ipc handlers
    ipc.on("NJSIAccountsController.setActiveAccount", (event, accountUUID) => {
      event.returnValue = this.accountsController.setActiveAccount(accountUUID);
    });

    ipc.on("NJSIAccountsController.getActiveAccount", event => {
      event.returnValue = this.accountsController.getActiveAccount();
    });

    ipc.on(
      "NJSIAccountsController.createAccount",
      (event, accountName, accountType) => {
        event.returnValue = this.accountsController.createAccount(
          accountName,
          accountType
        );
      }
    );

    ipc.on(
      "NJSIAccountsController.renameAccount",
      (event, accountUUID, newAccountName) => {
        event.returnValue = this.accountsController.renameAccount(
          accountUUID,
          newAccountName
        );
      }
    );

    ipc.on("NJSIAccountsController.getAccountLinkURI", (event, accountUUID) => {
      event.returnValue = this.accountsController.getAccountLinkURI(
        accountUUID
      );
    });

    ipc.on("NJSIAccountsController.getWitnessKeyURI", (event, accountUUID) => {
      event.returnValue = this.accountsController.getWitnessKeyURI(accountUUID);
    });

    ipc.on(
      "NJSIAccountsController.createAccountFromWitnessKeyURI",
      (event, witnessKeyURI, newAccountName) => {
        event.returnValue = this.accountsController.createAccountFromWitnessKeyURI(
          witnessKeyURI,
          newAccountName
        );
      }
    );

    ipc.on("NJSIAccountsController.deleteAccount", (event, accountUUID) => {
      event.returnValue = this.accountsController.deleteAccount(accountUUID);
    });

    ipc.on("NJSIAccountsController.purgeAccount", (event, accountUUID) => {
      event.returnValue = this.accountsController.purgeAccount(accountUUID);
    });

    ipc.on("NJSIAccountsController.listAccounts", event => {
      event.returnValue = this.accountsController.listAccounts();
    });

    ipc.on("NJSIAccountsController.getActiveAccountBalance", event => {
      event.returnValue = this.accountsController.getActiveAccountBalance();
    });

    ipc.on("NJSIAccountsController.getAccountBalance", (event, accountUUID) => {
      event.returnValue = this.accountsController.getAccountBalance(
        accountUUID
      );
    });

    ipc.on("NJSIAccountsController.getAllAccountBalances", event => {
      event.returnValue = this.accountsController.getAllAccountBalances();
    });

    ipc.on(
      "NJSIAccountsController.getTransactionHistory",
      (event, accountUUID) => {
        event.returnValue = this.accountsController.getTransactionHistory(
          accountUUID
        );
      }
    );

    ipc.on(
      "NJSIAccountsController.getMutationHistory",
      (event, accountUUID) => {
        event.returnValue = this.accountsController.getMutationHistory(
          accountUUID
        );
      }
    );

    // Register NJSIWitnessController ipc handlers
    ipc.on("NJSIWitnessController.getNetworkLimits", event => {
      event.returnValue = this.witnessController.getNetworkLimits();
    });

    ipc.on(
      "NJSIWitnessController.getEstimatedWeight",
      (event, amount_to_lock, lock_period_in_blocks) => {
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
        event.returnValue = this.witnessController.getAccountWitnessStatistics(
          witnessAccountUUID
        );
      }
    );

    ipc.on(
      "NJSIWitnessController.setAccountCompounding",
      (event, witnessAccountUUID, should_compound) => {
        event.returnValue = this.witnessController.setAccountCompounding(
          witnessAccountUUID,
          should_compound
        );
      }
    );

    ipc.on(
      "NJSIWitnessController.isAccountCompounding",
      (event, witnessAccountUUID) => {
        event.returnValue = this.witnessController.isAccountCompounding(
          witnessAccountUUID
        );
      }
    );

    // Register NJSIGenerationController ipc handlers
    ipc.on(
      "NJSIGenerationController.startGeneration",
      (event, numThreads, memoryLimit) => {
        event.returnValue = this.generationController.startGeneration(
          numThreads,
          memoryLimit
        );
      }
    );

    ipc.on("NJSIGenerationController.stopGeneration", event => {
      event.returnValue = this.generationController.stopGeneration();
    });

    ipc.on("NJSIGenerationController.getGenerationAddress", event => {
      event.returnValue = this.generationController.getGenerationAddress();
    });

    ipc.on("NJSIGenerationController.getGenerationOverrideAddress", event => {
      event.returnValue = this.generationController.getGenerationOverrideAddress();
    });

    ipc.on(
      "NJSIGenerationController.setGenerationOverrideAddress",
      (event, overrideAddress) => {
        event.returnValue = this.generationController.setGenerationOverrideAddress(
          overrideAddress
        );
      }
    );

    ipc.on("NJSIGenerationController.getAvailableCores", event => {
      event.returnValue = this.generationController.getAvailableCores();
    });

    ipc.on("NJSIGenerationController.getMinimumMemory", event => {
      event.returnValue = this.generationController.getMinimumMemory();
    });

    ipc.on("NJSIGenerationController.getMaximumMemory", event => {
      event.returnValue = this.generationController.getMaximumMemory();
    });
    /* inject:generated-code */
  }
}

export default LibUnity;
