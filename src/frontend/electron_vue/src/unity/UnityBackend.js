import { ipcRenderer as ipc } from "electron-better-ipc";

class UnityBackend {
  /* inject:code */
  /** Add a record to the address book */
  static AddAddressBookRecord(address) {
    return ipc.sendSync("AddAddressBookRecord", address);
  }

  static AddAddressBookRecordAsync(address) {
    return ipc.callMain("AddAddressBookRecord", { address });
  }

  /** Get the build information (ie. commit id and status) */
  static BuildInfo() {
    return ipc.sendSync("BuildInfo");
  }

  static BuildInfoAsync() {
    return ipc.callMain("BuildInfo");
  }

  /** Change the wallet password */
  static ChangePassword(oldPassword, newPassword) {
    return ipc.sendSync("ChangePassword", oldPassword, newPassword);
  }

  static ChangePasswordAsync(oldPassword, newPassword) {
    return ipc.callMain("ChangePassword", { oldPassword, newPassword });
  }

  /** Compute recovery phrase with birth number */
  static ComposeRecoveryPhrase(mnemonic, birthTime) {
    return ipc.sendSync("ComposeRecoveryPhrase", mnemonic, birthTime);
  }

  static ComposeRecoveryPhraseAsync(mnemonic, birthTime) {
    return ipc.callMain("ComposeRecoveryPhrase", { mnemonic, birthTime });
  }

  /** Continue creating wallet that was previously erased using EraseWalletSeedsAndAccounts */
  static ContinueWalletFromRecoveryPhrase(phrase, password) {
    return ipc.sendSync("ContinueWalletFromRecoveryPhrase", phrase, password);
  }

  static ContinueWalletFromRecoveryPhraseAsync(phrase, password) {
    return ipc.callMain("ContinueWalletFromRecoveryPhrase", {
      phrase,
      password
    });
  }

  /** Continue creating wallet that was previously erased using EraseWalletSeedsAndAccounts */
  static ContinueWalletLinkedFromURI(linked_uri, password) {
    return ipc.sendSync("ContinueWalletLinkedFromURI", linked_uri, password);
  }

  static ContinueWalletLinkedFromURIAsync(linked_uri, password) {
    return ipc.callMain("ContinueWalletLinkedFromURI", {
      linked_uri,
      password
    });
  }

  /** Delete a record from the address book */
  static DeleteAddressBookRecord(address) {
    return ipc.sendSync("DeleteAddressBookRecord", address);
  }

  static DeleteAddressBookRecordAsync(address) {
    return ipc.callMain("DeleteAddressBookRecord", { address });
  }

  /** Rescan blockchain for wallet transactions */
  static DoRescan() {
    return ipc.sendSync("DoRescan");
  }

  static DoRescanAsync() {
    return ipc.callMain("DoRescan");
  }

  /**
   * Erase the seeds and accounts of a wallet leaving an empty wallet (with things like the address book intact)
   * After calling this it will be necessary to create a new linked account or recovery phrase account again.
   * NB! This will empty a wallet regardless of whether it has funds in it or not and makes no provisions to check for this - it is the callers responsibility to ensure that erasing the wallet is safe to do in this regard.
   */
  static EraseWalletSeedsAndAccounts() {
    return ipc.sendSync("EraseWalletSeedsAndAccounts");
  }

  static EraseWalletSeedsAndAccountsAsync() {
    return ipc.callMain("EraseWalletSeedsAndAccounts");
  }

  /** Compute the fee required to send amount to given recipient */
  static FeeForRecipient(request) {
    return ipc.sendSync("FeeForRecipient", request);
  }

  static FeeForRecipientAsync(request) {
    return ipc.callMain("FeeForRecipient", { request });
  }

  static GenerateGenesisKeys() {
    return ipc.sendSync("GenerateGenesisKeys");
  }

  static GenerateGenesisKeysAsync() {
    return ipc.callMain("GenerateGenesisKeys");
  }

  /** Generate a new recovery mnemonic */
  static GenerateRecoveryMnemonic() {
    return ipc.sendSync("GenerateRecoveryMnemonic");
  }

  static GenerateRecoveryMnemonicAsync() {
    return ipc.callMain("GenerateRecoveryMnemonic");
  }

  /** Get list of all address book entries */
  static GetAddressBookRecords() {
    return ipc.sendSync("GetAddressBookRecords");
  }

