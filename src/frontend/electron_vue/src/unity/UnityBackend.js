import { ipcRenderer as ipc } from "electron-better-ipc";

class UnityBackend {
  // hook to RPC controller
  static ExecuteRpc(command) {
    return ipc.sendSync("ExecuteRpc", command);
  }
  static ExecuteRpcAsync(command) {
    return ipc.callMain("ExecuteRpc", { command });
  }

  /* inject:code */
  static InitWalletFromRecoveryPhrase(phrase, password) {
    return ipc.sendSync("InitWalletFromRecoveryPhrase", phrase, password);
  }

  static InitWalletFromRecoveryPhraseAsync(phrase, password) {
    return ipc.callMain("InitWalletFromRecoveryPhrase", { phrase, password });
  }

  static IsValidRecoveryPhrase(phrase) {
    return ipc.sendSync("IsValidRecoveryPhrase", phrase);
  }

  static IsValidRecoveryPhraseAsync(phrase) {
    return ipc.callMain("IsValidRecoveryPhrase", { phrase });
  }

  static GenerateRecoveryMnemonic() {
    return ipc.sendSync("GenerateRecoveryMnemonic");
  }

  static GenerateRecoveryMnemonicAsync() {
    return ipc.callMain("GenerateRecoveryMnemonic");
  }

  static GetRecoveryPhrase() {
    return ipc.sendSync("GetRecoveryPhrase");
  }

  static GetRecoveryPhraseAsync() {
    return ipc.callMain("GetRecoveryPhrase");
  }

  static GetMnemonicDictionary() {
    return ipc.sendSync("GetMnemonicDictionary");
  }

  static GetMnemonicDictionaryAsync() {
    return ipc.callMain("GetMnemonicDictionary");
  }

  static UnlockWallet(password) {
    return ipc.sendSync("UnlockWallet", password);
  }

  static UnlockWalletAsync(password) {
    return ipc.callMain("UnlockWallet", { password });
  }

  static LockWallet() {
    return ipc.sendSync("LockWallet");
  }

  static LockWalletAsync() {
    return ipc.callMain("LockWallet");
  }

  static ChangePassword(oldPassword, newPassword) {
    return ipc.sendSync("ChangePassword", oldPassword, newPassword);
  }

  static ChangePasswordAsync(oldPassword, newPassword) {
    return ipc.callMain("ChangePassword", { oldPassword, newPassword });
  }

  static IsValidRecipient(request) {
    return ipc.sendSync("IsValidRecipient", request);
  }

  static IsValidRecipientAsync(request) {
    return ipc.callMain("IsValidRecipient", { request });
  }

  static PerformPaymentToRecipient(request, substract_fee) {
    return ipc.sendSync("PerformPaymentToRecipient", request, substract_fee);
  }

  static PerformPaymentToRecipientAsync(request, substract_fee) {
    return ipc.callMain("PerformPaymentToRecipient", {
      request,
      substract_fee
    });
  }

  static GetTransactionHistory() {
    return ipc.sendSync("GetTransactionHistory");
  }

  static GetTransactionHistoryAsync() {
    return ipc.callMain("GetTransactionHistory");
  }

  static ResendTransaction(txHash) {
    return ipc.sendSync("ResendTransaction", txHash);
  }

  static ResendTransactionAsync(txHash) {
    return ipc.callMain("ResendTransaction", { txHash });
  }

  static SetActiveAccount(accountUUID) {
    return ipc.sendSync("SetActiveAccount", accountUUID);
  }

  static SetActiveAccountAsync(accountUUID) {
    return ipc.callMain("SetActiveAccount", { accountUUID });
  }

  static CreateAccount(accountName, accountType) {
    return ipc.sendSync("CreateAccount", accountName, accountType);
  }

  static CreateAccountAsync(accountName, accountType) {
    return ipc.callMain("CreateAccount", { accountName, accountType });
  }

  static StartGeneration(numThreads, memoryLimit) {
    return ipc.sendSync("StartGeneration", numThreads, memoryLimit);
  }

  static StartGenerationAsync(numThreads, memoryLimit) {
    return ipc.callMain("StartGeneration", { numThreads, memoryLimit });
  }

  static StopGeneration() {
    return ipc.sendSync("StopGeneration");
  }

  static StopGenerationAsync() {
    return ipc.callMain("StopGeneration");
  }

  /* inject:code */
}

export default UnityBackend;
