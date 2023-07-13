// Copyright (c) 2019-2022 The Centure developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

#include "witnessrewardtemplate.h"

#include <numeric>

bool CWitnessRewardTemplate::empty() const
{
    return destinations.empty();
}

CAmount CWitnessRewardTemplate::fixedAmountsSum() const {
    return std::accumulate(destinations.begin(), destinations.end(), CAmount(0),
                           [](const CAmount acc, const auto& it){ return acc + it.amount; });
}

double CWitnessRewardTemplate::percentagesSum() const {
    return std::accumulate(destinations.begin(), destinations.end(), 0.0,
                           [](const double acc, const auto& it){ return acc + it.percent; });
}

void CWitnessRewardTemplate::validate(const CAmount witnessBlockSubsidy)
{
    int numRemainder = std::count_if(destinations.begin(), destinations.end(), [](const CWitnessRewardDestination& dest) { return dest.takesRemainder; });
    if (numRemainder > 1)
        throw std::runtime_error("Too many remainder destinations in reward template");

    int numOverflow = std::count_if(destinations.begin(), destinations.end(), [](const CWitnessRewardDestination& dest) { return dest.takesCompoundOverflow; });
    if (numOverflow > 1)
        throw std::runtime_error("Too many compound_overflow destinations in reward template");

    std::for_each(destinations.begin(), destinations.end(), [](const CWitnessRewardDestination& dest) {
        if (dest.percent < 0 || dest.percent > 1.0)
            throw std::runtime_error("Percentage out-of-range [0..100] in reward template");

        if (dest.type == CWitnessRewardDestination::DestType::Compound && dest.takesCompoundOverflow)
            throw std::runtime_error("\"compound_overflow\" not allowed on \"compound\" destination in reward template");
    });

    double totalPercent = percentagesSum();
    if (totalPercent > 1.0)
        throw std::runtime_error("Percentage out-of-range [0..100] in reward template");

    if (totalPercent < 1.0 && numRemainder == 0)
        throw std::runtime_error("Remainder required in reward template");

    if (fixedAmountsSum() > witnessBlockSubsidy)
        throw std::runtime_error("Fixed payouts exceed block reward in reward template");
}
