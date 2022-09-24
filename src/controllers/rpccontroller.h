// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2017-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING
//
// File contains modifications by: The Novo developers
// All modifications:
// Copyright (c) 2020 The Novo developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the NOVO software license, see the accompanying
// file COPYING

#ifndef CONTROLLERS_RPC_CONTROLLER
#define CONTROLLERS_RPC_CONTROLLER

#include <univalue/include/univalue.h>
#include <tinyformat.h>
#include <rpc/server.h>
#include <rpc/client.h>

class RPCController
{
public:
    void executeCommandLine(const std::string& sCommandLine, const std::function<void(const std::string&)>& filteredCommandHandler, const std::function<void(const std::string&, const std::string&)>& errorHandler, const std::function<void(const std::string&, const std::string&)>& successHandler);
    std::vector<std::string> getAutocompleteList();
private:
    bool parseCommandLine(std::string& strResult, const std::string& strCommand, const bool fExecute, std::string* const pstrFilteredOut);
};

#endif
