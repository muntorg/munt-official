import { ipcRenderer as ipc } from "electron-better-ipc";

/* inject:generated-code */
class LibraryController {
  static async BuildInfoAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.BuildInfoAsync"));
  }

  static BuildInfo() {
    return handleError(ipc.sendSync("NJSILibraryController.BuildInfo"));
  }

  static async InitWalletFromRecoveryPhraseAsync(phrase, password) {
    return handleError(await ipc.callMain("NJSILibraryController.InitWalletFromRecoveryPhraseAsync", { phrase, password }));
  }

  static InitWalletFromRecoveryPhrase(phrase, password) {
    return handleError(ipc.sendSync("NJSILibraryController.InitWalletFromRecoveryPhrase", phrase, password));
  }

  static async IsValidLinkURIAsync(phrase) {
    return handleError(await ipc.callMain("NJSILibraryController.IsValidLinkURIAsync", { phrase }));
  }

  static IsValidLinkURI(phrase) {
    return handleError(ipc.sendSync("NJSILibraryController.IsValidLinkURI", phrase));
  }

  static async ReplaceWalletLinkedFromURIAsync(linked_uri, password) {
    return handleError(await ipc.callMain("NJSILibraryController.ReplaceWalletLinkedFromURIAsync", { linked_uri, password }));
  }

  static ReplaceWalletLinkedFromURI(linked_uri, password) {
    return handleError(ipc.sendSync("NJSILibraryController.ReplaceWalletLinkedFromURI", linked_uri, password));
  }

  static async EraseWalletSeedsAndAccountsAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.EraseWalletSeedsAndAccountsAsync"));
  }

  static EraseWalletSeedsAndAccounts() {
    return handleError(ipc.sendSync("NJSILibraryController.EraseWalletSeedsAndAccounts"));
  }

  static async IsValidRecoveryPhraseAsync(phrase) {
    return handleError(await ipc.callMain("NJSILibraryController.IsValidRecoveryPhraseAsync", { phrase }));
  }

  static IsValidRecoveryPhrase(phrase) {
    return handleError(ipc.sendSync("NJSILibraryController.IsValidRecoveryPhrase", phrase));
  }

  static async GenerateRecoveryMnemonicAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.GenerateRecoveryMnemonicAsync"));
  }

  static GenerateRecoveryMnemonic() {
    return handleError(ipc.sendSync("NJSILibraryController.GenerateRecoveryMnemonic"));
  }

  static async GenerateGenesisKeysAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.GenerateGenesisKeysAsync"));
  }

  static GenerateGenesisKeys() {
    return handleError(ipc.sendSync("NJSILibraryController.GenerateGenesisKeys"));
  }

  static async ComposeRecoveryPhraseAsync(mnemonic, birthTime) {
    return handleError(await ipc.callMain("NJSILibraryController.ComposeRecoveryPhraseAsync", { mnemonic, birthTime }));
  }

  static ComposeRecoveryPhrase(mnemonic, birthTime) {
    return handleError(ipc.sendSync("NJSILibraryController.ComposeRecoveryPhrase", mnemonic, birthTime));
  }

  static async TerminateUnityLibAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.TerminateUnityLibAsync"));
  }

  static TerminateUnityLib() {
    return handleError(ipc.sendSync("NJSILibraryController.TerminateUnityLib"));
  }

  static async QRImageFromStringAsync(qr_string, width_hint) {
    return handleError(await ipc.callMain("NJSILibraryController.QRImageFromStringAsync", { qr_string, width_hint }));
  }

  static QRImageFromString(qr_string, width_hint) {
    return handleError(ipc.sendSync("NJSILibraryController.QRImageFromString", qr_string, width_hint));
  }

  static async GetReceiveAddressAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.GetReceiveAddressAsync"));
  }

  static GetReceiveAddress() {
    return handleError(ipc.sendSync("NJSILibraryController.GetReceiveAddress"));
  }

  static async GetRecoveryPhraseAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.GetRecoveryPhraseAsync"));
  }

  static GetRecoveryPhrase() {
    return handleError(ipc.sendSync("NJSILibraryController.GetRecoveryPhrase"));
  }

  static async IsMnemonicWalletAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.IsMnemonicWalletAsync"));
  }

  static IsMnemonicWallet() {
    return handleError(ipc.sendSync("NJSILibraryController.IsMnemonicWallet"));
  }

  static async IsMnemonicCorrectAsync(phrase) {
    return handleError(await ipc.callMain("NJSILibraryController.IsMnemonicCorrectAsync", { phrase }));
  }

  static IsMnemonicCorrect(phrase) {
    return handleError(ipc.sendSync("NJSILibraryController.IsMnemonicCorrect", phrase));
  }

  static async GetMnemonicDictionaryAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.GetMnemonicDictionaryAsync"));
  }

  static GetMnemonicDictionary() {
    return handleError(ipc.sendSync("NJSILibraryController.GetMnemonicDictionary"));
  }

  static async UnlockWalletAsync(password, timeout_in_seconds) {
    return handleError(await ipc.callMain("NJSILibraryController.UnlockWalletAsync", { password, timeout_in_seconds }));
  }

  static UnlockWallet(password, timeout_in_seconds) {
    return handleError(ipc.sendSync("NJSILibraryController.UnlockWallet", password, timeout_in_seconds));
  }

  static async LockWalletAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.LockWalletAsync"));
  }

  static LockWallet() {
    return handleError(ipc.sendSync("NJSILibraryController.LockWallet"));
  }

  static async GetWalletLockStatusAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.GetWalletLockStatusAsync"));
  }

  static GetWalletLockStatus() {
    return handleError(ipc.sendSync("NJSILibraryController.GetWalletLockStatus"));
  }

  static async ChangePasswordAsync(oldPassword, newPassword) {
    return handleError(await ipc.callMain("NJSILibraryController.ChangePasswordAsync", { oldPassword, newPassword }));
  }

  static ChangePassword(oldPassword, newPassword) {
    return handleError(ipc.sendSync("NJSILibraryController.ChangePassword", oldPassword, newPassword));
  }

  static async DoRescanAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.DoRescanAsync"));
  }

  static DoRescan() {
    return handleError(ipc.sendSync("NJSILibraryController.DoRescan"));
  }

  static async IsValidRecipientAsync(request) {
    return handleError(await ipc.callMain("NJSILibraryController.IsValidRecipientAsync", { request }));
  }

  static IsValidRecipient(request) {
    return handleError(ipc.sendSync("NJSILibraryController.IsValidRecipient", request));
  }

  static async IsValidNativeAddressAsync(address) {
    return handleError(await ipc.callMain("NJSILibraryController.IsValidNativeAddressAsync", { address }));
  }

  static IsValidNativeAddress(address) {
    return handleError(ipc.sendSync("NJSILibraryController.IsValidNativeAddress", address));
  }

  static async IsValidBitcoinAddressAsync(address) {
    return handleError(await ipc.callMain("NJSILibraryController.IsValidBitcoinAddressAsync", { address }));
  }

  static IsValidBitcoinAddress(address) {
    return handleError(ipc.sendSync("NJSILibraryController.IsValidBitcoinAddress", address));
  }

  static async FeeForRecipientAsync(request) {
    return handleError(await ipc.callMain("NJSILibraryController.feeForRecipientAsync", { request }));
  }

  static FeeForRecipient(request) {
    return handleError(ipc.sendSync("NJSILibraryController.feeForRecipient", request));
  }

  static async PerformPaymentToRecipientAsync(request, substract_fee) {
    return handleError(await ipc.callMain("NJSILibraryController.performPaymentToRecipientAsync", { request, substract_fee }));
  }

  static PerformPaymentToRecipient(request, substract_fee) {
    return handleError(ipc.sendSync("NJSILibraryController.performPaymentToRecipient", request, substract_fee));
  }

  static async GetTransactionAsync(txHash) {
    return handleError(await ipc.callMain("NJSILibraryController.getTransactionAsync", { txHash }));
  }

  static GetTransaction(txHash) {
    return handleError(ipc.sendSync("NJSILibraryController.getTransaction", txHash));
  }

  static async ResendTransactionAsync(txHash) {
    return handleError(await ipc.callMain("NJSILibraryController.resendTransactionAsync", { txHash }));
  }

  static ResendTransaction(txHash) {
    return handleError(ipc.sendSync("NJSILibraryController.resendTransaction", txHash));
  }

  static async GetAddressBookRecordsAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.getAddressBookRecordsAsync"));
  }

  static GetAddressBookRecords() {
    return handleError(ipc.sendSync("NJSILibraryController.getAddressBookRecords"));
  }

  static async AddAddressBookRecordAsync(address) {
    return handleError(await ipc.callMain("NJSILibraryController.addAddressBookRecordAsync", { address }));
  }

  static AddAddressBookRecord(address) {
    return handleError(ipc.sendSync("NJSILibraryController.addAddressBookRecord", address));
  }

  static async DeleteAddressBookRecordAsync(address) {
    return handleError(await ipc.callMain("NJSILibraryController.deleteAddressBookRecordAsync", { address }));
  }

  static DeleteAddressBookRecord(address) {
    return handleError(ipc.sendSync("NJSILibraryController.deleteAddressBookRecord", address));
  }

  static async ResetUnifiedProgressAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.ResetUnifiedProgressAsync"));
  }

  static ResetUnifiedProgress() {
    return handleError(ipc.sendSync("NJSILibraryController.ResetUnifiedProgress"));
  }

  static async GetLastSPVBlockInfosAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.getLastSPVBlockInfosAsync"));
  }

  static GetLastSPVBlockInfos() {
    return handleError(ipc.sendSync("NJSILibraryController.getLastSPVBlockInfos"));
  }

  static async GetUnifiedProgressAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.getUnifiedProgressAsync"));
  }

  static GetUnifiedProgress() {
    return handleError(ipc.sendSync("NJSILibraryController.getUnifiedProgress"));
  }

  static async GetMonitoringStatsAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.getMonitoringStatsAsync"));
  }

  static GetMonitoringStats() {
    return handleError(ipc.sendSync("NJSILibraryController.getMonitoringStats"));
  }

  static async RegisterMonitorListenerAsync(listener) {
    return handleError(await ipc.callMain("NJSILibraryController.RegisterMonitorListenerAsync", { listener }));
  }

  static RegisterMonitorListener(listener) {
    return handleError(ipc.sendSync("NJSILibraryController.RegisterMonitorListener", listener));
  }

  static async UnregisterMonitorListenerAsync(listener) {
    return handleError(await ipc.callMain("NJSILibraryController.UnregisterMonitorListenerAsync", { listener }));
  }

  static UnregisterMonitorListener(listener) {
    return handleError(ipc.sendSync("NJSILibraryController.UnregisterMonitorListener", listener));
  }

  static async GetClientInfoAsync() {
    return handleError(await ipc.callMain("NJSILibraryController.getClientInfoAsync"));
  }

  static GetClientInfo() {
    return handleError(ipc.sendSync("NJSILibraryController.getClientInfo"));
  }
}

