import { ipcRenderer as ipc } from "electron";

/* inject:generated-code */
class LibraryController {
  static BuildInfo() {
    return ipc.sendSync("NJSILibraryController.BuildInfo");
  }

  static InitWalletFromRecoveryPhrase(phrase, password) {
    return ipc.sendSync(
      "NJSILibraryController.InitWalletFromRecoveryPhrase",
      phrase,
      password
    );
  }

  static IsValidLinkURI(phrase) {
    return ipc.sendSync("NJSILibraryController.IsValidLinkURI", phrase);
  }

  static ReplaceWalletLinkedFromURI(linked_uri, password) {
    return ipc.sendSync(
      "NJSILibraryController.ReplaceWalletLinkedFromURI",
      linked_uri,
      password
    );
  }

  static EraseWalletSeedsAndAccounts() {
    return ipc.sendSync("NJSILibraryController.EraseWalletSeedsAndAccounts");
  }

  static IsValidRecoveryPhrase(phrase) {
    return ipc.sendSync("NJSILibraryController.IsValidRecoveryPhrase", phrase);
  }

  static GenerateRecoveryMnemonic() {
    return ipc.sendSync("NJSILibraryController.GenerateRecoveryMnemonic");
  }

  static GenerateGenesisKeys() {
    return ipc.sendSync("NJSILibraryController.GenerateGenesisKeys");
  }

  static ComposeRecoveryPhrase(mnemonic, birthTime) {
    return ipc.sendSync(
      "NJSILibraryController.ComposeRecoveryPhrase",
      mnemonic,
      birthTime
    );
  }

  static TerminateUnityLib() {
    return ipc.sendSync("NJSILibraryController.TerminateUnityLib");
  }

  static QRImageFromString(qr_string, width_hint) {
    return ipc.sendSync(
      "NJSILibraryController.QRImageFromString",
      qr_string,
      width_hint
    );
  }

  static GetReceiveAddress() {
    return ipc.sendSync("NJSILibraryController.GetReceiveAddress");
  }

  static GetRecoveryPhrase() {
    return ipc.sendSync("NJSILibraryController.GetRecoveryPhrase");
  }

  static IsMnemonicWallet() {
    return ipc.sendSync("NJSILibraryController.IsMnemonicWallet");
  }

  static IsMnemonicCorrect(phrase) {
    return ipc.sendSync("NJSILibraryController.IsMnemonicCorrect", phrase);
  }

  static GetMnemonicDictionary() {
    return ipc.sendSync("NJSILibraryController.GetMnemonicDictionary");
  }

  static UnlockWallet(password) {
    return ipc.sendSync("NJSILibraryController.UnlockWallet", password);
  }

  static LockWallet() {
    return ipc.sendSync("NJSILibraryController.LockWallet");
  }

  static ChangePassword(oldPassword, newPassword) {
    return ipc.sendSync(
      "NJSILibraryController.ChangePassword",
      oldPassword,
      newPassword
    );
  }

  static DoRescan() {
    return ipc.sendSync("NJSILibraryController.DoRescan");
  }

  static IsValidRecipient(request) {
    return ipc.sendSync("NJSILibraryController.IsValidRecipient", request);
  }

  static IsValidNativeAddress(address) {
    return ipc.sendSync("NJSILibraryController.IsValidNativeAddress", address);
  }

  static IsValidBitcoinAddress(address) {
    return ipc.sendSync("NJSILibraryController.IsValidBitcoinAddress", address);
  }

  static FeeForRecipient(request) {
    return ipc.sendSync("NJSILibraryController.feeForRecipient", request);
  }

  static PerformPaymentToRecipient(request, substract_fee) {
    return ipc.sendSync(
      "NJSILibraryController.performPaymentToRecipient",
      request,
      substract_fee
    );
  }

  static GetTransaction(txHash) {
    return ipc.sendSync("NJSILibraryController.getTransaction", txHash);
  }

  static ResendTransaction(txHash) {
    return ipc.sendSync("NJSILibraryController.resendTransaction", txHash);
  }

