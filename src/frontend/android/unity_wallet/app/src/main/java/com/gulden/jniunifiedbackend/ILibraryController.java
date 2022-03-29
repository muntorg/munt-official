// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from libunity.djinni

package com.gulden.jniunifiedbackend;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * The library controller is used to Init/Terminate the library, and other similar tasks.
 * It is also home to various generic utility functions that don't (yet) have a place in more specific controllers
 * Specific functionality should go in specific controllers; account related functionality -> accounts_controller, network related functionality -> network_controller and so on
 */
public abstract class ILibraryController {
    /** Interface constants */
    public static final int VERSION = 1;

    /** Get the build information (ie. commit id and status) */
    public static String BuildInfo()
    {
        return CppProxy.BuildInfo();
    }

    /**
     * Start the library
     * extraArgs - any additional commandline arguments as could normally be passed to the daemon binary
     * NB!!! This call blocks until the library is terminated, it is the callers responsibility to place it inside a thread or similar.
     * If you are in an environment where this is not possible (node.js for example use InitUnityLibThreaded instead which places it in a thread on your behalf)
     */
    public static int InitUnityLib(String dataDir, String staticFilterPath, long staticFilterOffset, long staticFilterLength, boolean testnet, boolean spvMode, ILibraryListener signalHandler, String extraArgs)
    {
        return CppProxy.InitUnityLib(dataDir,
                                     staticFilterPath,
                                     staticFilterOffset,
                                     staticFilterLength,
                                     testnet,
                                     spvMode,
                                     signalHandler,
                                     extraArgs);
    }

    /** Threaded implementation of InitUnityLib */
    public static void InitUnityLibThreaded(String dataDir, String staticFilterPath, long staticFilterOffset, long staticFilterLength, boolean testnet, boolean spvMode, ILibraryListener signalHandler, String extraArgs)
    {
        CppProxy.InitUnityLibThreaded(dataDir,
                                      staticFilterPath,
                                      staticFilterOffset,
                                      staticFilterLength,
                                      testnet,
                                      spvMode,
                                      signalHandler,
                                      extraArgs);
    }

    /** Create the wallet - this should only be called after receiving a `notifyInit...` signal from InitUnityLib */
    public static boolean InitWalletFromRecoveryPhrase(String phrase, String password)
    {
        return CppProxy.InitWalletFromRecoveryPhrase(phrase,
                                                     password);
    }

    /** Continue creating wallet that was previously erased using EraseWalletSeedsAndAccounts */
    public static boolean ContinueWalletFromRecoveryPhrase(String phrase, String password)
    {
        return CppProxy.ContinueWalletFromRecoveryPhrase(phrase,
                                                         password);
    }

    /** Create the wallet - this should only be called after receiving a `notifyInit...` signal from InitUnityLib */
    public static boolean InitWalletLinkedFromURI(String linkedUri, String password)
    {
        return CppProxy.InitWalletLinkedFromURI(linkedUri,
                                                password);
    }

    /** Continue creating wallet that was previously erased using EraseWalletSeedsAndAccounts */
    public static boolean ContinueWalletLinkedFromURI(String linkedUri, String password)
    {
        return CppProxy.ContinueWalletLinkedFromURI(linkedUri,
                                                    password);
    }

    /** Create the wallet - this should only be called after receiving a `notifyInit...` signal from InitUnityLib */
    public static boolean InitWalletFromAndroidLegacyProtoWallet(String walletFile, String oldPassword, String newPassword)
    {
        return CppProxy.InitWalletFromAndroidLegacyProtoWallet(walletFile,
                                                               oldPassword,
                                                               newPassword);
    }

    /** Check if a file is a valid legacy proto wallet */
    public static LegacyWalletResult isValidAndroidLegacyProtoWallet(String walletFile, String oldPassword)
    {
        return CppProxy.isValidAndroidLegacyProtoWallet(walletFile,
                                                        oldPassword);
    }

    /** Check link URI for validity */
    public static boolean IsValidLinkURI(String phrase)
    {
        return CppProxy.IsValidLinkURI(phrase);
    }

    /** Replace the existing wallet accounts with a new one from a linked URI - only after first emptying the wallet. */
    public static boolean ReplaceWalletLinkedFromURI(String linkedUri, String password)
    {
        return CppProxy.ReplaceWalletLinkedFromURI(linkedUri,
                                                   password);
    }