class WalletController {
  static async HaveUnconfirmedFundsAsync() {
    return handleError(await ipc.callMain("NJSIWalletController.HaveUnconfirmedFundsAsync"));
  }

  static HaveUnconfirmedFunds() {
    return handleError(ipc.sendSync("NJSIWalletController.HaveUnconfirmedFunds"));
  }

  static async GetBalanceSimpleAsync() {
    return handleError(await ipc.callMain("NJSIWalletController.GetBalanceSimpleAsync"));
  }

  static GetBalanceSimple() {
    return handleError(ipc.sendSync("NJSIWalletController.GetBalanceSimple"));
  }

  static async GetBalanceAsync() {
    return handleError(await ipc.callMain("NJSIWalletController.GetBalanceAsync"));
  }

  static GetBalance() {
    return handleError(ipc.sendSync("NJSIWalletController.GetBalance"));
  }

  static async AbandonTransactionAsync(txHash) {
    return handleError(await ipc.callMain("NJSIWalletController.AbandonTransactionAsync", { txHash }));
  }

  static AbandonTransaction(txHash) {
    return handleError(ipc.sendSync("NJSIWalletController.AbandonTransaction", txHash));
  }

  static async GetUUIDAsync() {
    return handleError(await ipc.callMain("NJSIWalletController.GetUUIDAsync"));
  }

