import { app } from "electron";
import { ipcMain as ipc } from "electron-better-ipc";
import fs from "fs";

import store, { AppStatus } from "../store";

import libUnity from "native-ext-loader!./lib_unity.node";

class LibUnity {
  constructor(options) {
    this.initialized = false;
    this.isTerminated = false;

    this.options = {
      useTestNet: false,
      extraArgs: process.env.UNITY_EXTRA_ARGS
        ? process.env.UNITY_EXTRA_ARGS
        : "",
      ...options
    };

    this.win = null;
    this.backend = new libUnity.NJSUnifiedBackend();
    this.signalHandler = new libUnity.NJSUnifiedFrontend();
  }

  SetWindow(win) {
    this.win = win;
  }

  Initialize() {
    if (this.initialized) return;

    this.initialized = true;

    this._registerIpcHandlers();
    this._registerSignalHandlers();
    this._startUnityLib();
  }

  TerminateUnityLib() {
    if (this.backend === null || this.isTerminated) return;
    console.log(`terminating unity lib`);
    // TODO:
    // Maybe the call to terminate comes before the core is ready.
    // Then it's better to wait for the coreReady signal and then call TerminateUnityLib
    this.backend.TerminateUnityLib();
  }

  _startUnityLib() {
    if (!fs.existsSync(this.options.walletPath)) {
      console.log(`create wallet folder ${this.options.walletPath}`);
      fs.mkdirSync(this.options.walletPath, { recursive: true });
    } else {
      console.log(`wallet folder ${this.options.walletPath} already exists`);
    }

    console.log(`init unity lib threaded`);
    this.backend.InitUnityLibThreaded(
      this.options.walletPath,
      "",
      -1,
      -1,
      this.options.useTestNet,
      false, // non spv mode
      this.signalHandler,
      this.options.extraArgs
    );
  }

  _registerSignalHandlers() {
    let self = this;
    let signalHandler = this.signalHandler;
    let backend = this.backend;

    signalHandler.notifyCoreReady = function() {
      console.log("received: notifyCoreReady");

      store.dispatch({
        type: "SET_RECEIVE_ADDRESS",
        receiveAddress: backend.GetReceiveAddress()
      });

      store.dispatch({
        type: "SET_MUTATIONS",
        mutations: backend.getMutationHistory()
      });
      store.dispatch({ type: "SET_CORE_READY", coreReady: true });
    };

    signalHandler.logPrint = function(message) {
      console.log("unity_core: " + message);

      if (message.indexOf("Novo version v") === 0) {
        store.dispatch({
          type: "SET_UNITY_VERSION",
          version: message
            .substr(0, message.indexOf("-"))
            .replace("Novo version v", "")
        });
      }
    };

    signalHandler.notifyUnifiedProgress = function(/*progress*/) {
      console.log("received: notifyUnifiedProgress");
      // todo: set progress property in store?
    };

    signalHandler.notifyBalanceChange = async function(new_balance) {
      console.log("received: notifyBalanceChange");
      store.dispatch({ type: "SET_BALANCE", balance: new_balance });
    };

    signalHandler.notifyNewMutation = function(/*mutation, self_committed*/) {
      console.log("received: notifyNewMutation");

      store.dispatch({
        type: "SET_MUTATIONS",
        mutations: backend.getMutationHistory()
      });
      store.dispatch({
        type: "SET_RECEIVE_ADDRESS",
        receiveAddress: backend.GetReceiveAddress()
      });
    };

    signalHandler.notifyUpdatedTransaction = function(/*transaction*/) {
      console.log("received: notifyUpdatedTransaction");
      store.dispatch({
        type: "SET_MUTATIONS",
        mutations: backend.getMutationHistory()
      });
    };

    signalHandler.notifyInitWithExistingWallet = function() {
      console.log("received: notifyInitWithExistingWallet");
      store.dispatch({ type: "SET_WALLET_EXISTS", walletExists: true });
    };

    signalHandler.notifyInitWithoutExistingWallet = function() {
      console.log("received: notifyInitWithoutExistingWallet");
      self.newRecoveryPhrase = backend.GenerateRecoveryMnemonic();
      store.dispatch({ type: "SET_WALLET_EXISTS", walletExists: false });
    };

    signalHandler.notifyShutdown = function() {
      //NB! It is important to set the signalHandler to null here as this is the last thing the core lib is waiting on for clean exit; once we set this to null we are free to quit the app
      signalHandler = null;
      console.log("received: notifyShutdown");
      self.isTerminated = true;
      app.quit();
    };
  }

