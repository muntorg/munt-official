// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2018-2022 The Centure developers
// Authored by: Willem de Jonge (willem@isnapp.nl)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

#ifndef NETMESSAGEMAKER_H
#define NETMESSAGEMAKER_H

#include "net.h"
#include "serialize.h"

class CNetMsgMaker
{
public:
    CNetMsgMaker(int nVersionIn, int nExtraFlagsIn = 0) : nVersion(nVersionIn), nExtraFlags(nExtraFlagsIn){}

    template <typename... Args>
    CSerializedNetMsg Make(int nFlags, std::string sCommand, Args&&... args) const
    {
        CSerializedNetMsg msg;
        msg.command = std::move(sCommand);
        CVectorWriter{ SER_NETWORK, nFlags | nVersion | nExtraFlags, msg.data, 0, std::forward<Args>(args)... };
        return msg;
    }

    template <typename... Args>
    CSerializedNetMsg Make(std::string sCommand, Args&&... args) const
    {
        return Make(0, std::move(sCommand), std::forward<Args>(args)...);
    }

private:
    const int nVersion;
    const int nExtraFlags;
};

#endif