  static GetAddressBookRecordsAsync() {
    return ipc.callMain("GetAddressBookRecords");
  }

  /** Check current wallet balance (including unconfirmed funds) */
  static GetBalance() {
    return ipc.sendSync("GetBalance");
  }

  static GetBalanceAsync() {
    return ipc.callMain("GetBalance");
  }

  /** Get info of last blocks (at most 32) in SPV chain */
  static GetLastSPVBlockInfos() {
    return ipc.sendSync("GetLastSPVBlockInfos");
  }

  static GetLastSPVBlockInfosAsync() {
    return ipc.callMain("GetLastSPVBlockInfos");
  }

  /**
   * Get the 'dictionary' of valid words that a recovery phrase can be composed of
   * NB! Not all combinations of these words are valid
   * Do not use this to generate/compose your own phrases - always use 'GenerateRecoveryMnemonic' for this
   * This function should only be used for input validation/auto-completion
   */
  static GetMnemonicDictionary() {
    return ipc.sendSync("GetMnemonicDictionary");
  }

  static GetMnemonicDictionaryAsync() {
    return ipc.callMain("GetMnemonicDictionary");
  }

  static GetMonitoringStats() {
    return ipc.sendSync("GetMonitoringStats");
  }

  static GetMonitoringStatsAsync() {
    return ipc.callMain("GetMonitoringStats");
  }

  /** Get list of wallet mutations */
  static GetMutationHistory() {
    return ipc.sendSync("GetMutationHistory");
  }

  static GetMutationHistoryAsync() {
    return ipc.callMain("GetMutationHistory");
  }

  /** Get connected peer info */
  static GetPeers() {
    return ipc.sendSync("GetPeers");
  }

  static GetPeersAsync() {
    return ipc.callMain("GetPeers");
  }

  /** Get a receive address from the wallet */
  static GetReceiveAddress() {
    return ipc.sendSync("GetReceiveAddress");
  }

  static GetReceiveAddressAsync() {
    return ipc.callMain("GetReceiveAddress");
  }

  /** Get the recovery phrase for the wallet */
  static GetRecoveryPhrase() {
    return ipc.sendSync("GetRecoveryPhrase");
  }

  static GetRecoveryPhraseAsync() {
    return ipc.callMain("GetRecoveryPhrase");
  }

  /**
   * Get the wallet transaction for the hash
   * Will throw if not found
   */
  static GetTransaction(txHash) {
    return ipc.sendSync("GetTransaction", txHash);
  }

  static GetTransactionAsync(txHash) {
    return ipc.callMain("GetTransaction", { txHash });
  }

  /** Get list of all transactions wallet has been involved in */
  static GetTransactionHistory() {
    return ipc.sendSync("GetTransactionHistory");
  }

  static GetTransactionHistoryAsync() {
    return ipc.callMain("GetTransactionHistory");
  }

  static GetUnifiedProgress() {
    return ipc.sendSync("GetUnifiedProgress");
  }

  static GetUnifiedProgressAsync() {
    return ipc.callMain("GetUnifiedProgress");
  }

  /** Check if the wallet has any transactions that are still pending confirmation, to be used to determine if e.g. it is safe to perform a link or whether we should wait. */
  static HaveUnconfirmedFunds() {
    return ipc.sendSync("HaveUnconfirmedFunds");
  }

  static HaveUnconfirmedFundsAsync() {
    return ipc.callMain("HaveUnconfirmedFunds");
  }

  /** Create the wallet - this should only be called after receiving a `notifyInit...` signal from InitUnityLib */
  static InitWalletFromRecoveryPhrase(phrase, password) {
    return ipc.sendSync("InitWalletFromRecoveryPhrase", phrase, password);
  }

  static InitWalletFromRecoveryPhraseAsync(phrase, password) {
    return ipc.callMain("InitWalletFromRecoveryPhrase", { phrase, password });
  }

  /** Check if the phrase mnemonic is a correct one for the wallet (phrase can be with or without birth time) */
  static IsMnemonicCorrect(phrase) {
    return ipc.sendSync("IsMnemonicCorrect", phrase);
  }

  static IsMnemonicCorrectAsync(phrase) {
    return ipc.callMain("IsMnemonicCorrect", { phrase });
  }

  /** Check if the wallet is using a mnemonic seed ie. recovery phrase (else it is a linked wallet) */
  static IsMnemonicWallet() {
    return ipc.sendSync("IsMnemonicWallet");
  }