  static GetUUID() {
    return handleError(ipc.sendSync("NJSIWalletController.GetUUID"));
  }
}

class RpcController {
  static async ExecuteAsync(command) {
    return handleError(await ipc.callMain("NJSIRpcController.ExecuteAsync", { command }));
  }

  static Execute(command) {
    return handleError(ipc.sendSync("NJSIRpcController.Execute", command));
  }
  static async GetAutocompleteListAsync() {
    return handleError(await ipc.callMain("NJSIRpcController.getAutocompleteListAsync"));
  }

  static GetAutocompleteList() {
    return handleError(ipc.sendSync("NJSIRpcController.getAutocompleteList"));
  }
}

class P2pNetworkController {
  static async DisableNetworkAsync() {
    return handleError(await ipc.callMain("NJSIP2pNetworkController.disableNetworkAsync"));
  }

  static DisableNetwork() {
    return handleError(ipc.sendSync("NJSIP2pNetworkController.disableNetwork"));
  }

  static async EnableNetworkAsync() {
    return handleError(await ipc.callMain("NJSIP2pNetworkController.enableNetworkAsync"));
  }

  static EnableNetwork() {
    return handleError(ipc.sendSync("NJSIP2pNetworkController.enableNetwork"));
  }

  static async GetPeerInfoAsync() {
    return handleError(await ipc.callMain("NJSIP2pNetworkController.getPeerInfoAsync"));
  }