  _tryGetResultBeforeBackendCall(event) {
    switch (event) {
      case "GenerateRecoveryMnemonic":
        return this.newRecoveryPhrase; // when a new recovery phrase is created it is stored in memory once to prevent multiple callse to GenerateRecoveryMnemonic
    }
    return undefined;
  }

  _handleIpcInternalAfterBackendCall(event, result) {
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

  _registerIpcHandlers() {
    /* inject:code */
    ipc.on("AddAddressBookRecord", (event, address) => {
      let result = this._tryGetResultBeforeBackendCall("AddAddressBookRecord");
      if (result === undefined) {
        result = this.backend.addAddressBookRecord(address);
      }
      this._handleIpcInternalAfterBackendCall("AddAddressBookRecord", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("AddAddressBookRecord", async data => {
      let result = this._tryGetResultBeforeBackendCall("AddAddressBookRecord");
      if (result === undefined) {
        result = this.backend.addAddressBookRecord(data.address);
      }
      this._handleIpcInternalAfterBackendCall("AddAddressBookRecord", result);
      return result;
    });

    ipc.on("BuildInfo", event => {
      let result = this._tryGetResultBeforeBackendCall("BuildInfo");
      if (result === undefined) {
        result = this.backend.BuildInfo();
      }
      this._handleIpcInternalAfterBackendCall("BuildInfo", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("BuildInfo", async () => {
      let result = this._tryGetResultBeforeBackendCall("BuildInfo");
      if (result === undefined) {
        result = this.backend.BuildInfo();
      }
      this._handleIpcInternalAfterBackendCall("BuildInfo", result);
      return result;
    });

    ipc.on("ChangePassword", (event, oldPassword, newPassword) => {
      let result = this._tryGetResultBeforeBackendCall("ChangePassword");
      if (result === undefined) {
        result = this.backend.ChangePassword(oldPassword, newPassword);
      }
      this._handleIpcInternalAfterBackendCall("ChangePassword", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("ChangePassword", async data => {
      let result = this._tryGetResultBeforeBackendCall("ChangePassword");
      if (result === undefined) {
        result = this.backend.ChangePassword(
          data.oldPassword,
          data.newPassword
        );
      }
      this._handleIpcInternalAfterBackendCall("ChangePassword", result);
      return result;
    });

    ipc.on("ComposeRecoveryPhrase", (event, mnemonic, birthTime) => {
      let result = this._tryGetResultBeforeBackendCall("ComposeRecoveryPhrase");
      if (result === undefined) {
        result = this.backend.ComposeRecoveryPhrase(mnemonic, birthTime);
      }
      this._handleIpcInternalAfterBackendCall("ComposeRecoveryPhrase", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("ComposeRecoveryPhrase", async data => {
      let result = this._tryGetResultBeforeBackendCall("ComposeRecoveryPhrase");
      if (result === undefined) {
        result = this.backend.ComposeRecoveryPhrase(
          data.mnemonic,
          data.birthTime
        );
      }
      this._handleIpcInternalAfterBackendCall("ComposeRecoveryPhrase", result);
      return result;
    });

    ipc.on("ContinueWalletFromRecoveryPhrase", (event, phrase, password) => {
      let result = this._tryGetResultBeforeBackendCall(
        "ContinueWalletFromRecoveryPhrase"
      );
      if (result === undefined) {
        result = this.backend.ContinueWalletFromRecoveryPhrase(
          phrase,
          password
        );
      }
      this._handleIpcInternalAfterBackendCall(
        "ContinueWalletFromRecoveryPhrase",
        result
      );
      event.returnValue = result;
    });

    ipc.answerRenderer("ContinueWalletFromRecoveryPhrase", async data => {
      let result = this._tryGetResultBeforeBackendCall(
        "ContinueWalletFromRecoveryPhrase"
      );
      if (result === undefined) {
        result = this.backend.ContinueWalletFromRecoveryPhrase(
          data.phrase,
          data.password
        );
      }
      this._handleIpcInternalAfterBackendCall(
        "ContinueWalletFromRecoveryPhrase",
        result
      );
      return result;
    });

    ipc.on("ContinueWalletLinkedFromURI", (event, linked_uri, password) => {
      let result = this._tryGetResultBeforeBackendCall(
        "ContinueWalletLinkedFromURI"
      );
      if (result === undefined) {
        result = this.backend.ContinueWalletLinkedFromURI(linked_uri, password);
      }
      this._handleIpcInternalAfterBackendCall(
        "ContinueWalletLinkedFromURI",
        result
      );
      event.returnValue = result;
    });

    ipc.answerRenderer("ContinueWalletLinkedFromURI", async data => {
      let result = this._tryGetResultBeforeBackendCall(
        "ContinueWalletLinkedFromURI"
      );
      if (result === undefined) {
        result = this.backend.ContinueWalletLinkedFromURI(
          data.linked_uri,
          data.password
        );
      }
      this._handleIpcInternalAfterBackendCall(
        "ContinueWalletLinkedFromURI",
        result
      );
      return result;
    });

    ipc.on("DeleteAddressBookRecord", (event, address) => {
      let result = this._tryGetResultBeforeBackendCall(
        "DeleteAddressBookRecord"
      );
      if (result === undefined) {
        result = this.backend.deleteAddressBookRecord(address);
      }
      this._handleIpcInternalAfterBackendCall(
        "DeleteAddressBookRecord",
        result
      );
      event.returnValue = result;
    });

    ipc.answerRenderer("DeleteAddressBookRecord", async data => {
      let result = this._tryGetResultBeforeBackendCall(
        "DeleteAddressBookRecord"
      );
      if (result === undefined) {
        result = this.backend.deleteAddressBookRecord(data.address);
      }
      this._handleIpcInternalAfterBackendCall(
        "DeleteAddressBookRecord",
        result
      );
      return result;
    });

    ipc.on("DoRescan", event => {
      let result = this._tryGetResultBeforeBackendCall("DoRescan");
      if (result === undefined) {
        result = this.backend.DoRescan();
      }
      this._handleIpcInternalAfterBackendCall("DoRescan", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("DoRescan", async () => {
      let result = this._tryGetResultBeforeBackendCall("DoRescan");
      if (result === undefined) {
        result = this.backend.DoRescan();
      }
      this._handleIpcInternalAfterBackendCall("DoRescan", result);
      return result;
    });

    ipc.on("EraseWalletSeedsAndAccounts", event => {
      let result = this._tryGetResultBeforeBackendCall(
        "EraseWalletSeedsAndAccounts"
      );
      if (result === undefined) {
        result = this.backend.EraseWalletSeedsAndAccounts();
      }
      this._handleIpcInternalAfterBackendCall(
        "EraseWalletSeedsAndAccounts",
        result
      );
      event.returnValue = result;
    });

    ipc.answerRenderer("EraseWalletSeedsAndAccounts", async () => {
      let result = this._tryGetResultBeforeBackendCall(
        "EraseWalletSeedsAndAccounts"
      );
      if (result === undefined) {
        result = this.backend.EraseWalletSeedsAndAccounts();
      }
      this._handleIpcInternalAfterBackendCall(
        "EraseWalletSeedsAndAccounts",
        result
      );
      return result;
    });

    ipc.on("FeeForRecipient", (event, request) => {
      let result = this._tryGetResultBeforeBackendCall("FeeForRecipient");
      if (result === undefined) {
        result = this.backend.feeForRecipient(request);
      }
      this._handleIpcInternalAfterBackendCall("FeeForRecipient", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("FeeForRecipient", async data => {
      let result = this._tryGetResultBeforeBackendCall("FeeForRecipient");
      if (result === undefined) {
        result = this.backend.feeForRecipient(data.request);
      }
      this._handleIpcInternalAfterBackendCall("FeeForRecipient", result);
      return result;
    });

    ipc.on("GenerateGenesisKeys", event => {
      let result = this._tryGetResultBeforeBackendCall("GenerateGenesisKeys");
      if (result === undefined) {
        result = this.backend.GenerateGenesisKeys();
      }
      this._handleIpcInternalAfterBackendCall("GenerateGenesisKeys", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GenerateGenesisKeys", async () => {
      let result = this._tryGetResultBeforeBackendCall("GenerateGenesisKeys");
      if (result === undefined) {
        result = this.backend.GenerateGenesisKeys();
      }
      this._handleIpcInternalAfterBackendCall("GenerateGenesisKeys", result);
      return result;
    });

    ipc.on("GenerateRecoveryMnemonic", event => {
      let result = this._tryGetResultBeforeBackendCall(
        "GenerateRecoveryMnemonic"
      );
      if (result === undefined) {
        result = this.backend.GenerateRecoveryMnemonic();
      }
      this._handleIpcInternalAfterBackendCall(
        "GenerateRecoveryMnemonic",
        result
      );
      event.returnValue = result;
    });

    ipc.answerRenderer("GenerateRecoveryMnemonic", async () => {
      let result = this._tryGetResultBeforeBackendCall(
        "GenerateRecoveryMnemonic"
      );
      if (result === undefined) {
        result = this.backend.GenerateRecoveryMnemonic();
      }
      this._handleIpcInternalAfterBackendCall(
        "GenerateRecoveryMnemonic",
        result
      );
      return result;
    });

    ipc.on("GetAddressBookRecords", event => {
      let result = this._tryGetResultBeforeBackendCall("GetAddressBookRecords");
      if (result === undefined) {
        result = this.backend.getAddressBookRecords();
      }
      this._handleIpcInternalAfterBackendCall("GetAddressBookRecords", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetAddressBookRecords", async () => {
      let result = this._tryGetResultBeforeBackendCall("GetAddressBookRecords");
      if (result === undefined) {
        result = this.backend.getAddressBookRecords();
      }
      this._handleIpcInternalAfterBackendCall("GetAddressBookRecords", result);
      return result;
    });

    ipc.on("GetBalance", event => {
      let result = this._tryGetResultBeforeBackendCall("GetBalance");
      if (result === undefined) {
        result = this.backend.GetBalance();
      }
      this._handleIpcInternalAfterBackendCall("GetBalance", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetBalance", async () => {
      let result = this._tryGetResultBeforeBackendCall("GetBalance");
      if (result === undefined) {
        result = this.backend.GetBalance();
      }
      this._handleIpcInternalAfterBackendCall("GetBalance", result);
      return result;
    });

    ipc.on("GetLastSPVBlockInfos", event => {
      let result = this._tryGetResultBeforeBackendCall("GetLastSPVBlockInfos");
      if (result === undefined) {
        result = this.backend.getLastSPVBlockInfos();
      }
      this._handleIpcInternalAfterBackendCall("GetLastSPVBlockInfos", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetLastSPVBlockInfos", async () => {
      let result = this._tryGetResultBeforeBackendCall("GetLastSPVBlockInfos");
      if (result === undefined) {
        result = this.backend.getLastSPVBlockInfos();
      }
      this._handleIpcInternalAfterBackendCall("GetLastSPVBlockInfos", result);
      return result;
    });

    ipc.on("GetMonitoringStats", event => {
      let result = this._tryGetResultBeforeBackendCall("GetMonitoringStats");
      if (result === undefined) {
        result = this.backend.getMonitoringStats();
      }
      this._handleIpcInternalAfterBackendCall("GetMonitoringStats", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetMonitoringStats", async () => {
      let result = this._tryGetResultBeforeBackendCall("GetMonitoringStats");
      if (result === undefined) {
        result = this.backend.getMonitoringStats();
      }
      this._handleIpcInternalAfterBackendCall("GetMonitoringStats", result);
      return result;
    });

    ipc.on("GetMutationHistory", event => {
      let result = this._tryGetResultBeforeBackendCall("GetMutationHistory");
      if (result === undefined) {
        result = this.backend.getMutationHistory();
      }
      this._handleIpcInternalAfterBackendCall("GetMutationHistory", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetMutationHistory", async () => {
      let result = this._tryGetResultBeforeBackendCall("GetMutationHistory");
      if (result === undefined) {
        result = this.backend.getMutationHistory();
      }
      this._handleIpcInternalAfterBackendCall("GetMutationHistory", result);
      return result;
    });

    ipc.on("GetPeers", event => {
      let result = this._tryGetResultBeforeBackendCall("GetPeers");
      if (result === undefined) {
        result = this.backend.getPeers();
      }
      this._handleIpcInternalAfterBackendCall("GetPeers", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetPeers", async () => {
      let result = this._tryGetResultBeforeBackendCall("GetPeers");
      if (result === undefined) {
        result = this.backend.getPeers();
      }
      this._handleIpcInternalAfterBackendCall("GetPeers", result);
      return result;
    });

    ipc.on("GetReceiveAddress", event => {
      let result = this._tryGetResultBeforeBackendCall("GetReceiveAddress");
      if (result === undefined) {
        result = this.backend.GetReceiveAddress();
      }
      this._handleIpcInternalAfterBackendCall("GetReceiveAddress", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetReceiveAddress", async () => {
      let result = this._tryGetResultBeforeBackendCall("GetReceiveAddress");
      if (result === undefined) {
        result = this.backend.GetReceiveAddress();
      }
      this._handleIpcInternalAfterBackendCall("GetReceiveAddress", result);
      return result;
    });

    ipc.on("GetRecoveryPhrase", event => {
      let result = this._tryGetResultBeforeBackendCall("GetRecoveryPhrase");
      if (result === undefined) {
        result = this.backend.GetRecoveryPhrase();
      }
      this._handleIpcInternalAfterBackendCall("GetRecoveryPhrase", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetRecoveryPhrase", async () => {
      let result = this._tryGetResultBeforeBackendCall("GetRecoveryPhrase");
      if (result === undefined) {
        result = this.backend.GetRecoveryPhrase();
      }
      this._handleIpcInternalAfterBackendCall("GetRecoveryPhrase", result);
      return result;
    });

    ipc.on("GetTransaction", (event, txHash) => {
      let result = this._tryGetResultBeforeBackendCall("GetTransaction");
      if (result === undefined) {
        result = this.backend.getTransaction(txHash);
      }
      this._handleIpcInternalAfterBackendCall("GetTransaction", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetTransaction", async data => {
      let result = this._tryGetResultBeforeBackendCall("GetTransaction");
      if (result === undefined) {
        result = this.backend.getTransaction(data.txHash);
      }
      this._handleIpcInternalAfterBackendCall("GetTransaction", result);
      return result;
    });

    ipc.on("GetTransactionHistory", event => {
      let result = this._tryGetResultBeforeBackendCall("GetTransactionHistory");
      if (result === undefined) {
        result = this.backend.getTransactionHistory();
      }
      this._handleIpcInternalAfterBackendCall("GetTransactionHistory", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetTransactionHistory", async () => {
      let result = this._tryGetResultBeforeBackendCall("GetTransactionHistory");
      if (result === undefined) {
        result = this.backend.getTransactionHistory();
      }
      this._handleIpcInternalAfterBackendCall("GetTransactionHistory", result);
      return result;
    });

    ipc.on("GetUnifiedProgress", event => {
      let result = this._tryGetResultBeforeBackendCall("GetUnifiedProgress");
      if (result === undefined) {
        result = this.backend.getUnifiedProgress();
      }
      this._handleIpcInternalAfterBackendCall("GetUnifiedProgress", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetUnifiedProgress", async () => {
      let result = this._tryGetResultBeforeBackendCall("GetUnifiedProgress");
      if (result === undefined) {
        result = this.backend.getUnifiedProgress();
      }
      this._handleIpcInternalAfterBackendCall("GetUnifiedProgress", result);
      return result;
    });

    ipc.on("HaveUnconfirmedFunds", event => {
      let result = this._tryGetResultBeforeBackendCall("HaveUnconfirmedFunds");
      if (result === undefined) {
        result = this.backend.HaveUnconfirmedFunds();
      }
      this._handleIpcInternalAfterBackendCall("HaveUnconfirmedFunds", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("HaveUnconfirmedFunds", async () => {
      let result = this._tryGetResultBeforeBackendCall("HaveUnconfirmedFunds");
      if (result === undefined) {
        result = this.backend.HaveUnconfirmedFunds();
      }
      this._handleIpcInternalAfterBackendCall("HaveUnconfirmedFunds", result);
      return result;
    });

    ipc.on("InitWalletFromRecoveryPhrase", (event, phrase, password) => {
      let result = this._tryGetResultBeforeBackendCall(
        "InitWalletFromRecoveryPhrase"
      );
      if (result === undefined) {
        result = this.backend.InitWalletFromRecoveryPhrase(phrase, password);
      }
      this._handleIpcInternalAfterBackendCall(
        "InitWalletFromRecoveryPhrase",
        result
      );
      event.returnValue = result;
    });

    ipc.answerRenderer("InitWalletFromRecoveryPhrase", async data => {
      let result = this._tryGetResultBeforeBackendCall(
        "InitWalletFromRecoveryPhrase"
      );
      if (result === undefined) {
        result = this.backend.InitWalletFromRecoveryPhrase(
          data.phrase,
          data.password
        );
      }
      this._handleIpcInternalAfterBackendCall(
        "InitWalletFromRecoveryPhrase",
        result
      );
      return result;
    });

    ipc.on("IsMnemonicCorrect", (event, phrase) => {
      let result = this._tryGetResultBeforeBackendCall("IsMnemonicCorrect");
      if (result === undefined) {
        result = this.backend.IsMnemonicCorrect(phrase);
      }
      this._handleIpcInternalAfterBackendCall("IsMnemonicCorrect", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("IsMnemonicCorrect", async data => {
      let result = this._tryGetResultBeforeBackendCall("IsMnemonicCorrect");
      if (result === undefined) {
        result = this.backend.IsMnemonicCorrect(data.phrase);
      }
      this._handleIpcInternalAfterBackendCall("IsMnemonicCorrect", result);
      return result;
    });

    ipc.on("IsMnemonicWallet", event => {
      let result = this._tryGetResultBeforeBackendCall("IsMnemonicWallet");
      if (result === undefined) {
        result = this.backend.IsMnemonicWallet();
      }
      this._handleIpcInternalAfterBackendCall("IsMnemonicWallet", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("IsMnemonicWallet", async () => {
      let result = this._tryGetResultBeforeBackendCall("IsMnemonicWallet");
      if (result === undefined) {
        result = this.backend.IsMnemonicWallet();
      }
      this._handleIpcInternalAfterBackendCall("IsMnemonicWallet", result);
      return result;
    });

    ipc.on("IsValidLinkURI", (event, phrase) => {
      let result = this._tryGetResultBeforeBackendCall("IsValidLinkURI");
      if (result === undefined) {
        result = this.backend.IsValidLinkURI(phrase);
      }
      this._handleIpcInternalAfterBackendCall("IsValidLinkURI", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("IsValidLinkURI", async data => {
      let result = this._tryGetResultBeforeBackendCall("IsValidLinkURI");
      if (result === undefined) {
        result = this.backend.IsValidLinkURI(data.phrase);
      }
      this._handleIpcInternalAfterBackendCall("IsValidLinkURI", result);
      return result;
    });

    ipc.on("IsValidRecipient", (event, request) => {
      let result = this._tryGetResultBeforeBackendCall("IsValidRecipient");
      if (result === undefined) {
        result = this.backend.IsValidRecipient(request);
      }
      this._handleIpcInternalAfterBackendCall("IsValidRecipient", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("IsValidRecipient", async data => {
      let result = this._tryGetResultBeforeBackendCall("IsValidRecipient");
      if (result === undefined) {
        result = this.backend.IsValidRecipient(data.request);
      }
      this._handleIpcInternalAfterBackendCall("IsValidRecipient", result);
      return result;
    });

    ipc.on("IsValidRecoveryPhrase", (event, phrase) => {
      let result = this._tryGetResultBeforeBackendCall("IsValidRecoveryPhrase");
      if (result === undefined) {
        result = this.backend.IsValidRecoveryPhrase(phrase);
      }
      this._handleIpcInternalAfterBackendCall("IsValidRecoveryPhrase", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("IsValidRecoveryPhrase", async data => {
      let result = this._tryGetResultBeforeBackendCall("IsValidRecoveryPhrase");
      if (result === undefined) {
        result = this.backend.IsValidRecoveryPhrase(data.phrase);
      }
      this._handleIpcInternalAfterBackendCall("IsValidRecoveryPhrase", result);
      return result;
    });

    ipc.on("LockWallet", event => {
      let result = this._tryGetResultBeforeBackendCall("LockWallet");
      if (result === undefined) {
        result = this.backend.LockWallet();
      }
      this._handleIpcInternalAfterBackendCall("LockWallet", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("LockWallet", async () => {
      let result = this._tryGetResultBeforeBackendCall("LockWallet");
      if (result === undefined) {
        result = this.backend.LockWallet();
      }
      this._handleIpcInternalAfterBackendCall("LockWallet", result);
      return result;
    });

    ipc.on("PerformPaymentToRecipient", (event, request, substract_fee) => {
      let result = this._tryGetResultBeforeBackendCall(
        "PerformPaymentToRecipient"
      );
      if (result === undefined) {
        result = this.backend.performPaymentToRecipient(request, substract_fee);
      }
      this._handleIpcInternalAfterBackendCall(
        "PerformPaymentToRecipient",
        result
      );
      event.returnValue = result;
    });

    ipc.answerRenderer("PerformPaymentToRecipient", async data => {
      let result = this._tryGetResultBeforeBackendCall(
        "PerformPaymentToRecipient"
      );
      if (result === undefined) {
        result = this.backend.performPaymentToRecipient(
          data.request,
          data.substract_fee
        );
      }
      this._handleIpcInternalAfterBackendCall(
        "PerformPaymentToRecipient",
        result
      );
      return result;
    });

    ipc.on("PersistAndPruneForSPV", event => {
      let result = this._tryGetResultBeforeBackendCall("PersistAndPruneForSPV");
      if (result === undefined) {
        result = this.backend.PersistAndPruneForSPV();
      }
      this._handleIpcInternalAfterBackendCall("PersistAndPruneForSPV", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("PersistAndPruneForSPV", async () => {
      let result = this._tryGetResultBeforeBackendCall("PersistAndPruneForSPV");
      if (result === undefined) {
        result = this.backend.PersistAndPruneForSPV();
      }
      this._handleIpcInternalAfterBackendCall("PersistAndPruneForSPV", result);
      return result;
    });

    ipc.on("QRImageFromString", (event, qr_string, width_hint) => {
      let result = this._tryGetResultBeforeBackendCall("QRImageFromString");
      if (result === undefined) {
        result = this.backend.QRImageFromString(qr_string, width_hint);
      }
      this._handleIpcInternalAfterBackendCall("QRImageFromString", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("QRImageFromString", async data => {
      let result = this._tryGetResultBeforeBackendCall("QRImageFromString");
      if (result === undefined) {
        result = this.backend.QRImageFromString(
          data.qr_string,
          data.width_hint
        );
      }
      this._handleIpcInternalAfterBackendCall("QRImageFromString", result);
      return result;
    });

    ipc.on("RegisterMonitorListener", (event, listener) => {
      let result = this._tryGetResultBeforeBackendCall(
        "RegisterMonitorListener"
      );
      if (result === undefined) {
        result = this.backend.RegisterMonitorListener(listener);
      }
      this._handleIpcInternalAfterBackendCall(
        "RegisterMonitorListener",
        result
      );
      event.returnValue = result;
    });

    ipc.answerRenderer("RegisterMonitorListener", async data => {
      let result = this._tryGetResultBeforeBackendCall(
        "RegisterMonitorListener"
      );
      if (result === undefined) {
        result = this.backend.RegisterMonitorListener(data.listener);
      }
      this._handleIpcInternalAfterBackendCall(
        "RegisterMonitorListener",
        result
      );
      return result;
    });

    ipc.on("ReplaceWalletLinkedFromURI", (event, linked_uri, password) => {
      let result = this._tryGetResultBeforeBackendCall(
        "ReplaceWalletLinkedFromURI"
      );
      if (result === undefined) {
        result = this.backend.ReplaceWalletLinkedFromURI(linked_uri, password);
      }
      this._handleIpcInternalAfterBackendCall(
        "ReplaceWalletLinkedFromURI",
        result
      );
      event.returnValue = result;
    });

    ipc.answerRenderer("ReplaceWalletLinkedFromURI", async data => {
      let result = this._tryGetResultBeforeBackendCall(
        "ReplaceWalletLinkedFromURI"
      );
      if (result === undefined) {
        result = this.backend.ReplaceWalletLinkedFromURI(
          data.linked_uri,
          data.password
        );
      }
      this._handleIpcInternalAfterBackendCall(
        "ReplaceWalletLinkedFromURI",
        result
      );
      return result;
    });

    ipc.on("ResetUnifiedProgress", event => {
      let result = this._tryGetResultBeforeBackendCall("ResetUnifiedProgress");
      if (result === undefined) {
        result = this.backend.ResetUnifiedProgress();
      }
      this._handleIpcInternalAfterBackendCall("ResetUnifiedProgress", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("ResetUnifiedProgress", async () => {
      let result = this._tryGetResultBeforeBackendCall("ResetUnifiedProgress");
      if (result === undefined) {
        result = this.backend.ResetUnifiedProgress();
      }
      this._handleIpcInternalAfterBackendCall("ResetUnifiedProgress", result);
      return result;
    });

    ipc.on("TerminateUnityLib", event => {
      let result = this._tryGetResultBeforeBackendCall("TerminateUnityLib");
      if (result === undefined) {
        result = this.backend.TerminateUnityLib();
      }
      this._handleIpcInternalAfterBackendCall("TerminateUnityLib", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("TerminateUnityLib", async () => {
      let result = this._tryGetResultBeforeBackendCall("TerminateUnityLib");
      if (result === undefined) {
        result = this.backend.TerminateUnityLib();
      }
      this._handleIpcInternalAfterBackendCall("TerminateUnityLib", result);
      return result;
    });

    ipc.on("UnlockWallet", (event, password) => {
      let result = this._tryGetResultBeforeBackendCall("UnlockWallet");
      if (result === undefined) {
        result = this.backend.UnlockWallet(password);
      }
      this._handleIpcInternalAfterBackendCall("UnlockWallet", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("UnlockWallet", async data => {
      let result = this._tryGetResultBeforeBackendCall("UnlockWallet");
      if (result === undefined) {
        result = this.backend.UnlockWallet(data.password);
      }
      this._handleIpcInternalAfterBackendCall("UnlockWallet", result);
      return result;
    });

    ipc.on("UnregisterMonitorListener", (event, listener) => {
      let result = this._tryGetResultBeforeBackendCall(
        "UnregisterMonitorListener"
      );
      if (result === undefined) {
        result = this.backend.UnregisterMonitorListener(listener);
      }
      this._handleIpcInternalAfterBackendCall(
        "UnregisterMonitorListener",
        result
      );
      event.returnValue = result;
    });

    ipc.answerRenderer("UnregisterMonitorListener", async data => {
      let result = this._tryGetResultBeforeBackendCall(
        "UnregisterMonitorListener"
      );
      if (result === undefined) {
        result = this.backend.UnregisterMonitorListener(data.listener);
      }
      this._handleIpcInternalAfterBackendCall(
        "UnregisterMonitorListener",
        result
      );
      return result;
    });
    /* inject:code */
  }
}

export default LibUnity;