  static IsMnemonicWalletAsync() {
    return ipc.callMain("IsMnemonicWallet");
  }

  /** Check link URI for validity */
  static IsValidLinkURI(phrase) {
    return ipc.sendSync("IsValidLinkURI", phrase);
  }

  static IsValidLinkURIAsync(phrase) {
    return ipc.callMain("IsValidLinkURI", { phrase });
  }

  /** Check if text/address is something we are capable of sending money too */
  static IsValidRecipient(request) {
    return ipc.sendSync("IsValidRecipient", request);
  }

  static IsValidRecipientAsync(request) {
    return ipc.callMain("IsValidRecipient", { request });
  }

  /**
   * Check recovery phrase for (syntactic) validity
   * Considered valid if the contained mnemonic is valid and the birth-number is either absent or passes Base-10 checksum
   */
  static IsValidRecoveryPhrase(phrase) {
    return ipc.sendSync("IsValidRecoveryPhrase", phrase);
  }

  static IsValidRecoveryPhraseAsync(phrase) {
    return ipc.callMain("IsValidRecoveryPhrase", { phrase });
  }

  /** Forcefully lock wallet again */
  static LockWallet() {
    return ipc.sendSync("LockWallet");
  }

  static LockWalletAsync() {
    return ipc.callMain("LockWallet");
  }

  /** Attempt to pay a recipient, will throw on failure with description */
  static PerformPaymentToRecipient(request, substract_fee) {
    return ipc.sendSync("PerformPaymentToRecipient", request, substract_fee);
  }

  static PerformPaymentToRecipientAsync(request, substract_fee) {
    return ipc.callMain("PerformPaymentToRecipient", {
      request,
      substract_fee
    });
  }

  /** Interim persist and prune of state. Use at key moments like app backgrounding. */
  static PersistAndPruneForSPV() {
    return ipc.sendSync("PersistAndPruneForSPV");
  }

  static PersistAndPruneForSPVAsync() {
    return ipc.callMain("PersistAndPruneForSPV");
  }

  /** Generate a QR code for a string, QR code will be as close to width_hint as possible when applying simple scaling. */
  static QRImageFromString(qr_string, width_hint) {
    return ipc.sendSync("QRImageFromString", qr_string, width_hint);
  }

  static QRImageFromStringAsync(qr_string, width_hint) {
    return ipc.callMain("QRImageFromString", { qr_string, width_hint });
  }

  static RegisterMonitorListener(listener) {
    return ipc.sendSync("RegisterMonitorListener", listener);
  }

  static RegisterMonitorListenerAsync(listener) {
    return ipc.callMain("RegisterMonitorListener", { listener });
  }

  /** Replace the existing wallet accounts with a new one from a linked URI - only after first emptying the wallet. */
  static ReplaceWalletLinkedFromURI(linked_uri, password) {
    return ipc.sendSync("ReplaceWalletLinkedFromURI", linked_uri, password);
  }

  static ReplaceWalletLinkedFromURIAsync(linked_uri, password) {
    return ipc.callMain("ReplaceWalletLinkedFromURI", { linked_uri, password });
  }

  /**
   * Reset progress notification. In cases where there has been no progress for a long time, but the process
   * is still running the progress can be reset and will represent work to be done from this reset onwards.
   * For example when the process is in the background on iOS for a long long time (but has not been terminated
   * by the OS) this might make more sense then to continue the progress from where it was a day or more ago.
   */
  static ResetUnifiedProgress() {
    return ipc.sendSync("ResetUnifiedProgress");
  }

  static ResetUnifiedProgressAsync() {
    return ipc.callMain("ResetUnifiedProgress");
  }

  /** Stop the library */
  static TerminateUnityLib() {
    return ipc.sendSync("TerminateUnityLib");
  }

  static TerminateUnityLibAsync() {
    return ipc.callMain("TerminateUnityLib");
  }

  /** Unlock wallet */
  static UnlockWallet(password) {
    return ipc.sendSync("UnlockWallet", password);
  }

  static UnlockWalletAsync(password) {
    return ipc.callMain("UnlockWallet", { password });
  }

  static UnregisterMonitorListener(listener) {
    return ipc.sendSync("UnregisterMonitorListener", listener);
  }

  static UnregisterMonitorListenerAsync(listener) {
    return ipc.callMain("UnregisterMonitorListener", { listener });
  }

  /* inject:code */
}

export default UnityBackend;
