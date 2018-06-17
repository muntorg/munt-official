// Copyright (c) 2017-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_WITNESS_H
#define GULDEN_WITNESS_H

#include <boost/thread/thread.hpp>

void StartPoW2WitnessThread(boost::thread_group& threadGroup);

#endif