    /**
     * Erase the seeds and accounts of a wallet leaving an empty wallet (with things like the address book intact)
     * After calling this it will be necessary to create a new linked account or recovery phrase account again.
     * NB! This will empty a wallet regardless of whether it has funds in it or not and makes no provisions to check for this - it is the callers responsibility to ensure that erasing the wallet is safe to do in this regard.
     */
    public static boolean EraseWalletSeedsAndAccounts()
    {
        return CppProxy.EraseWalletSeedsAndAccounts();
    }

    /**
     * Check recovery phrase for (syntactic) validity
     * Considered valid if the contained mnemonic is valid and the birth-number is either absent or passes Base-10 checksum
     */
    public static boolean IsValidRecoveryPhrase(String phrase)
    {
        return CppProxy.IsValidRecoveryPhrase(phrase);
    }

    /** Generate a new recovery mnemonic */
    public static MnemonicRecord GenerateRecoveryMnemonic()
    {
        return CppProxy.GenerateRecoveryMnemonic();
    }

    public static String GenerateGenesisKeys()
    {
        return CppProxy.GenerateGenesisKeys();
    }

    /** Compute recovery phrase with birth number */
    public static MnemonicRecord ComposeRecoveryPhrase(String mnemonic, long birthTime)
    {
        return CppProxy.ComposeRecoveryPhrase(mnemonic,
                                              birthTime);
    }

    /** Stop the library */
    public static void TerminateUnityLib()
    {
        CppProxy.TerminateUnityLib();
    }

    /** Generate a QR code for a string, QR code will be as close to widthHint as possible when applying simple scaling. */
    public static QrCodeRecord QRImageFromString(String qrString, int widthHint)
    {
        return CppProxy.QRImageFromString(qrString,
                                          widthHint);
    }

    /** Get a receive address for the active account */
    public static String GetReceiveAddress()
    {
        return CppProxy.GetReceiveAddress();
    }

    /** Get the recovery phrase for the wallet */
    public static MnemonicRecord GetRecoveryPhrase()
    {
        return CppProxy.GetRecoveryPhrase();
    }

    /** Check if the wallet is using a mnemonic seed ie. recovery phrase (else it is a linked wallet) */
    public static boolean IsMnemonicWallet()
    {
        return CppProxy.IsMnemonicWallet();
    }

    /** Check if the phrase mnemonic is a correct one for the wallet (phrase can be with or without birth time) */
    public static boolean IsMnemonicCorrect(String phrase)
    {
        return CppProxy.IsMnemonicCorrect(phrase);
    }

    /**
     * Get the 'dictionary' of valid words that a recovery phrase can be composed of
     * NB! Not all combinations of these words are valid
     * Do not use this to generate/compose your own phrases - always use 'GenerateRecoveryMnemonic' for this
     * This function should only be used for input validation/auto-completion
     */
    public static ArrayList<String> GetMnemonicDictionary()
    {
        return CppProxy.GetMnemonicDictionary();
    }

    /** Unlock wallet; wallet will automatically relock after "timeoutInSeconds" */
    public static boolean UnlockWallet(String password, long timeoutInSeconds)
    {
        return CppProxy.UnlockWallet(password,
                                     timeoutInSeconds);
    }

    /** Forcefully lock wallet again */
    public static boolean LockWallet()
    {
        return CppProxy.LockWallet();
    }

    public static WalletLockStatus GetWalletLockStatus()
    {
        return CppProxy.GetWalletLockStatus();
    }

    /** Change the wallet password */
    public static boolean ChangePassword(String oldPassword, String newPassword)
    {
        return CppProxy.ChangePassword(oldPassword,
                                       newPassword);
    }

    /** Rescan blockchain for wallet transactions */
    public static void DoRescan()
    {
        CppProxy.DoRescan();
    }

    /** Check if text/address is something we are capable of sending money too */
    public static UriRecipient IsValidRecipient(UriRecord request)
    {
        return CppProxy.IsValidRecipient(request);
    }

    /** Check if text/address is a native (to our blockchain) address */
    public static boolean IsValidNativeAddress(String address)
    {
        return CppProxy.IsValidNativeAddress(address);
    }

