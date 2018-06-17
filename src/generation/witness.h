// Copyright (c) 2017-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_WITNESS_H
#define GULDEN_WITNESS_H

#include <boost/thread/thread.hpp>

//fixme: (2.1) This is non-ideal; we should rather use a signal or something for this.
//! Indicate to the witness thread that it must erase the witness script cache and recalculate it.
extern bool witnessScriptsAreDirty;

//! Run the main witnessing thread; On wallets with no witnessing accounts this will just sleep permanently.
void StartPoW2WitnessThread(boost::thread_group& threadGroup);

#endif
