// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from libunity.djinni

#pragma once

#include <cstdint>
#include <utility>

struct WalletLockStatus final {
    bool locked;
    int64_t lock_timeout;

    WalletLockStatus(bool locked_,
                     int64_t lock_timeout_)
    : locked(std::move(locked_))
    , lock_timeout(std::move(lock_timeout_))
    {}
};
