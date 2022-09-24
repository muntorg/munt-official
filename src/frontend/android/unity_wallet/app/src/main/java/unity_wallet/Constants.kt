// Copyright (c) 2018-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

package unity_wallet

object Constants
{
    const val TEST = BuildConfig.TESTNET
    const val RECOMMENDED_CONFIRMATIONS = 3
    const val ACCESS_CODE_ATTEMPTS_ALLOWED = 3
    const val ACCESS_CODE_LENGTH = 6
    const val OLD_WALLET_PROTOBUF_FILENAME = BuildConfig.OLD_WALLET_PROTOBUF_FILENAME
}
