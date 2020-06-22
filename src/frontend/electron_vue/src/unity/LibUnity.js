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
      fs.mkdirSync(this.options.walletPath);
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

  _setStatus(status) {
    if (status === AppStatus.synchronize) status = AppStatus.ready; // todo: when novo network is available remove this code to show synchronization
    store.dispatch({ type: "SET_STATUS", status: status });
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
      self._setStatus(AppStatus.synchronize);
    };

    signalHandler.notifyInitWithoutExistingWallet = function() {
      console.log("received: notifyInitWithoutExistingWallet");
      self._setStatus(AppStatus.setup);
    };

    signalHandler.notifyShutdown = function() {
      console.log("received: notifyShutdown");
      self.isTerminated = true;
      app.quit();
    };
  }

  _handleIpcInternal(event, result) {
    switch (event) {
      case "InitWalletFromRecoveryPhrase":
        if (result === true) {
          store.dispatch({
            type: "SET_STATUS",
            status: AppStatus.ready // todo: for now set status to ready. when the novo network is available, this needs to be changed to synchronize
          });
        }
        break;
    }
  }

  _registerIpcHandlers() {
    /* inject:code */
    ipc.on("AddAddressBookRecord", (event, address) => {
      let result = this.backend.addAddressBookRecord(address);
      this._handleIpcInternal("AddAddressBookRecord", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("AddAddressBookRecord", async data => {
      let result = this.backend.addAddressBookRecord(data.address);
      this._handleIpcInternal("AddAddressBookRecord", result);
      return result;
    });

    ipc.on("BuildInfo", event => {
      let result = this.backend.BuildInfo();
      this._handleIpcInternal("BuildInfo", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("BuildInfo", async () => {
      let result = this.backend.BuildInfo();
      this._handleIpcInternal("BuildInfo", result);
      return result;
    });

    ipc.on("ChangePassword", (event, oldPassword, newPassword) => {
      let result = this.backend.ChangePassword(oldPassword, newPassword);
      this._handleIpcInternal("ChangePassword", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("ChangePassword", async data => {
      let result = this.backend.ChangePassword(
        data.oldPassword,
        data.newPassword
      );
      this._handleIpcInternal("ChangePassword", result);
      return result;
    });

    ipc.on("ComposeRecoveryPhrase", (event, mnemonic, birthTime) => {
      let result = this.backend.ComposeRecoveryPhrase(mnemonic, birthTime);
      this._handleIpcInternal("ComposeRecoveryPhrase", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("ComposeRecoveryPhrase", async data => {
      let result = this.backend.ComposeRecoveryPhrase(
        data.mnemonic,
        data.birthTime
      );
      this._handleIpcInternal("ComposeRecoveryPhrase", result);
      return result;
    });

    ipc.on("ContinueWalletFromRecoveryPhrase", (event, phrase, password) => {
      let result = this.backend.ContinueWalletFromRecoveryPhrase(
        phrase,
        password
      );
      this._handleIpcInternal("ContinueWalletFromRecoveryPhrase", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("ContinueWalletFromRecoveryPhrase", async data => {
      let result = this.backend.ContinueWalletFromRecoveryPhrase(
        data.phrase,
        data.password
      );
      this._handleIpcInternal("ContinueWalletFromRecoveryPhrase", result);
      return result;
    });

    ipc.on("ContinueWalletLinkedFromURI", (event, linked_uri, password) => {
      let result = this.backend.ContinueWalletLinkedFromURI(
        linked_uri,
        password
      );
      this._handleIpcInternal("ContinueWalletLinkedFromURI", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("ContinueWalletLinkedFromURI", async data => {
      let result = this.backend.ContinueWalletLinkedFromURI(
        data.linked_uri,
        data.password
      );
      this._handleIpcInternal("ContinueWalletLinkedFromURI", result);
      return result;
    });

    ipc.on("DeleteAddressBookRecord", (event, address) => {
      let result = this.backend.deleteAddressBookRecord(address);
      this._handleIpcInternal("DeleteAddressBookRecord", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("DeleteAddressBookRecord", async data => {
      let result = this.backend.deleteAddressBookRecord(data.address);
      this._handleIpcInternal("DeleteAddressBookRecord", result);
      return result;
    });

    ipc.on("DoRescan", event => {
      let result = this.backend.DoRescan();
      this._handleIpcInternal("DoRescan", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("DoRescan", async () => {
      let result = this.backend.DoRescan();
      this._handleIpcInternal("DoRescan", result);
      return result;
    });

    ipc.on("EraseWalletSeedsAndAccounts", event => {
      let result = this.backend.EraseWalletSeedsAndAccounts();
      this._handleIpcInternal("EraseWalletSeedsAndAccounts", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("EraseWalletSeedsAndAccounts", async () => {
      let result = this.backend.EraseWalletSeedsAndAccounts();
      this._handleIpcInternal("EraseWalletSeedsAndAccounts", result);
      return result;
    });

    ipc.on("FeeForRecipient", (event, request) => {
      let result = this.backend.feeForRecipient(request);
      this._handleIpcInternal("FeeForRecipient", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("FeeForRecipient", async data => {
      let result = this.backend.feeForRecipient(data.request);
      this._handleIpcInternal("FeeForRecipient", result);
      return result;
    });

    ipc.on("GenerateGenesisKeys", event => {
      let result = this.backend.GenerateGenesisKeys();
      this._handleIpcInternal("GenerateGenesisKeys", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GenerateGenesisKeys", async () => {
      let result = this.backend.GenerateGenesisKeys();
      this._handleIpcInternal("GenerateGenesisKeys", result);
      return result;
    });

    ipc.on("GenerateRecoveryMnemonic", event => {
      let result = this.backend.GenerateRecoveryMnemonic();
      this._handleIpcInternal("GenerateRecoveryMnemonic", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GenerateRecoveryMnemonic", async () => {
      let result = this.backend.GenerateRecoveryMnemonic();
      this._handleIpcInternal("GenerateRecoveryMnemonic", result);
      return result;
    });

    ipc.on("GetAddressBookRecords", event => {
      let result = this.backend.getAddressBookRecords();
      this._handleIpcInternal("GetAddressBookRecords", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetAddressBookRecords", async () => {
      let result = this.backend.getAddressBookRecords();
      this._handleIpcInternal("GetAddressBookRecords", result);
      return result;
    });

    ipc.on("GetBalance", event => {
      let result = this.backend.GetBalance();
      this._handleIpcInternal("GetBalance", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetBalance", async () => {
      let result = this.backend.GetBalance();
      this._handleIpcInternal("GetBalance", result);
      return result;
    });

    ipc.on("GetLastSPVBlockInfos", event => {
      let result = this.backend.getLastSPVBlockInfos();
      this._handleIpcInternal("GetLastSPVBlockInfos", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetLastSPVBlockInfos", async () => {
      let result = this.backend.getLastSPVBlockInfos();
      this._handleIpcInternal("GetLastSPVBlockInfos", result);
      return result;
    });

    ipc.on("GetMonitoringStats", event => {
      let result = this.backend.getMonitoringStats();
      this._handleIpcInternal("GetMonitoringStats", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetMonitoringStats", async () => {
      let result = this.backend.getMonitoringStats();
      this._handleIpcInternal("GetMonitoringStats", result);
      return result;
    });

    ipc.on("GetMutationHistory", event => {
      let result = this.backend.getMutationHistory();
      this._handleIpcInternal("GetMutationHistory", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetMutationHistory", async () => {
      let result = this.backend.getMutationHistory();
      this._handleIpcInternal("GetMutationHistory", result);
      return result;
    });

    ipc.on("GetPeers", event => {
      let result = this.backend.getPeers();
      this._handleIpcInternal("GetPeers", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetPeers", async () => {
      let result = this.backend.getPeers();
      this._handleIpcInternal("GetPeers", result);
      return result;
    });

    ipc.on("GetReceiveAddress", event => {
      let result = this.backend.GetReceiveAddress();
      this._handleIpcInternal("GetReceiveAddress", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetReceiveAddress", async () => {
      let result = this.backend.GetReceiveAddress();
      this._handleIpcInternal("GetReceiveAddress", result);
      return result;
    });

    ipc.on("GetRecoveryPhrase", event => {
      let result = this.backend.GetRecoveryPhrase();
      this._handleIpcInternal("GetRecoveryPhrase", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetRecoveryPhrase", async () => {
      let result = this.backend.GetRecoveryPhrase();
      this._handleIpcInternal("GetRecoveryPhrase", result);
      return result;
    });

    ipc.on("GetTransaction", (event, txHash) => {
      let result = this.backend.getTransaction(txHash);
      this._handleIpcInternal("GetTransaction", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetTransaction", async data => {
      let result = this.backend.getTransaction(data.txHash);
      this._handleIpcInternal("GetTransaction", result);
      return result;
    });

    ipc.on("GetTransactionHistory", event => {
      let result = this.backend.getTransactionHistory();
      this._handleIpcInternal("GetTransactionHistory", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetTransactionHistory", async () => {
      let result = this.backend.getTransactionHistory();
      this._handleIpcInternal("GetTransactionHistory", result);
      return result;
    });

    ipc.on("GetUnifiedProgress", event => {
      let result = this.backend.getUnifiedProgress();
      this._handleIpcInternal("GetUnifiedProgress", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("GetUnifiedProgress", async () => {
      let result = this.backend.getUnifiedProgress();
      this._handleIpcInternal("GetUnifiedProgress", result);
      return result;
    });

    ipc.on("HaveUnconfirmedFunds", event => {
      let result = this.backend.HaveUnconfirmedFunds();
      this._handleIpcInternal("HaveUnconfirmedFunds", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("HaveUnconfirmedFunds", async () => {
      let result = this.backend.HaveUnconfirmedFunds();
      this._handleIpcInternal("HaveUnconfirmedFunds", result);
      return result;
    });

    ipc.on("InitWalletFromRecoveryPhrase", (event, phrase, password) => {
      let result = this.backend.InitWalletFromRecoveryPhrase(phrase, password);
      this._handleIpcInternal("InitWalletFromRecoveryPhrase", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("InitWalletFromRecoveryPhrase", async data => {
      let result = this.backend.InitWalletFromRecoveryPhrase(
        data.phrase,
        data.password
      );
      this._handleIpcInternal("InitWalletFromRecoveryPhrase", result);
      return result;
    });

    ipc.on("IsMnemonicCorrect", (event, phrase) => {
      let result = this.backend.IsMnemonicCorrect(phrase);
      this._handleIpcInternal("IsMnemonicCorrect", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("IsMnemonicCorrect", async data => {
      let result = this.backend.IsMnemonicCorrect(data.phrase);
      this._handleIpcInternal("IsMnemonicCorrect", result);
      return result;
    });

    ipc.on("IsMnemonicWallet", event => {
      let result = this.backend.IsMnemonicWallet();
      this._handleIpcInternal("IsMnemonicWallet", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("IsMnemonicWallet", async () => {
      let result = this.backend.IsMnemonicWallet();
      this._handleIpcInternal("IsMnemonicWallet", result);
      return result;
    });

    ipc.on("IsValidLinkURI", (event, phrase) => {
      let result = this.backend.IsValidLinkURI(phrase);
      this._handleIpcInternal("IsValidLinkURI", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("IsValidLinkURI", async data => {
      let result = this.backend.IsValidLinkURI(data.phrase);
      this._handleIpcInternal("IsValidLinkURI", result);
      return result;
    });

    ipc.on("IsValidRecipient", (event, request) => {
      let result = this.backend.IsValidRecipient(request);
      this._handleIpcInternal("IsValidRecipient", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("IsValidRecipient", async data => {
      let result = this.backend.IsValidRecipient(data.request);
      this._handleIpcInternal("IsValidRecipient", result);
      return result;
    });

    ipc.on("IsValidRecoveryPhrase", (event, phrase) => {
      let result = this.backend.IsValidRecoveryPhrase(phrase);
      this._handleIpcInternal("IsValidRecoveryPhrase", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("IsValidRecoveryPhrase", async data => {
      let result = this.backend.IsValidRecoveryPhrase(data.phrase);
      this._handleIpcInternal("IsValidRecoveryPhrase", result);
      return result;
    });

    ipc.on("LockWallet", event => {
      let result = this.backend.LockWallet();
      this._handleIpcInternal("LockWallet", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("LockWallet", async () => {
      let result = this.backend.LockWallet();
      this._handleIpcInternal("LockWallet", result);
      return result;
    });

    ipc.on("PerformPaymentToRecipient", (event, request, substract_fee) => {
      let result = this.backend.performPaymentToRecipient(
        request,
        substract_fee
      );
      this._handleIpcInternal("PerformPaymentToRecipient", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("PerformPaymentToRecipient", async data => {
      let result = this.backend.performPaymentToRecipient(
        data.request,
        data.substract_fee
      );
      this._handleIpcInternal("PerformPaymentToRecipient", result);
      return result;
    });

    ipc.on("PersistAndPruneForSPV", event => {
      let result = this.backend.PersistAndPruneForSPV();
      this._handleIpcInternal("PersistAndPruneForSPV", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("PersistAndPruneForSPV", async () => {
      let result = this.backend.PersistAndPruneForSPV();
      this._handleIpcInternal("PersistAndPruneForSPV", result);
      return result;
    });

    ipc.on("QRImageFromString", (event, qr_string, width_hint) => {
      let result = this.backend.QRImageFromString(qr_string, width_hint);
      this._handleIpcInternal("QRImageFromString", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("QRImageFromString", async data => {
      let result = this.backend.QRImageFromString(
        data.qr_string,
        data.width_hint
      );
      this._handleIpcInternal("QRImageFromString", result);
      return result;
    });

    ipc.on("RegisterMonitorListener", (event, listener) => {
      let result = this.backend.RegisterMonitorListener(listener);
      this._handleIpcInternal("RegisterMonitorListener", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("RegisterMonitorListener", async data => {
      let result = this.backend.RegisterMonitorListener(data.listener);
      this._handleIpcInternal("RegisterMonitorListener", result);
      return result;
    });

    ipc.on("ReplaceWalletLinkedFromURI", (event, linked_uri, password) => {
      let result = this.backend.ReplaceWalletLinkedFromURI(
        linked_uri,
        password
      );
      this._handleIpcInternal("ReplaceWalletLinkedFromURI", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("ReplaceWalletLinkedFromURI", async data => {
      let result = this.backend.ReplaceWalletLinkedFromURI(
        data.linked_uri,
        data.password
      );
      this._handleIpcInternal("ReplaceWalletLinkedFromURI", result);
      return result;
    });

    ipc.on("ResetUnifiedProgress", event => {
      let result = this.backend.ResetUnifiedProgress();
      this._handleIpcInternal("ResetUnifiedProgress", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("ResetUnifiedProgress", async () => {
      let result = this.backend.ResetUnifiedProgress();
      this._handleIpcInternal("ResetUnifiedProgress", result);
      return result;
    });

    ipc.on("TerminateUnityLib", event => {
      let result = this.backend.TerminateUnityLib();
      this._handleIpcInternal("TerminateUnityLib", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("TerminateUnityLib", async () => {
      let result = this.backend.TerminateUnityLib();
      this._handleIpcInternal("TerminateUnityLib", result);
      return result;
    });

    ipc.on("UnlockWallet", (event, password) => {
      let result = this.backend.UnlockWallet(password);
      this._handleIpcInternal("UnlockWallet", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("UnlockWallet", async data => {
      let result = this.backend.UnlockWallet(data.password);
      this._handleIpcInternal("UnlockWallet", result);
      return result;
    });

    ipc.on("UnregisterMonitorListener", (event, listener) => {
      let result = this.backend.UnregisterMonitorListener(listener);
      this._handleIpcInternal("UnregisterMonitorListener", result);
      event.returnValue = result;
    });

    ipc.answerRenderer("UnregisterMonitorListener", async data => {
      let result = this.backend.UnregisterMonitorListener(data.listener);
      this._handleIpcInternal("UnregisterMonitorListener", result);
      return result;
    });
    /* inject:code */
  }
}

export default LibUnity;
