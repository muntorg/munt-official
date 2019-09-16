// Copyright (c) 2019 The Gulden developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "witnessrewardtemplate.h"

#include <numeric>

bool CWitnessRewardTemplate::empty() const
{
    return destinations.empty();
}
