// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from libunity.djinni

#pragma once

#include <cstdint>
#include <memory>

#ifdef DJINNI_NODEJS
#include "NJSIWalletListener.hpp"
#else
class IWalletListener;
#endif
struct BalanceRecord;

/**
 * Controller to perform functions at a wallet level (e.g. get balance of the entire wallet)
 * For per account functionality see accounts_controller
 */
class IWalletController {
public:
    virtual ~IWalletController() {}

    /** Set listener to be notified of wallet events */
    static void setListener(const std::shared_ptr<IWalletListener> & networklistener);

    /** Check if the wallet has any transactions that are still pending confirmation, to be used to determine if e.g. it is safe to perform a link or whether we should wait. */
    static bool HaveUnconfirmedFunds();

    /** Check current wallet balance, as a single simple number that includes confirmed/unconfirmed/immature funds */
    static int64_t GetBalanceSimple();

    /** Check current wallet balance */
    static BalanceRecord GetBalance();
};
