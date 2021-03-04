import { ipcRenderer as ipc } from "electron";

/* inject:generated-code */
class LibraryController {
  static BuildInfo() {
    return handleError(ipc.sendSync("NJSILibraryController.BuildInfo"));
  }

  static InitWalletFromRecoveryPhrase(phrase, password) {
    return handleError(
      ipc.sendSync(
        "NJSILibraryController.InitWalletFromRecoveryPhrase",
        phrase,
        password
      )
    );
  }

  static IsValidLinkURI(phrase) {
    return handleError(
      ipc.sendSync("NJSILibraryController.IsValidLinkURI", phrase)
    );
  }

  static ReplaceWalletLinkedFromURI(linked_uri, password) {
    return handleError(
      ipc.sendSync(
        "NJSILibraryController.ReplaceWalletLinkedFromURI",
        linked_uri,
        password
      )
    );
  }

  static EraseWalletSeedsAndAccounts() {
    return handleError(
      ipc.sendSync("NJSILibraryController.EraseWalletSeedsAndAccounts")
    );
  }

  static IsValidRecoveryPhrase(phrase) {
    return handleError(
      ipc.sendSync("NJSILibraryController.IsValidRecoveryPhrase", phrase)
    );
  }

  static GenerateRecoveryMnemonic() {
    return handleError(
      ipc.sendSync("NJSILibraryController.GenerateRecoveryMnemonic")
    );
  }

  static GenerateGenesisKeys() {
    return handleError(
      ipc.sendSync("NJSILibraryController.GenerateGenesisKeys")
    );
  }

  static ComposeRecoveryPhrase(mnemonic, birthTime) {
    return handleError(
      ipc.sendSync(
        "NJSILibraryController.ComposeRecoveryPhrase",
        mnemonic,
        birthTime
      )
    );
  }

  static TerminateUnityLib() {
    return handleError(ipc.sendSync("NJSILibraryController.TerminateUnityLib"));
  }

  static QRImageFromString(qr_string, width_hint) {
    return handleError(
      ipc.sendSync(
        "NJSILibraryController.QRImageFromString",
        qr_string,
        width_hint
      )
    );
  }

  static GetReceiveAddress() {
    return handleError(ipc.sendSync("NJSILibraryController.GetReceiveAddress"));
  }

  static GetRecoveryPhrase() {
    return handleError(ipc.sendSync("NJSILibraryController.GetRecoveryPhrase"));
  }

  static IsMnemonicWallet() {
    return handleError(ipc.sendSync("NJSILibraryController.IsMnemonicWallet"));
  }

  static IsMnemonicCorrect(phrase) {
    return handleError(
      ipc.sendSync("NJSILibraryController.IsMnemonicCorrect", phrase)
    );
  }

  static GetMnemonicDictionary() {
    return handleError(
      ipc.sendSync("NJSILibraryController.GetMnemonicDictionary")
    );
  }

  static UnlockWallet(password) {
    return handleError(
      ipc.sendSync("NJSILibraryController.UnlockWallet", password)
    );
  }

  static LockWallet() {
    return handleError(ipc.sendSync("NJSILibraryController.LockWallet"));
  }

  static ChangePassword(oldPassword, newPassword) {
    return handleError(
      ipc.sendSync(
        "NJSILibraryController.ChangePassword",
        oldPassword,
        newPassword
      )
    );
  }

  static DoRescan() {
    return handleError(ipc.sendSync("NJSILibraryController.DoRescan"));
  }

  static IsValidRecipient(request) {
    return handleError(
      ipc.sendSync("NJSILibraryController.IsValidRecipient", request)
    );
  }

  static IsValidNativeAddress(address) {
    return handleError(
      ipc.sendSync("NJSILibraryController.IsValidNativeAddress", address)
    );
  }

  static IsValidBitcoinAddress(address) {
    return handleError(
      ipc.sendSync("NJSILibraryController.IsValidBitcoinAddress", address)
    );
  }

  static FeeForRecipient(request) {
    return handleError(
      ipc.sendSync("NJSILibraryController.feeForRecipient", request)
    );
  }

  static PerformPaymentToRecipient(request, substract_fee) {
    return handleError(
      ipc.sendSync(
        "NJSILibraryController.performPaymentToRecipient",
        request,
        substract_fee
      )
    );
  }

  static GetTransaction(txHash) {
    return handleError(
      ipc.sendSync("NJSILibraryController.getTransaction", txHash)
    );
  }

  static ResendTransaction(txHash) {
    return handleError(
      ipc.sendSync("NJSILibraryController.resendTransaction", txHash)
    );
  }

  static GetAddressBookRecords() {
    return handleError(
      ipc.sendSync("NJSILibraryController.getAddressBookRecords")
    );
  }

  static AddAddressBookRecord(address) {
    return handleError(
      ipc.sendSync("NJSILibraryController.addAddressBookRecord", address)
    );
  }

  static DeleteAddressBookRecord(address) {
    return handleError(
      ipc.sendSync("NJSILibraryController.deleteAddressBookRecord", address)
    );
  }

  static ResetUnifiedProgress() {
    return handleError(
      ipc.sendSync("NJSILibraryController.ResetUnifiedProgress")
    );
  }