  static GetPeerInfo() {
    return handleError(ipc.sendSync("NJSIP2pNetworkController.getPeerInfo"));
  }
}

class AccountsController {
  static async ListAccountsAsync() {
    return handleError(await ipc.callMain("NJSIAccountsController.listAccountsAsync"));
  }

  static ListAccounts() {
    return handleError(ipc.sendSync("NJSIAccountsController.listAccounts"));
  }

  static async SetActiveAccountAsync(accountUUID) {
    return handleError(await ipc.callMain("NJSIAccountsController.setActiveAccountAsync", { accountUUID }));
  }

  static SetActiveAccount(accountUUID) {
    return handleError(ipc.sendSync("NJSIAccountsController.setActiveAccount", accountUUID));
  }

  static async GetActiveAccountAsync() {
    return handleError(await ipc.callMain("NJSIAccountsController.getActiveAccountAsync"));
  }

  static GetActiveAccount() {
    return handleError(ipc.sendSync("NJSIAccountsController.getActiveAccount"));
  }

  static async CreateAccountAsync(accountName, accountType) {
    return handleError(await ipc.callMain("NJSIAccountsController.createAccountAsync", { accountName, accountType }));
  }

  static CreateAccount(accountName, accountType) {
    return handleError(ipc.sendSync("NJSIAccountsController.createAccount", accountName, accountType));
  }

  static async GetAccountNameAsync(accountUUID) {
    return handleError(await ipc.callMain("NJSIAccountsController.getAccountNameAsync", { accountUUID }));
  }

  static GetAccountName(accountUUID) {
    return handleError(ipc.sendSync("NJSIAccountsController.getAccountName", accountUUID));
  }

  static async RenameAccountAsync(accountUUID, newAccountName) {
    return handleError(await ipc.callMain("NJSIAccountsController.renameAccountAsync", { accountUUID, newAccountName }));
  }

  static RenameAccount(accountUUID, newAccountName) {
    return handleError(ipc.sendSync("NJSIAccountsController.renameAccount", accountUUID, newAccountName));
  }