    /** Check if text/address is a valid bitcoin address */
    public static boolean IsValidBitcoinAddress(String address)
    {
        return CppProxy.IsValidBitcoinAddress(address);
    }

    /** Compute the fee required to send amount to given recipient */
    public static long feeForRecipient(UriRecipient request)
    {
        return CppProxy.feeForRecipient(request);
    }

    /** Attempt to pay a recipient, will throw on failure with description */
    public static PaymentResultStatus performPaymentToRecipient(UriRecipient request, boolean substractFee)
    {
        return CppProxy.performPaymentToRecipient(request,
                                                  substractFee);
    }

    /**
     * Get the wallet transaction for the hash
     * Will throw if not found
     */
    public static TransactionRecord getTransaction(String txHash)
    {
        return CppProxy.getTransaction(txHash);
    }

    /** resubmit a transaction to the network, returns the raw hex of the transaction as a string or empty on fail */
    public static String resendTransaction(String txHash)
    {
        return CppProxy.resendTransaction(txHash);
    }

    /** Get list of all address book entries */
    public static ArrayList<AddressRecord> getAddressBookRecords()
    {
        return CppProxy.getAddressBookRecords();
    }

    /** Add a record to the address book */
    public static void addAddressBookRecord(AddressRecord address)
    {
        CppProxy.addAddressBookRecord(address);
    }

    /** Delete a record from the address book */
    public static void deleteAddressBookRecord(AddressRecord address)
    {
        CppProxy.deleteAddressBookRecord(address);
    }

    /** Interim persist and prune of state. Use at key moments like app backgrounding. */
    public static void PersistAndPruneForSPV()
    {
        CppProxy.PersistAndPruneForSPV();
    }

    /**
     * Reset progress notification. In cases where there has been no progress for a long time, but the process
     * is still running the progress can be reset and will represent work to be done from this reset onwards.
     * For example when the process is in the background on iOS for a long long time (but has not been terminated
     * by the OS) this might make more sense then to continue the progress from where it was a day or more ago.
     */
    public static void ResetUnifiedProgress()
    {
        CppProxy.ResetUnifiedProgress();
    }

    /** Get info of last blocks (at most 32) in SPV chain */
    public static ArrayList<BlockInfoRecord> getLastSPVBlockInfos()
    {
        return CppProxy.getLastSPVBlockInfos();
    }

    public static float getUnifiedProgress()
    {
        return CppProxy.getUnifiedProgress();
    }

    public static MonitorRecord getMonitoringStats()
    {
        return CppProxy.getMonitoringStats();
    }

    public static void RegisterMonitorListener(MonitorListener listener)
    {
        CppProxy.RegisterMonitorListener(listener);
    }

    public static void UnregisterMonitorListener(MonitorListener listener)
    {
        CppProxy.UnregisterMonitorListener(listener);
    }

    public static HashMap<String, String> getClientInfo()
    {
        return CppProxy.getClientInfo();
    }

    /**
     * Get list of wallet mutations
     *NB! This is SPV specific, non SPV wallets should use account specific getMutationHistory on an accounts controller instead
     */
    public static ArrayList<MutationRecord> getMutationHistory()
    {
        return CppProxy.getMutationHistory();
    }

    /**
     * Get list of all transactions wallet has been involved in
     *NB! This is SPV specific, non SPV wallets should use account specific getTransactionHistory on an accounts controller instead
     */
    public static ArrayList<TransactionRecord> getTransactionHistory()
    {
        return CppProxy.getTransactionHistory();
    }

    /**
     * Check if the wallet has any transactions that are still pending confirmation, to be used to determine if e.g. it is safe to perform a link or whether we should wait.
     *NB! This is SPV specific, non SPV wallets should use HaveUnconfirmedFunds on wallet controller instead
     */
    public static boolean HaveUnconfirmedFunds()
    {
        return CppProxy.HaveUnconfirmedFunds();
    }

    /**
     * Check current wallet balance (including unconfirmed funds)
     *NB! This is SPV specific, non SPV wallets should use GetBalance on wallet controller instead
     */
    public static long GetBalance()
    {
        return CppProxy.GetBalance();
    }