  static GetLastSPVBlockInfos() {
    return handleError(
      ipc.sendSync("NJSILibraryController.getLastSPVBlockInfos")
    );
  }

  static GetUnifiedProgress() {
    return handleError(
      ipc.sendSync("NJSILibraryController.getUnifiedProgress")
    );
  }

  static GetMonitoringStats() {
    return handleError(
      ipc.sendSync("NJSILibraryController.getMonitoringStats")
    );
  }

  static RegisterMonitorListener(listener) {
    return handleError(
      ipc.sendSync("NJSILibraryController.RegisterMonitorListener", listener)
    );
  }

  static UnregisterMonitorListener(listener) {
    return handleError(
      ipc.sendSync("NJSILibraryController.UnregisterMonitorListener", listener)
    );
  }

  static GetClientInfo() {
    return handleError(ipc.sendSync("NJSILibraryController.getClientInfo"));
  }
}

class WalletController {
  static HaveUnconfirmedFunds() {
    return handleError(
      ipc.sendSync("NJSIWalletController.HaveUnconfirmedFunds")
    );
  }

  static GetBalanceSimple() {
    return handleError(ipc.sendSync("NJSIWalletController.GetBalanceSimple"));
  }

  static GetBalance() {
    return handleError(ipc.sendSync("NJSIWalletController.GetBalance"));
  }

  static AbandonTransaction(txHash) {
    return handleError(
      ipc.sendSync("NJSIWalletController.AbandonTransaction", txHash)
    );
  }

  static GetUUID() {
    return handleError(ipc.sendSync("NJSIWalletController.GetUUID"));
  }
}

class RpcController {
  static Execute(command) {
    return handleError(ipc.sendSync("NJSIRpcController.Execute", command));
  }
  static GetAutocompleteList() {
    return handleError(ipc.sendSync("NJSIRpcController.getAutocompleteList"));
  }
}

class P2pNetworkController {
  static DisableNetwork() {
    return handleError(ipc.sendSync("NJSIP2pNetworkController.disableNetwork"));
  }

  static EnableNetwork() {
    return handleError(ipc.sendSync("NJSIP2pNetworkController.enableNetwork"));
  }

  static GetPeerInfo() {
    return handleError(ipc.sendSync("NJSIP2pNetworkController.getPeerInfo"));
  }
}

class AccountsController {
  static ListAccounts() {
    return handleError(ipc.sendSync("NJSIAccountsController.listAccounts"));
  }

  static SetActiveAccount(accountUUID) {
    return handleError(
      ipc.sendSync("NJSIAccountsController.setActiveAccount", accountUUID)
    );
  }

  static GetActiveAccount() {
    return handleError(ipc.sendSync("NJSIAccountsController.getActiveAccount"));
  }

  static CreateAccount(accountName, accountType) {
    return handleError(
      ipc.sendSync(
        "NJSIAccountsController.createAccount",
        accountName,
        accountType
      )
    );
  }

  static GetAccountName(accountUUID) {
    return handleError(
      ipc.sendSync("NJSIAccountsController.getAccountName", accountUUID)
    );
  }

  static RenameAccount(accountUUID, newAccountName) {
    return handleError(
      ipc.sendSync(
        "NJSIAccountsController.renameAccount",
        accountUUID,
        newAccountName
      )
    );
  }

  static DeleteAccount(accountUUID) {
    return handleError(
      ipc.sendSync("NJSIAccountsController.deleteAccount", accountUUID)
    );
  }

  static PurgeAccount(accountUUID) {
    return handleError(
      ipc.sendSync("NJSIAccountsController.purgeAccount", accountUUID)
    );
  }

  static GetAccountLinkURI(accountUUID) {
    return handleError(
      ipc.sendSync("NJSIAccountsController.getAccountLinkURI", accountUUID)
    );
  }

  static GetWitnessKeyURI(accountUUID) {
    return handleError(
      ipc.sendSync("NJSIAccountsController.getWitnessKeyURI", accountUUID)
    );
  }

  static CreateAccountFromWitnessKeyURI(witnessKeyURI, newAccountName) {
    return handleError(
      ipc.sendSync(
        "NJSIAccountsController.createAccountFromWitnessKeyURI",
        witnessKeyURI,
        newAccountName
      )
    );
  }

  static GetReceiveAddress(accountUUID) {
    return handleError(
      ipc.sendSync("NJSIAccountsController.getReceiveAddress", accountUUID)
    );
  }

  static GetTransactionHistory(accountUUID) {
    return handleError(
      ipc.sendSync("NJSIAccountsController.getTransactionHistory", accountUUID)
    );
  }

  static GetMutationHistory(accountUUID) {
    return handleError(
      ipc.sendSync("NJSIAccountsController.getMutationHistory", accountUUID)
    );
  }

  static GetActiveAccountBalance() {
    return handleError(
      ipc.sendSync("NJSIAccountsController.getActiveAccountBalance")
    );
  }

  static GetAccountBalance(accountUUID) {
    return handleError(
      ipc.sendSync("NJSIAccountsController.getAccountBalance", accountUUID)
    );
  }

  static GetAllAccountBalances() {
    return handleError(
      ipc.sendSync("NJSIAccountsController.getAllAccountBalances")
    );
  }
}

export {
  LibraryController,
  WalletController,
  RpcController,
  P2pNetworkController,
  AccountsController
};
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