  static async DeleteAccountAsync(accountUUID) {
    return handleError(await ipc.callMain("NJSIAccountsController.deleteAccountAsync", { accountUUID }));
  }

  static DeleteAccount(accountUUID) {
    return handleError(ipc.sendSync("NJSIAccountsController.deleteAccount", accountUUID));
  }

  static async PurgeAccountAsync(accountUUID) {
    return handleError(await ipc.callMain("NJSIAccountsController.purgeAccountAsync", { accountUUID }));
  }

  static PurgeAccount(accountUUID) {
    return handleError(ipc.sendSync("NJSIAccountsController.purgeAccount", accountUUID));
  }

  static async GetAccountLinkURIAsync(accountUUID) {
    return handleError(await ipc.callMain("NJSIAccountsController.getAccountLinkURIAsync", { accountUUID }));
  }

  static GetAccountLinkURI(accountUUID) {
    return handleError(ipc.sendSync("NJSIAccountsController.getAccountLinkURI", accountUUID));
  }

  static async GetWitnessKeyURIAsync(accountUUID) {
    return handleError(await ipc.callMain("NJSIAccountsController.getWitnessKeyURIAsync", { accountUUID }));
  }

  static GetWitnessKeyURI(accountUUID) {
    return handleError(ipc.sendSync("NJSIAccountsController.getWitnessKeyURI", accountUUID));
  }

  static async CreateAccountFromWitnessKeyURIAsync(witnessKeyURI, newAccountName) {
    return handleError(await ipc.callMain("NJSIAccountsController.createAccountFromWitnessKeyURIAsync", { witnessKeyURI, newAccountName }));
  }

  static CreateAccountFromWitnessKeyURI(witnessKeyURI, newAccountName) {
    return handleError(ipc.sendSync("NJSIAccountsController.createAccountFromWitnessKeyURI", witnessKeyURI, newAccountName));
  }

  static async GetReceiveAddressAsync(accountUUID) {
    return handleError(await ipc.callMain("NJSIAccountsController.getReceiveAddressAsync", { accountUUID }));
  }

  static GetReceiveAddress(accountUUID) {
    return handleError(ipc.sendSync("NJSIAccountsController.getReceiveAddress", accountUUID));
  }

  static async GetTransactionHistoryAsync(accountUUID) {
    return handleError(await ipc.callMain("NJSIAccountsController.getTransactionHistoryAsync", { accountUUID }));
  }

  static GetTransactionHistory(accountUUID) {
    return handleError(ipc.sendSync("NJSIAccountsController.getTransactionHistory", accountUUID));
  }

  static async GetMutationHistoryAsync(accountUUID) {
    return handleError(await ipc.callMain("NJSIAccountsController.getMutationHistoryAsync", { accountUUID }));
  }

  static GetMutationHistory(accountUUID) {
    return handleError(ipc.sendSync("NJSIAccountsController.getMutationHistory", accountUUID));
  }

  static async GetActiveAccountBalanceAsync() {
    return handleError(await ipc.callMain("NJSIAccountsController.getActiveAccountBalanceAsync"));
  }

  static GetActiveAccountBalance() {
    return handleError(ipc.sendSync("NJSIAccountsController.getActiveAccountBalance"));
  }

  static async GetAccountBalanceAsync(accountUUID) {
    return handleError(await ipc.callMain("NJSIAccountsController.getAccountBalanceAsync", { accountUUID }));
  }

  static GetAccountBalance(accountUUID) {
    return handleError(ipc.sendSync("NJSIAccountsController.getAccountBalance", accountUUID));
  }

  static async GetAllAccountBalancesAsync() {
    return handleError(await ipc.callMain("NJSIAccountsController.getAllAccountBalancesAsync"));
  }

  static GetAllAccountBalances() {
    return handleError(ipc.sendSync("NJSIAccountsController.getAllAccountBalances"));
  }