    private static final class CppProxy extends ILibraryController
    {
        private final long nativeRef;
        private final AtomicBoolean destroyed = new AtomicBoolean(false);

        private CppProxy(long nativeRef)
        {
            if (nativeRef == 0) throw new RuntimeException("nativeRef is zero");
            this.nativeRef = nativeRef;
        }

        private native void nativeDestroy(long nativeRef);
        public void _djinni_private_destroy()
        {
            boolean destroyed = this.destroyed.getAndSet(true);
            if (!destroyed) nativeDestroy(this.nativeRef);
        }
        protected void finalize() throws java.lang.Throwable
        {
            _djinni_private_destroy();
            super.finalize();
        }

        public static native String BuildInfo();

        public static native int InitUnityLib(String dataDir, String staticFilterPath, long staticFilterOffset, long staticFilterLength, boolean testnet, boolean spvMode, ILibraryListener signalHandler, String extraArgs);

        public static native void InitUnityLibThreaded(String dataDir, String staticFilterPath, long staticFilterOffset, long staticFilterLength, boolean testnet, boolean spvMode, ILibraryListener signalHandler, String extraArgs);

        public static native boolean InitWalletFromRecoveryPhrase(String phrase, String password);

        public static native boolean ContinueWalletFromRecoveryPhrase(String phrase, String password);

        public static native boolean InitWalletLinkedFromURI(String linkedUri, String password);

        public static native boolean ContinueWalletLinkedFromURI(String linkedUri, String password);

        public static native boolean InitWalletFromAndroidLegacyProtoWallet(String walletFile, String oldPassword, String newPassword);

        public static native LegacyWalletResult isValidAndroidLegacyProtoWallet(String walletFile, String oldPassword);

        public static native boolean IsValidLinkURI(String phrase);

        public static native boolean ReplaceWalletLinkedFromURI(String linkedUri, String password);

        public static native boolean EraseWalletSeedsAndAccounts();

        public static native boolean IsValidRecoveryPhrase(String phrase);

        public static native MnemonicRecord GenerateRecoveryMnemonic();

        public static native String GenerateGenesisKeys();

        public static native MnemonicRecord ComposeRecoveryPhrase(String mnemonic, long birthTime);

        public static native void TerminateUnityLib();

        public static native QrCodeRecord QRImageFromString(String qrString, int widthHint);

        public static native String GetReceiveAddress();

        public static native MnemonicRecord GetRecoveryPhrase();

        public static native boolean IsMnemonicWallet();

        public static native boolean IsMnemonicCorrect(String phrase);

        public static native ArrayList<String> GetMnemonicDictionary();

        public static native boolean UnlockWallet(String password, long timeoutInSeconds);

        public static native boolean LockWallet();

        public static native WalletLockStatus GetWalletLockStatus();

        public static native boolean ChangePassword(String oldPassword, String newPassword);

        public static native void DoRescan();

        public static native UriRecipient IsValidRecipient(UriRecord request);

        public static native boolean IsValidNativeAddress(String address);

        public static native boolean IsValidBitcoinAddress(String address);

        public static native long feeForRecipient(UriRecipient request);

        public static native PaymentResultStatus performPaymentToRecipient(UriRecipient request, boolean substractFee);

        public static native TransactionRecord getTransaction(String txHash);

        public static native String resendTransaction(String txHash);

        public static native ArrayList<AddressRecord> getAddressBookRecords();

        public static native void addAddressBookRecord(AddressRecord address);

        public static native void deleteAddressBookRecord(AddressRecord address);

        public static native void PersistAndPruneForSPV();

        public static native void ResetUnifiedProgress();

        public static native ArrayList<BlockInfoRecord> getLastSPVBlockInfos();

        public static native float getUnifiedProgress();

        public static native MonitorRecord getMonitoringStats();

        public static native void RegisterMonitorListener(MonitorListener listener);

        public static native void UnregisterMonitorListener(MonitorListener listener);

        public static native HashMap<String, String> getClientInfo();

        public static native ArrayList<MutationRecord> getMutationHistory();

        public static native ArrayList<TransactionRecord> getTransactionHistory();

        public static native boolean HaveUnconfirmedFunds();

        public static native long GetBalance();
    }
}
