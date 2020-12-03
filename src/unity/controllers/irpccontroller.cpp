// Copyright (c) 2020 The Novo developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the NOVO software license, see the accompanying
// file COPYING

//Workaround braindamaged 'hack' in libtool.m4 that defines DLL_EXPORT when building a dll via libtool (this in turn imports unwanted symbols from e.g. pthread that breaks static pthread linkage)
#ifdef DLL_EXPORT
#undef DLL_EXPORT
#endif

// Unity specific includes
#include "../unity_impl.h"
#include "i_rpc_controller.hpp"
#include "i_rpc_listener.hpp"
#include "controllers/rpccontroller.h"

void IRpcController::execute(const std::string& rpcCommandLine, const std::shared_ptr<IRpcListener>& resultListener)
{
    std::thread([=]
    {
        RPCController controller;
        const std::function<void(const std::string&, const std::string&)>& errorHandler = [=](const std::string& filteredCommand, const std::string& errorMessage) {resultListener->onError(filteredCommand, errorMessage);};
        const std::function<void(const std::string&)>& filteredCommandHandler = [=](const std::string& filteredCommand) {resultListener->onFilteredCommand(filteredCommand);};
        const std::function<void(const std::string&, const std::string&)>& successHandler = [=](const std::string& filteredCommand, const std::string& result) {resultListener->onSuccess(filteredCommand, result);};
        controller.executeCommandLine(rpcCommandLine, filteredCommandHandler, errorHandler, successHandler);
    }).detach();
}

std::vector<std::string> IRpcController::getAutocompleteList()
{
    RPCController controller;
    return controller.getAutocompleteList();
}