  static async AddAccountLinkAsync(accountUUID, serviceName) {
    return handleError(await ipc.callMain("NJSIAccountsController.addAccountLinkAsync", { accountUUID, serviceName }));
  }

  static AddAccountLink(accountUUID, serviceName) {
    return handleError(ipc.sendSync("NJSIAccountsController.addAccountLink", accountUUID, serviceName));
  }

  static async RemoveAccountLinkAsync(accountUUID, serviceName) {
    return handleError(await ipc.callMain("NJSIAccountsController.removeAccountLinkAsync", { accountUUID, serviceName }));
  }

  static RemoveAccountLink(accountUUID, serviceName) {
    return handleError(ipc.sendSync("NJSIAccountsController.removeAccountLink", accountUUID, serviceName));
  }

  static async ListAccountLinksAsync(accountUUID) {
    return handleError(await ipc.callMain("NJSIAccountsController.listAccountLinksAsync", { accountUUID }));
  }

  static ListAccountLinks(accountUUID) {
    return handleError(ipc.sendSync("NJSIAccountsController.listAccountLinks", accountUUID));
  }
}

class WitnessController {
  static async GetNetworkLimitsAsync() {
    return handleError(await ipc.callMain("NJSIWitnessController.getNetworkLimitsAsync"));
  }

  static GetNetworkLimits() {
    return handleError(ipc.sendSync("NJSIWitnessController.getNetworkLimits"));
  }

  static async GetEstimatedWeightAsync(amount_to_lock, lock_period_in_blocks) {
    return handleError(await ipc.callMain("NJSIWitnessController.getEstimatedWeightAsync", { amount_to_lock, lock_period_in_blocks }));
  }

  static GetEstimatedWeight(amount_to_lock, lock_period_in_blocks) {
    return handleError(ipc.sendSync("NJSIWitnessController.getEstimatedWeight", amount_to_lock, lock_period_in_blocks));
  }

  static async FundWitnessAccountAsync(funding_account_UUID, witness_account_UUID, funding_amount, requestedLockPeriodInBlocks) {
    return handleError(
      await ipc.callMain("NJSIWitnessController.fundWitnessAccountAsync", {
        funding_account_UUID,
        witness_account_UUID,
        funding_amount,
        requestedLockPeriodInBlocks
      })
    );
  }

  static FundWitnessAccount(funding_account_UUID, witness_account_UUID, funding_amount, requestedLockPeriodInBlocks) {
    return handleError(
      ipc.sendSync("NJSIWitnessController.fundWitnessAccount", funding_account_UUID, witness_account_UUID, funding_amount, requestedLockPeriodInBlocks)
    );
  }

  static async RenewWitnessAccountAsync(funding_account_UUID, witness_account_UUID) {
    return handleError(await ipc.callMain("NJSIWitnessController.renewWitnessAccountAsync", { funding_account_UUID, witness_account_UUID }));
  }

  static RenewWitnessAccount(funding_account_UUID, witness_account_UUID) {
    return handleError(ipc.sendSync("NJSIWitnessController.renewWitnessAccount", funding_account_UUID, witness_account_UUID));
  }

  static async GetAccountWitnessStatisticsAsync(witnessAccountUUID) {
    return handleError(await ipc.callMain("NJSIWitnessController.getAccountWitnessStatisticsAsync", { witnessAccountUUID }));
  }

  static GetAccountWitnessStatistics(witnessAccountUUID) {
    return handleError(ipc.sendSync("NJSIWitnessController.getAccountWitnessStatistics", witnessAccountUUID));
  }

  static async SetAccountCompoundingAsync(witnessAccountUUID, percent_to_compount) {
    return handleError(await ipc.callMain("NJSIWitnessController.setAccountCompoundingAsync", { witnessAccountUUID, percent_to_compount }));
  }

  static SetAccountCompounding(witnessAccountUUID, percent_to_compount) {
    return handleError(ipc.sendSync("NJSIWitnessController.setAccountCompounding", witnessAccountUUID, percent_to_compount));
  }