  static GetAddressBookRecords() {
    return ipc.sendSync("NJSILibraryController.getAddressBookRecords");
  }

  static AddAddressBookRecord(address) {
    return ipc.sendSync("NJSILibraryController.addAddressBookRecord", address);
  }

  static DeleteAddressBookRecord(address) {
    return ipc.sendSync(
      "NJSILibraryController.deleteAddressBookRecord",
      address
    );
  }

  static ResetUnifiedProgress() {
    return ipc.sendSync("NJSILibraryController.ResetUnifiedProgress");
  }

  static GetLastSPVBlockInfos() {
    return ipc.sendSync("NJSILibraryController.getLastSPVBlockInfos");
  }

  static GetUnifiedProgress() {
    return ipc.sendSync("NJSILibraryController.getUnifiedProgress");
  }

  static GetMonitoringStats() {
    return ipc.sendSync("NJSILibraryController.getMonitoringStats");
  }

  static RegisterMonitorListener(listener) {
    return ipc.sendSync(
      "NJSILibraryController.RegisterMonitorListener",
      listener
    );
  }

  static UnregisterMonitorListener(listener) {
    return ipc.sendSync(
      "NJSILibraryController.UnregisterMonitorListener",
      listener
    );
  }

  static GetClientInfo() {
    return ipc.sendSync("NJSILibraryController.getClientInfo");
  }
}

class WalletController {
  static HaveUnconfirmedFunds() {
    return ipc.sendSync("NJSIWalletController.HaveUnconfirmedFunds");
  }

  static GetBalanceSimple() {
    return ipc.sendSync("NJSIWalletController.GetBalanceSimple");
  }

  static GetBalance() {
    return ipc.sendSync("NJSIWalletController.GetBalance");
  }
}

class RpcController {
  static Execute(command) {
    return ipc.sendSync("NJSIRpcController.Execute", command);
  }
  static GetAutocompleteList() {
    return ipc.sendSync("NJSIRpcController.getAutocompleteList");
  }
}

class P2pNetworkController {
  static DisableNetwork() {
    return ipc.sendSync("NJSIP2pNetworkController.disableNetwork");
  }

  static EnableNetwork() {
    return ipc.sendSync("NJSIP2pNetworkController.enableNetwork");
  }

  static GetPeerInfo() {
    return ipc.sendSync("NJSIP2pNetworkController.getPeerInfo");
  }
}

class AccountsController {
  static ListAccounts() {
    return ipc.sendSync("NJSIAccountsController.listAccounts");
  }

  static SetActiveAccount(accountUUID) {
    return ipc.sendSync("NJSIAccountsController.setActiveAccount", accountUUID);
  }

  static GetActiveAccount() {
    return ipc.sendSync("NJSIAccountsController.getActiveAccount");
  }

  static CreateAccount(accountName, accountType) {
    return ipc.sendSync(
      "NJSIAccountsController.createAccount",
      accountName,
      accountType
    );
  }

  static RenameAccount(accountUUID, newAccountName) {
    return ipc.sendSync(
      "NJSIAccountsController.renameAccount",
      accountUUID,
      newAccountName
    );
  }

  static DeleteAccount(accountUUID) {
    return ipc.sendSync("NJSIAccountsController.deleteAccount", accountUUID);
  }

  static PurgeAccount(accountUUID) {
    return ipc.sendSync("NJSIAccountsController.purgeAccount", accountUUID);
  }

  static GetAccountLinkURI(accountUUID) {
    return ipc.sendSync(
      "NJSIAccountsController.getAccountLinkURI",
      accountUUID
    );
  }

  static GetWitnessKeyURI(accountUUID) {
    return ipc.sendSync("NJSIAccountsController.getWitnessKeyURI", accountUUID);
  }

  static CreateAccountFromWitnessKeyURI(witnessKeyURI, newAccountName) {
    return ipc.sendSync(
      "NJSIAccountsController.createAccountFromWitnessKeyURI",
      witnessKeyURI,
      newAccountName
    );
  }

