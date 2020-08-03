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

  static IsValidNativeAddress(address) {
    return ipc.sendSync("IsValidNativeAddress", address);
  }

  static IsValidNativeAddressAsync(address) {
    return ipc.callMain("IsValidNativeAddress", { address });
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

  static ResendTransaction(txHash) {
    return ipc.sendSync("ResendTransaction", txHash);
  }

  static ResendTransactionAsync(txHash) {
    return ipc.callMain("ResendTransaction", { txHash });
  }

  static GetClientInfo() {
    return ipc.sendSync("GetClientInfo");
  }

  static GetClientInfoAsync() {
    return ipc.callMain("GetClientInfo");
  }

  static GetTransactionHistory() {
    return ipc.sendSync("GetTransactionHistory");
  }

  static GetTransactionHistoryAsync() {
    return ipc.callMain("GetTransactionHistory");
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

  static DeleteAccount(accountUUID) {
    return ipc.sendSync("DeleteAccount", accountUUID);
  }

  static DeleteAccountAsync(accountUUID) {
    return ipc.callMain("DeleteAccount", { accountUUID });
  }

  static GetActiveAccountBalance() {
    return ipc.sendSync("GetActiveAccountBalance");
  }

  static GetActiveAccountBalanceAsync() {
    return ipc.callMain("GetActiveAccountBalance");
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

  static GetAvailableCores() {
    return ipc.sendSync("GetAvailableCores");
  }

  static GetAvailableCoresAsync() {
    return ipc.callMain("GetAvailableCores");
  }

  static GetMinimumMemory() {
    return ipc.sendSync("GetMinimumMemory");
  }

  static GetMinimumMemoryAsync() {
    return ipc.callMain("GetMinimumMemory");
  }

  static GetMaximumMemory() {
    return ipc.sendSync("GetMaximumMemory");
  }

  static GetMaximumMemoryAsync() {
    return ipc.callMain("GetMaximumMemory");
  }

  static GetNetworkLimits() {
    return ipc.sendSync("GetNetworkLimits");
  }

  static GetNetworkLimitsAsync() {
    return ipc.callMain("GetNetworkLimits");
  }

  static GetEstimatedWeight(amount_to_lock, lock_period_in_blocks) {
    return ipc.sendSync(
      "GetEstimatedWeight",
      amount_to_lock,
      lock_period_in_blocks
    );
  }

  static GetEstimatedWeightAsync(amount_to_lock, lock_period_in_blocks) {
    return ipc.callMain("GetEstimatedWeight", {
      amount_to_lock,
      lock_period_in_blocks
    });
  }

  static FundWitnessAccount(
    funding_account_UUID,
    witness_account_UUID,
    funding_amount,
    requestedLockPeriodInBlocks
  ) {
    return ipc.sendSync(
      "FundWitnessAccount",
      funding_account_UUID,
      witness_account_UUID,
      funding_amount,
      requestedLockPeriodInBlocks
    );
  }

  static FundWitnessAccountAsync(
    funding_account_UUID,
    witness_account_UUID,
    funding_amount,
    requestedLockPeriodInBlocks
  ) {
    return ipc.callMain("FundWitnessAccount", {
      funding_account_UUID,
      witness_account_UUID,
      funding_amount,
      requestedLockPeriodInBlocks
    });
  }

  static GetAccountWitnessStatistics(witnessAccountUUID) {
    return ipc.sendSync("GetAccountWitnessStatistics", witnessAccountUUID);
  }

  static GetAccountWitnessStatisticsAsync(witnessAccountUUID) {
    return ipc.callMain("GetAccountWitnessStatistics", { witnessAccountUUID });
  }

  static SetAccountCompounding(witnessAccountUUID, should_compound) {
    return ipc.sendSync(
      "SetAccountCompounding",
      witnessAccountUUID,
      should_compound
    );
  }

  static SetAccountCompoundingAsync(witnessAccountUUID, should_compound) {
    return ipc.callMain("SetAccountCompounding", {
      witnessAccountUUID,
      should_compound
    });
  }

  static IsAccountCompounding(witnessAccountUUID) {
    return ipc.sendSync("IsAccountCompounding", witnessAccountUUID);
  }

  static IsAccountCompoundingAsync(witnessAccountUUID) {
    return ipc.callMain("IsAccountCompounding", { witnessAccountUUID });
  }

  /* inject:code */
}

export default UnityBackend;