  static async IsAccountCompoundingAsync(witnessAccountUUID) {
    return handleError(await ipc.callMain("NJSIWitnessController.isAccountCompoundingAsync", { witnessAccountUUID }));
  }

  static IsAccountCompounding(witnessAccountUUID) {
    return handleError(ipc.sendSync("NJSIWitnessController.isAccountCompounding", witnessAccountUUID));
  }

  static async GetWitnessAddressAsync(witnessAccountUUID) {
    return handleError(await ipc.callMain("NJSIWitnessController.getWitnessAddressAsync", { witnessAccountUUID }));
  }

  static GetWitnessAddress(witnessAccountUUID) {
    return handleError(ipc.sendSync("NJSIWitnessController.getWitnessAddress", witnessAccountUUID));
  }
}

class GenerationController {
  static async StartGenerationAsync(numThreads, numArenaThreads, memoryLimit) {
    return handleError(await ipc.callMain("NJSIGenerationController.startGenerationAsync", { numThreads, numArenaThreads, memoryLimit }));
  }

  static StartGeneration(numThreads, numArenaThreads, memoryLimit) {
    return handleError(ipc.sendSync("NJSIGenerationController.startGeneration", numThreads, numArenaThreads, memoryLimit));
  }

  static async StopGenerationAsync() {
    return handleError(await ipc.callMain("NJSIGenerationController.stopGenerationAsync"));
  }

  static StopGeneration() {
    return handleError(ipc.sendSync("NJSIGenerationController.stopGeneration"));
  }

  static async GetGenerationAddressAsync() {
    return handleError(await ipc.callMain("NJSIGenerationController.getGenerationAddressAsync"));
  }

  static GetGenerationAddress() {
    return handleError(ipc.sendSync("NJSIGenerationController.getGenerationAddress"));
  }

  static async GetGenerationOverrideAddressAsync() {
    return handleError(await ipc.callMain("NJSIGenerationController.getGenerationOverrideAddressAsync"));
  }

  static GetGenerationOverrideAddress() {
    return handleError(ipc.sendSync("NJSIGenerationController.getGenerationOverrideAddress"));
  }

  static async SetGenerationOverrideAddressAsync(overrideAddress) {
    return handleError(await ipc.callMain("NJSIGenerationController.setGenerationOverrideAddressAsync", { overrideAddress }));
  }

  static SetGenerationOverrideAddress(overrideAddress) {
    return handleError(ipc.sendSync("NJSIGenerationController.setGenerationOverrideAddress", overrideAddress));
  }

  static async GetAvailableCoresAsync() {
    return handleError(await ipc.callMain("NJSIGenerationController.getAvailableCoresAsync"));
  }

  static GetAvailableCores() {
    return handleError(ipc.sendSync("NJSIGenerationController.getAvailableCores"));
  }

  static async GetMinimumMemoryAsync() {
    return handleError(await ipc.callMain("NJSIGenerationController.getMinimumMemoryAsync"));
  }

  static GetMinimumMemory() {
    return handleError(ipc.sendSync("NJSIGenerationController.getMinimumMemory"));
  }

  static async GetMaximumMemoryAsync() {
    return handleError(await ipc.callMain("NJSIGenerationController.getMaximumMemoryAsync"));
  }

  static GetMaximumMemory() {
    return handleError(ipc.sendSync("NJSIGenerationController.getMaximumMemory"));
  }
}

export { LibraryController, WalletController, RpcController, P2pNetworkController, AccountsController, WitnessController, GenerationController };
/* inject:generated-code */

class BackendUtilities {
  static GetBuySessionUrl() {
    return handleError(ipc.sendSync("BackendUtilities.GetBuySessionUrl"));
  }
  static GetSellSessionUrl() {
    return handleError(ipc.sendSync("BackendUtilities.GetSellSessionUrl"));
  }
}

export { BackendUtilities };

function handleError(response) {
  if (response.error) {
    // todo: maybe keep a list of notifications which can be shown
    console.error(response.error);
    return null;
  }
  return response.result;
}
