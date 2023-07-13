// Copyright (c) 2019-2022 The Centure developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

#ifndef WITNESS_REWARD_TEMPLATE_H
#define WITNESS_REWARD_TEMPLATE_H

#include "base58.h"
#include "amount.h"
#include <vector>

struct CWitnessRewardDestination {
    enum class DestType: uint16_t {
        Compound,
        Account,
        Address
    };

    DestType type;
    CNativeAddress address;
    CAmount amount;
    double percent;
    bool takesRemainder;
    bool takesCompoundOverflow;

    CWitnessRewardDestination(): type(DestType::Account), amount(0), percent(0), takesRemainder(false), takesCompoundOverflow(false) {}
    CWitnessRewardDestination(const DestType _type, const CNativeAddress& _address, const CAmount _amount, const double _percent, const bool _takesRemainder, const bool _takesCompoundOverflow)
        : type(_type), address(_address), amount(_amount), percent(_percent), takesRemainder(_takesRemainder), takesCompoundOverflow(_takesCompoundOverflow) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(type);
        READWRITE(address);
        READWRITE(amount);
        READWRITE(percent);
        READWRITE(takesRemainder);
        READWRITE(takesCompoundOverflow);
    }
};

class CWitnessRewardTemplate {
public:
    bool empty() const;
    std::vector<CWitnessRewardDestination> destinations;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITECOMPACTSIZEVECTOR(destinations);
    }

    CAmount fixedAmountsSum() const;
    double percentagesSum() const;

    //! Basic sanity checks on the template (like total % < 100 etc), throws if there are issues
    void validate(const CAmount witnessBlockSubsidy);
};

#endif
