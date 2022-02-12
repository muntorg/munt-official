// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2017-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef CONSENSUS_VALIDATION_H
#define CONSENSUS_VALIDATION_H

#include <string>

//fixme: this constant exists to support gradually factoring out the literal "576" used throughout
// Any literal "576" encountered can be replaced by this at any time (assuming this literal is used as daily block target)
// At any convenient time comment this const and resolve compiler errors the right way, for example using DailyBlocksTarget() where apprapriate
static const int gRefactorDailyBlocksUsage = 576;

static const int gMinimumWitnessAmount = 5000;
static const int gMinimumWitnessWeight = 10000;
static const int gMinimumParticipationAge = 100;    // This forces an attacker to split funds into at least 200 accounts to have a high percentage chance of controlling the network.
static const int gMaximumParticipationAge = 365 * gRefactorDailyBlocksUsage; // Witnesses will essentially be required to download and parse the utxo for this many blocks back from current tip.
                                                    // We try to balance this in such a way that it allows smaller witness accounts but not ones so absurdly small that they force witnesses to unnecessarily keep years of data around.
                                                    // Currently set at - 1 year (365 days * daily blocks).

static const int gStartingWitnessNetworkWeightEstimate  = 260000000;
static const int gMinimumWitnessLockDays                = 30;
static const int gMaximumWitnessLockDays                = 3 * 365;
#define GLOBAL_MAXIMUM_WITNESS_COMPOUND                  "40"
static const int gMaximumWitnessCompoundAmount          = 40;


/** "reject" message codes */
static const unsigned char REJECT_MALFORMED = 0x01;
static const unsigned char REJECT_INVALID = 0x10;
static const unsigned char REJECT_OBSOLETE = 0x11;
static const unsigned char REJECT_DUPLICATE = 0x12;
static const unsigned char REJECT_NONSTANDARD = 0x40;
// static const unsigned char REJECT_DUST = 0x41; // part of BIP 61
static const unsigned char REJECT_INSUFFICIENTFEE = 0x42;
static const unsigned char REJECT_CHECKPOINT = 0x43;

/** Capture information about block/transaction validation */
class CValidationState {
private:
    enum mode_state {
        MODE_VALID,   //!< everything ok
        MODE_INVALID, //!< network rule violation (DoS value may be set)
        MODE_ERROR,   //!< run-time error
    } mode;
    int nDoS;
    std::string strRejectReason;
    unsigned int chRejectCode;
    bool corruptionPossible;
    std::string strDebugMessage;
public:
    CValidationState() : mode(MODE_VALID), nDoS(0), chRejectCode(0), corruptionPossible(false) {}
    bool DoS(int level, bool ret = false,
             unsigned int chRejectCodeIn=0, const std::string &strRejectReasonIn="",
             bool corruptionIn=false,
             const std::string &strDebugMessageIn="") {
        chRejectCode = chRejectCodeIn;
        strRejectReason = strRejectReasonIn;
        corruptionPossible = corruptionIn;
        strDebugMessage = strDebugMessageIn;
        if (mode == MODE_ERROR)
            return ret;
        nDoS += level;
        mode = MODE_INVALID;
        return ret;
    }
    bool Invalid(bool ret = false,
                 unsigned int _chRejectCode=0, const std::string &_strRejectReason="",
                 const std::string &_strDebugMessage="") {
        return DoS(0, ret, _chRejectCode, _strRejectReason, false, _strDebugMessage);
    }
    bool Error(const std::string& strRejectReasonIn) {
        if (mode == MODE_VALID)
            strRejectReason = strRejectReasonIn;
        mode = MODE_ERROR;
        return false;
    }
    bool IsValid() const {
        return mode == MODE_VALID;
    }
    bool IsInvalid() const {
        return mode == MODE_INVALID;
    }
    bool IsError() const {
        return mode == MODE_ERROR;
    }
    bool IsInvalid(int &nDoSOut) const {
        if (IsInvalid()) {
            nDoSOut = nDoS;
            return true;
        }
        return false;
    }
    bool CorruptionPossible() const {
        return corruptionPossible;
    }
    void SetCorruptionPossible() {
        corruptionPossible = true;
    }
    unsigned int GetRejectCode() const { return chRejectCode; }
    std::string GetRejectReason() const { return strRejectReason; }
    std::string GetDebugMessage() const { return strDebugMessage; }
    std::string ToString() const
    {
        if (IsValid())
        {
            return "Valid";
        }

        if (!strDebugMessage.empty()) {
            return strRejectReason + ", " + strDebugMessage;
        }

        return strRejectReason;
    }
};

#endif
