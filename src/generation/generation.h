// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef GULDEN_GENERATION_H
#define GULDEN_GENERATION_H

#include "sync.h" //CCriticalSection

//! Prevent both mining and witnessing from trying to process a block at the same time.
extern CCriticalSection processBlockCS;

#endif