  static GetReceiveAddress(accountUUID) {
    return ipc.sendSync(
      "NJSIAccountsController.getReceiveAddress",
      accountUUID
    );
  }

  static GetTransactionHistory(accountUUID) {
    return ipc.sendSync(
      "NJSIAccountsController.getTransactionHistory",
      accountUUID
    );
  }

  static GetMutationHistory(accountUUID) {
    return ipc.sendSync(
      "NJSIAccountsController.getMutationHistory",
      accountUUID
    );
  }

  static GetActiveAccountBalance() {
    return ipc.sendSync("NJSIAccountsController.getActiveAccountBalance");
  }

  static GetAccountBalance(accountUUID) {
    return ipc.sendSync(
      "NJSIAccountsController.getAccountBalance",
      accountUUID
    );
  }

  static GetAllAccountBalances() {
    return ipc.sendSync("NJSIAccountsController.getAllAccountBalances");
  }
}

class WitnessController {
  static GetNetworkLimits() {
    return ipc.sendSync("NJSIWitnessController.getNetworkLimits");
  }

  static GetEstimatedWeight(amount_to_lock, lock_period_in_blocks) {
    return ipc.sendSync(
      "NJSIWitnessController.getEstimatedWeight",
      amount_to_lock,
      lock_period_in_blocks
    );
  }

  static FundWitnessAccount(
    funding_account_UUID,
    witness_account_UUID,
    funding_amount,
    requestedLockPeriodInBlocks
  ) {
    return ipc.sendSync(
      "NJSIWitnessController.fundWitnessAccount",
      funding_account_UUID,
      witness_account_UUID,
      funding_amount,
      requestedLockPeriodInBlocks
    );
  }

  static RenewWitnessAccount(funding_account_UUID, witness_account_UUID) {
    return ipc.sendSync(
      "NJSIWitnessController.renewWitnessAccount",
      funding_account_UUID,
      witness_account_UUID
    );
  }

  static GetAccountWitnessStatistics(witnessAccountUUID) {
    return ipc.sendSync(
      "NJSIWitnessController.getAccountWitnessStatistics",
      witnessAccountUUID
    );
  }

  static SetAccountCompounding(witnessAccountUUID, should_compound) {
    return ipc.sendSync(
      "NJSIWitnessController.setAccountCompounding",
      witnessAccountUUID,
      should_compound
    );
  }

  static IsAccountCompounding(witnessAccountUUID) {
    return ipc.sendSync(
      "NJSIWitnessController.isAccountCompounding",
      witnessAccountUUID
    );
  }
}

class GenerationController {
  static StartGeneration(numThreads, memoryLimit) {
    return ipc.sendSync(
      "NJSIGenerationController.startGeneration",
      numThreads,
      memoryLimit
    );
  }

  static StopGeneration() {
    return ipc.sendSync("NJSIGenerationController.stopGeneration");
  }

  static GetGenerationAddress() {
    return ipc.sendSync("NJSIGenerationController.getGenerationAddress");
  }

  static GetGenerationOverrideAddress() {
    return ipc.sendSync(
      "NJSIGenerationController.getGenerationOverrideAddress"
    );
  }

  static SetGenerationOverrideAddress(overrideAddress) {
    return ipc.sendSync(
      "NJSIGenerationController.setGenerationOverrideAddress",
      overrideAddress
    );
  }

  static GetAvailableCores() {
    return ipc.sendSync("NJSIGenerationController.getAvailableCores");
  }

  static GetMinimumMemory() {
    return ipc.sendSync("NJSIGenerationController.getMinimumMemory");
  }

  static GetMaximumMemory() {
    return ipc.sendSync("NJSIGenerationController.getMaximumMemory");
  }
}

export {
  LibraryController,
  WalletController,
  RpcController,
  P2pNetworkController,
  AccountsController,
  WitnessController,
  GenerationController
};
/* inject:generated-code */
