// Copyright (c) 2017-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

#ifndef GENERATION_WITNESS_H
#define GENERATION_WITNESS_H

#include <boost/thread/thread.hpp>

//fixme: (POST-PHASE5) This is non-ideal; we should rather use a signal or something for this.
//! Indicate to the witness thread that it must erase the witness script cache and recalculate it.
extern bool witnessScriptsAreDirty;

extern bool witnessingEnabled;

//! Run the main witnessing thread; On wallets with no witnessing accounts this will just sleep permanently.
void StartPoW2WitnessThread(boost::thread_group& threadGroup);

#endif
